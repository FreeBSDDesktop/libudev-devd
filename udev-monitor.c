/*
 * Copyright (c) 2015 Vladimir Kondratyev <wulf@cicgroup.ru>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "libudev.h"
#include "udev.h"
#include "udev-device.h"
#include "udev-utils.h"
#include "udev-filter.h"
#include "udev-utils.h"
#include "utils.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/queue.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	DEVD_SOCK_PATH		"/var/run/devd.pipe"
#define	DEVD_RECONNECT_INTERVAL	1000	/* reconnect after 1 second */

#define	DEVD_EVENT_ATTACH	'+'
#define	DEVD_EVENT_DETACH	'-'
#define	DEVD_EVENT_NOTICE	'!'
#define	DEVD_EVENT_UNKNOWN	'?'

STAILQ_HEAD(udev_monitor_queue_head, udev_monitor_queue_entry);
struct udev_monitor_queue_entry {
	struct udev_device *ud;
	STAILQ_ENTRY(udev_monitor_queue_entry) next;
};

struct udev_monitor {
	_Atomic(int) refcount;
	int fds[2];
	int kq;
	struct udev_filter_head filters;
	struct udev *udev;
	struct udev_monitor_queue_head queue;
	pthread_mutex_t mtx;
	pthread_t thread;
};

LIBUDEV_EXPORT struct udev_device *
udev_monitor_receive_device(struct udev_monitor *um)
{
	struct udev_monitor_queue_entry *umqe;
	struct udev_device *ud;
	char buf[1];

	TRC("(%p)", um);
	if (read(um->fds[0], buf, 1) < 0)
		return (NULL);

	if (STAILQ_EMPTY(&um->queue))
		return (NULL);

	pthread_mutex_lock(&um->mtx);
	umqe = STAILQ_FIRST(&um->queue);
	STAILQ_REMOVE_HEAD(&um->queue, next);
	pthread_mutex_unlock(&um->mtx);
	ud = umqe->ud;
	free(umqe);

	return (ud);
}

static int
udev_monitor_send_device(struct udev_monitor *um, const char *syspath,
    int action)
{
	struct udev_monitor_queue_entry *umqe;

	umqe = calloc(1, sizeof(struct udev_monitor_queue_entry));
	if (umqe == NULL)
		return (-1);

	umqe->ud = udev_device_new_common(um->udev, syspath, action);
	if (umqe->ud == NULL) {
		free(umqe);
		return (-1);
	}

	pthread_mutex_lock(&um->mtx);
	STAILQ_INSERT_TAIL(&um->queue, umqe, next);
	pthread_mutex_unlock(&um->mtx);

	if (write(um->fds[1], "*", 1) != 1) {
		pthread_mutex_lock(&um->mtx);
		STAILQ_REMOVE(&um->queue, umqe, udev_monitor_queue_entry, next);
		pthread_mutex_unlock(&um->mtx);
		udev_device_unref(umqe->ud);
		free(umqe);
		return (-1);
	}

	return (0);
}

static int
parse_devd_message(char *msg, char *syspath, size_t syspathlen)
{
	char devpath[DEV_PATH_MAX] = DEV_PATH_ROOT "/";
	const char *type, *dev_name;
	size_t type_len, dev_len, root_len;
	int action;

	root_len = strlen(devpath);
	action = UD_ACTION_NONE;

	switch (msg[0]) {
#ifdef HAVE_DEVINFO_H
	case DEVD_EVENT_ATTACH:
		action = UD_ACTION_ADD;
		/* FALLTHROUGH */
	case DEVD_EVENT_DETACH:
		if (action == UD_ACTION_NONE)
			action = UD_ACTION_REMOVE;
		*(strchrnul(msg + 1, ' ')) = '\0';
		strlcpy(syspath, msg + 1, syspathlen);
		break;
#endif /* HAVE_DEVINFO_H */
	case DEVD_EVENT_NOTICE:
		if (!match_kern_prop_value(msg + 1, "system", "DEVFS"))
			break;
		if (!match_kern_prop_value(msg + 1, "subsystem", "CDEV"))
			break;
		type = get_kern_prop_value(msg + 1, "type", &type_len);
		dev_name = get_kern_prop_value(msg + 1, "cdev", &dev_len);
		if (type == NULL ||
		    dev_name == NULL ||
		    dev_len > (sizeof(devpath) - root_len - 1))
			break;
		if (type_len == 6 &&
		    strncmp(type, "CREATE", type_len) == 0)
			action = UD_ACTION_ADD;
		else if (type_len == 7 &&
		    strncmp(type, "DESTROY", type_len) == 0)
			action = UD_ACTION_REMOVE;
		else
			break;
		memcpy(devpath + root_len, dev_name, dev_len);
		devpath[dev_len + root_len] = 0;
		strlcpy(syspath, get_syspath_by_devpath(devpath), syspathlen);
		break;
	case DEVD_EVENT_UNKNOWN:
	default:
		break;
	}

	return (action);
}

