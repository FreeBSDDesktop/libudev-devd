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
#include "udev-utils.h"
#include "udev-filter.h"

#include <sys/types.h>
#include <sys/queue.h>

#include <fnmatch.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct udev_filter_entry {
	int type;
	int neg;
	STAILQ_ENTRY(udev_filter_entry) next;
	char expr[];
};

void
udev_filter_init(struct udev_filter_head *ufh)
{

	STAILQ_INIT(ufh);
}

int
udev_filter_add(struct udev_filter_head *ufh, int type, int neg,
    const char *expr)
{
	struct udev_filter_entry *ufe;

	ufe = calloc
	    (1, offsetof(struct udev_filter_entry, expr) + strlen(expr) + 1);
	if (ufe == NULL)
		return (-1);

	ufe->type = type;
	ufe->neg = neg;
	strcpy(ufe->expr, expr);
	STAILQ_INSERT_TAIL(ufh, ufe, next);
	return (0);
}

void
udev_filter_free(struct udev_filter_head *ufh)
{
	struct udev_filter_entry *ufe1, *ufe2;

	ufe1 = STAILQ_FIRST(ufh);
	while (ufe1 != NULL) {
		ufe2 = STAILQ_NEXT(ufe1, next);
		free(ufe1);
		ufe1 = ufe2;
	}
	STAILQ_INIT(ufh);
}

int
udev_filter_match(struct udev_filter_head *ufh, const char *syspath)
{
	struct udev_filter_entry *ufe;
	const char *subsystem, *sysname;
	int ret;

	subsystem = get_subsystem_by_syspath(syspath);
	if (strcmp(subsystem, UNKNOWN_SUBSYSTEM) == 0)
		return (0);

	sysname = get_sysname_by_syspath(syspath);
	ret = 0;

	STAILQ_FOREACH(ufe, ufh, next) {
		if (ufe->type == UDEV_FILTER_TYPE_SUBSYSTEM &&
		    ufe->neg == 0 &&
		    fnmatch(ufe->expr, subsystem, 0) == 0) {
			ret = 1;
			break;
		}
		if (ufe->type == UDEV_FILTER_TYPE_SYSNAME &&
		    ufe->neg == 0 &&
		    fnmatch(ufe->expr, sysname, 0) == 0) {
			ret = 1;
			break;
		}
	}

	STAILQ_FOREACH(ufe, ufh, next) {
		if (ufe->type == UDEV_FILTER_TYPE_SUBSYSTEM &&
		    ufe->neg == 1 &&
		    fnmatch(ufe->expr, subsystem, 0) == 0) {
			ret = 0;
			break;
		}
		if (ufe->type == UDEV_FILTER_TYPE_SYSNAME &&
		    ufe->neg == 1 &&
		    fnmatch(ufe->expr, sysname, 0) == 0) {
			ret = 0;
			break;
		}
	}

	return (ret);
}
