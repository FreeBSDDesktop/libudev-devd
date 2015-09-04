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
#include "udev-filter.h"
#include "udev-list.h"
#include "udev-utils.h"
#include "utils.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct udev_device {
	_Atomic(int) refcount;
	uint32_t flags;
	struct udev_list prop_list;
	struct udev *udev;
	struct udev_device *parent;
	char syspath[];
};

LIBUDEV_EXPORT struct udev_device *
udev_device_new_from_syspath(struct udev *udev, const char *syspath)
{

	TRC("(%s)", syspath);
	return (udev_device_new_common(udev, syspath, UDF_ACTION_NONE));
}

LIBUDEV_EXPORT struct udev_device *
udev_device_new_from_devnum(struct udev *udev, char type, dev_t devnum)
{
	char devpath[DEV_PATH_MAX] = DEV_PATH_ROOT "/";
	const char *syspath;
	size_t dev_len;
	struct stat st;

	dev_len = strlen(devpath);
	devname_r(devnum, S_IFCHR, devpath + dev_len, sizeof(devpath) - dev_len);

	/* Recheck path as devname_r returns zero-terminated garbage on error */
	if (stat(devpath, &st) != 0 || st.st_rdev != devnum) {
		TRC("(%d) -> failed", (int)devnum);
		return NULL;
	}

	TRC("(%d) -> %s", (int)devnum, devpath);
	syspath = get_syspath_by_devpath(devpath);
	return (udev_device_new_common(udev, syspath, UDF_ACTION_NONE));
}

LIBUDEV_EXPORT char const *
udev_device_get_devnode(struct udev_device *ud)
{
	const char *devpath;

	TRC("(%p) %s", ud, ud->syspath);
	devpath = get_devpath_by_syspath(ud->syspath);
	return (devpath);
}

struct udev_list *
udev_device_get_properties_list(struct udev_device *ud)
{

	return (&ud->prop_list);
}

LIBUDEV_EXPORT struct udev_list_entry *
udev_device_get_properties_list_entry(struct udev_device *ud)
{

	TRC("(%p(%s))", ud, ud->syspath);
	return (udev_list_entry_get_first(udev_device_get_properties_list(ud)));
}

LIBUDEV_EXPORT char const *
udev_device_get_property_value(struct udev_device *ud, char const *property)
{
	char const *key, *value;
	struct udev_list_entry *entry;

	udev_list_entry_foreach(entry, udev_list_entry_get_first(&ud->prop_list)) {
		key = _udev_list_entry_get_name(entry);
		if (!key)
			continue;
		if (strcmp(key, property) == 0) {
			value = _udev_list_entry_get_value(entry);
			TRC("(%p(%s), %s) %s", ud, ud->syspath, property, value);
			return (value);
		}
	}
	TRC("(%p(%s), %s) NULL", ud, ud->syspath, property);
	return (NULL);
}

LIBUDEV_EXPORT char const *
udev_device_get_sysattr_value(struct udev_device *ud, const char *sysattr)
{

	TRC("(%p(%s), %s)", ud, ud->syspath, sysattr);
	return (NULL);
}

LIBUDEV_EXPORT struct udev *
udev_device_get_udev(struct udev_device *ud)
{

	TRC("(%p(%s))", ud, ud->syspath);
	return (ud->udev);
}

struct udev_device *
udev_device_new_common(struct udev *udev, const char *syspath, uint32_t flags)
{
	struct udev_device *ud;

	ud = calloc
	    (1, offsetof(struct udev_device, syspath) + strlen(syspath) + 1);
	if (ud == NULL)
		return (NULL);

	ud->udev = udev;
	ud->flags = flags;
	if (!(ud->flags & ~UDF_IS_PARENT))
		_udev_ref(udev);
	ud->parent = NULL;
	atomic_init(&ud->refcount, 1);
	strcpy(ud->syspath, syspath);
	udev_list_init(&ud->prop_list);
	if ((flags & UDF_ACTION_MASK) != UDF_ACTION_REMOVE)
		invoke_create_handler(ud);

	return (ud);
}

LIBUDEV_EXPORT const char *
udev_device_get_syspath(struct udev_device *ud)
{

	TRC("(%p) %s", ud, ud->syspath);
	return (ud->syspath);
}

LIBUDEV_EXPORT const char *
udev_device_get_sysname(struct udev_device *ud)
{
	const char *sysname;

	sysname = get_sysname_by_syspath(ud->syspath);
	TRC("(%p(%s)) %s", ud, ud->syspath, sysname);
	return (sysname);
}

LIBUDEV_EXPORT const char *
udev_device_get_subsystem(struct udev_device *ud)
{
	const char *subsystem;

	subsystem = get_subsystem_by_syspath(udev_device_get_syspath(ud));
	TRC("(%p(%s)) %s", ud, ud->syspath, subsystem);
	return (subsystem);
}

LIBUDEV_EXPORT struct udev_device *
udev_device_ref(struct udev_device *ud)
{
	TRC("(%p/%s) %d", ud, ud->syspath, ud->refcount);

	atomic_fetch_add(&ud->refcount, 1);
	return (ud);
}

static void
udev_device_free(struct udev_device *ud)
{

	udev_list_free(&ud->prop_list);
	if (!(ud->flags & ~UDF_IS_PARENT))
		_udev_unref(ud->udev);
	if (ud->parent != NULL)
		udev_device_free(ud->parent);
	free(ud);
}

LIBUDEV_EXPORT void
udev_device_unref(struct udev_device *ud)
{

	TRC("(%p/%s) %d", ud, ud->syspath, ud->refcount);
	if (atomic_fetch_sub(&ud->refcount, 1) == 1)
		udev_device_free(ud);
}

LIBUDEV_EXPORT struct udev_device *
udev_device_get_parent(struct udev_device *ud)
{

	TRC("(%p/%s) %p", ud, ud->syspath, ud->parent);
	return (ud->parent);
}

void
udev_device_set_parent(struct udev_device *ud, struct udev_device *parent)
{

	ud->parent = parent;
}

LIBUDEV_EXPORT int
udev_device_get_is_initialized(struct udev_device *ud)
{

	TRC("(%p/%s)", ud, ud->syspath);
	return (1);
}

LIBUDEV_EXPORT const char *
udev_device_get_action(struct udev_device *ud)
{
	const char *action;

	switch(ud->flags & UDF_ACTION_MASK) {
	case UDF_ACTION_NONE:
		action = "none";
		break;
	case UDF_ACTION_ADD:
		action = "add";
		break;
	case UDF_ACTION_REMOVE:
		action = "remove";
		break;
	default:
		action = "unknown";
	}
	TRC("(%p/%s) %s", ud, ud->syspath, action);
	return (action);
}

LIBUDEV_EXPORT dev_t
udev_device_get_devnum(struct udev_device *ud)
{
	const char *devpath;
	struct stat st;

	TRC("(%p) %s", ud, ud->syspath);
	devpath = get_devpath_by_syspath(ud->syspath);
	if (devpath == NULL ||
	    stat(devpath, &st) < 0 ||
	    !S_ISCHR(st.st_mode))
		return (makedev(0, 0));

	return (st.st_rdev);
}