/* Opens devd socket and set read kevent on success or timer kevent on failure */
static int
devd_connect(int kq)
{
	int devd_fd;
	struct kevent ke;

	devd_fd = socket_connect(DEVD_SOCK_PATH);

	if (devd_fd >= 0) {
		EV_SET(&ke, devd_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
		if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0) {
			close(devd_fd);
			devd_fd = -1;
		}
	}

	/* Set respawn timer */
	if (devd_fd < 0) {
		EV_SET(&ke, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT,
		    0, DEVD_RECONNECT_INTERVAL, 0);
		if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0)
			devd_fd = -1;
	}

	return (devd_fd);
}

static void *
udev_monitor_thread(void *args)
{
	struct udev_monitor *um = args;
	char ev[1024], syspath[DEV_PATH_MAX];
	int devd_fd = -1, ret, action;
	struct kevent ke;
	sigset_t set;

	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	for (;;) {
		if (devd_fd < 0)
			devd_fd = devd_connect(um->kq);

		ret = kevent(um->kq, NULL, 0, &ke, 1, NULL);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 1)
			break;

		/* edev_monitor is finishing */
		if (ke.filter == EVFILT_USER)
			break;

		/* connection respawn timer expired */
		if (ke.filter == EVFILT_TIMER) {
			continue;
		}

		/* XXX: assert() should be placed here */
		if (ke.filter != EVFILT_READ)
			continue;

		if (ke.flags & EV_EOF ||
		    socket_readline(devd_fd, ev, sizeof(ev)) < 0) {
			close(devd_fd);
			devd_fd = -1;
			continue;
		}

		action = parse_devd_message(ev, syspath, sizeof(syspath));

		if (action != UD_ACTION_NONE) {
			if (udev_filter_match(&um->filters, syspath))
				udev_monitor_send_device(um, syspath, action);
		}
	}

	if (devd_fd >= 0)
		close(devd_fd);

	return (NULL);
}

LIBUDEV_EXPORT struct udev_monitor *
udev_monitor_new_from_netlink(struct udev *udev, const char *name)
{
	struct udev_monitor *um;
	
	TRC("(%p, %s)", udev, name);
	um = calloc(1, sizeof(struct udev_monitor));
	if (!um)
		return (NULL);

	if (pipe2(um->fds, O_CLOEXEC) == -1) {
		ERR("pipe2 failed");
		free(um);
		return (NULL);
	}

	um->udev = udev;
	_udev_ref(udev);
	um->kq = -1;
	atomic_init(&um->refcount, 1);
	udev_filter_init(&um->filters);
	STAILQ_INIT(&um->queue);
	pthread_mutex_init(&um->mtx, NULL);

	return (um);
}

LIBUDEV_EXPORT int
udev_monitor_filter_add_match_subsystem_devtype(struct udev_monitor *um,
    const char *subsystem, const char *devtype)
{

	TRC("(%p, %s, %s)", um, subsystem, devtype);
	return (udev_filter_add(&um->filters, UDEV_FILTER_TYPE_SUBSYSTEM, 0,
	    subsystem, NULL));
}

LIBUDEV_EXPORT int
udev_monitor_enable_receiving(struct udev_monitor *um)
{

	TRC("(%p)", um);
	struct kevent ev;

	um->kq = kqueue();
	if (um->kq < 0)
		goto error;

	EV_SET(&ev, 1, EVFILT_USER, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, 0);
	if (kevent(um->kq, &ev, 1, NULL, 0, NULL) < 0)
		goto error;

	if (pthread_create(&um->thread, NULL, udev_monitor_thread, um) != 0) {
		ERR("thread_create failed");
		goto error;
	}

	return (0);
error:
	if (um->kq >= 0) {
		close (um->kq);
		um->kq = -1;
	}
	return (-1);
}

LIBUDEV_EXPORT int
udev_monitor_get_fd(struct udev_monitor *um)
{

	/* TRC("(%p)", um); */
	return (um->fds[0]);
}

LIBUDEV_EXPORT struct udev_monitor *
udev_monitor_ref(struct udev_monitor *um)
{

	TRC("(%p) refcount=%d", um, um->refcount);
	atomic_fetch_add(&um->refcount, 1);
	return (um);
}

static void
udev_monitor_queue_drop(struct udev_monitor_queue_head *umqh)
{
	struct udev_monitor_queue_entry *umqe;

	while (!STAILQ_EMPTY(umqh)) {
		umqe = STAILQ_FIRST(umqh);
		STAILQ_REMOVE_HEAD(umqh, next);
		udev_device_unref(umqe->ud);
		free(umqe);
	}
}

LIBUDEV_EXPORT void
udev_monitor_unref(struct udev_monitor *um)
{
	struct kevent ev;

	TRC("(%p) refcount=%d", um, um->refcount);
	if (atomic_fetch_sub(&um->refcount, 1) == 1) {
		EV_SET(&ev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, 0);
		kevent(um->kq, &ev, 1, NULL, 0, NULL);
		pthread_join(um->thread, NULL);
		close(um->kq);

		close(um->fds[0]);
		close(um->fds[1]);
		udev_filter_free(&um->filters);
		udev_monitor_queue_drop(&um->queue);
		pthread_mutex_destroy(&um->mtx);
		_udev_unref(um->udev);
		free(um);
	}
}

LIBUDEV_EXPORT
struct udev *udev_monitor_get_udev(struct udev_monitor *um)
{

	TRC();
	return (um->udev);
}
