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
#include "udev-utils.h"
#include "utils.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

struct udev {
	_Atomic(int) refcount;
	void *userdata;
};

LIBUDEV_EXPORT struct udev *
udev_new(void)
{
	struct udev *udev;

	TRC();
	udev = calloc(1, sizeof(struct udev));
	if (udev) {
		atomic_init(&udev->refcount, 1);
		udev->userdata = NULL;
	}

	return (udev);
}

struct udev *
_udev_ref(struct udev *udev)
{

	atomic_fetch_add(&udev->refcount, 1);
	return udev;
}

LIBUDEV_EXPORT struct udev *
udev_ref(struct udev *udev)
{

	TRC("(%p) refcount=%d", udev, udev->refcount);
	return (_udev_ref(udev));
}

void
_udev_unref(struct udev *udev)
{

	if (atomic_fetch_sub(&udev->refcount, 1) == 1)
		free(udev);
}

LIBUDEV_EXPORT void
udev_unref(struct udev *udev)
{

	TRC("(%p) refcount=%d", udev, udev->refcount);
	_udev_unref(udev);
}

LIBUDEV_EXPORT const char *
udev_get_dev_path(struct udev *udev __unused)
{

	TRC();
	return (DEV_PATH_ROOT);
}

LIBUDEV_EXPORT void *
udev_get_userdata(struct udev *udev)
{

	TRC();
	return (udev->userdata);
}

LIBUDEV_EXPORT void
udev_set_userdata(struct udev *udev, void *userdata)
{

	TRC();
	udev->userdata = userdata;
}
