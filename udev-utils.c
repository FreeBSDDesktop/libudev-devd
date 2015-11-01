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
#include "udev-device.h"
#include "udev-list.h"
#include "udev-utils.h"
#include "utils.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <fcntl.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_LIBEVDEV
/* input.h BUS_* defines are included via libevdev headers */
#include <libevdev/libevdev.h>
#else
#define	BUS_PCI		0x01
#define	BUS_USB		0x03                                                                                                                                          
#define BUS_VIRTUAL	0x06                                                                                                                                          
#define BUS_ISA		0x10                                                                                                                                          
#define BUS_I8042	0x11
#endif

#define	PS2_KEYBOARD_VENDOR		0x001
#define	PS2_KEYBOARD_PRODUCT		0x001
#define	PS2_MOUSE_VENDOR		0x002
#define	PS2_MOUSE_GENERIC_PRODUCT	0x001

void create_evdev_handler(struct udev_device *udev_device);
void create_keyboard_handler(struct udev_device *udev_device);
void create_mouse_handler(struct udev_device *udev_device);
void create_joystick_handler(struct udev_device *udev_device);
void create_touchpad_handler(struct udev_device *udev_device);
void create_touchscreen_handler(struct udev_device *udev_device);
void create_sysmouse_handler(struct udev_device *udev_device);
void create_kbdmux_handler(struct udev_device *udev_device);

struct subsystem_config {
	char *subsystem;
	char *syspath;
	void (*create_handler)(struct udev_device *udev_device);
};

enum {
	IT_NONE,
	IT_KEYBOARD,
	IT_MOUSE,
	IT_TOUCHPAD,
	IT_TOUCHSCREEN,
	IT_JOYSTICK
};

struct subsystem_config subsystems[] = {
#ifdef HAVE_LIBEVDEV
	{ "input", DEV_PATH_ROOT "/input/event[0-9]*", create_evdev_handler },
#endif
	{ "input", DEV_PATH_ROOT "/ukbd[0-9]*",  create_keyboard_handler },
	{ "input", DEV_PATH_ROOT "/atkbd[0-9]*", create_keyboard_handler },
	{ "input", DEV_PATH_ROOT "/kbdmux[0-9]*", create_kbdmux_handler },
	{ "input", DEV_PATH_ROOT "/ums[0-9]*", create_mouse_handler },
	{ "input", DEV_PATH_ROOT "/psm[0-9]*", create_mouse_handler },
	{ "input", DEV_PATH_ROOT "/joy[0-9]*", create_joystick_handler },
	{ "input", DEV_PATH_ROOT "/atp[0-9]*", create_touchpad_handler },
	{ "input", DEV_PATH_ROOT "/wsp[0-9]*", create_touchpad_handler },
	{ "input", DEV_PATH_ROOT "/uep[0-9]*", create_touchscreen_handler },
	{ "input", DEV_PATH_ROOT "/sysmouse", create_sysmouse_handler },
	{ "input", DEV_PATH_ROOT "/vboxguest", create_mouse_handler },
};

static struct subsystem_config *
get_subsystem_config_by_syspath(const char *path)
{
	size_t i;

	for (i = 0; i < nitems(subsystems); i++)
		if (fnmatch(subsystems[i].syspath, path, 0) == 0)
			return (&subsystems[i]);

	return (NULL);
}

const char *
get_subsystem_by_syspath(const char *syspath)
{
	struct subsystem_config *sc;

	sc = get_subsystem_config_by_syspath(syspath);
	return (sc == NULL ? UNKNOWN_SUBSYSTEM : sc->subsystem);
}

const char *
get_sysname_by_syspath(const char *syspath)
{

	return (strbase(syspath));
}

const char *
get_devpath_by_syspath(const char *syspath)
{

	return (syspath);
}

const char *
get_syspath_by_devpath(const char *devpath)
{

	return (devpath);
}

void
invoke_create_handler(struct udev_device *ud)
{
	const char *path;
	struct subsystem_config *sc;

	path = udev_device_get_syspath(ud);
	sc = get_subsystem_config_by_syspath(path);

	if (sc != NULL && sc->create_handler != NULL)
		sc->create_handler(ud);

	return;
}

static int
set_input_device_type(struct udev_device *ud, int input_type)
{
	struct udev_list *ul;

	ul = udev_device_get_properties_list(ud);
	if (udev_list_insert(ul, "ID_INPUT", "1") < 0)
		return (-1);
	switch (input_type) {
	case IT_KEYBOARD:
		udev_list_insert(ul, "ID_INPUT_KEY", "1");
		udev_list_insert(ul, "ID_INPUT_KEYBOARD", "1");
		break;
	case IT_MOUSE:
		udev_list_insert(ul, "ID_INPUT_MOUSE", "1");
		break;
	case IT_TOUCHPAD:
		udev_list_insert(ul, "ID_INPUT_MOUSE", "1");
		udev_list_insert(ul, "ID_INPUT_TOUCHPAD", "1");
		break;
	case IT_TOUCHSCREEN:
		udev_list_insert(ul, "ID_INPUT_TOUCHSCREEN", "1");
		break;
	case IT_JOYSTICK:
		udev_list_insert(ul, "ID_INPUT_JOYSTICK", "1");
		break;
	}
	return (0);
}

static struct udev_device *
create_xorg_parent(struct udev_device *ud, const char* sysname,
    const char *name, const char *product, const char *pnp_id)
{
	struct udev_device *parent;
	struct udev *udev;
	struct udev_list *props, *sysattrs;

	/* xorg-server gets device name and vendor string from parent device */
	udev = udev_device_get_udev(ud);
	parent = udev_device_new_common(udev, sysname, UDF_IS_PARENT);
	if (parent == NULL)
		return NULL;

	props = udev_device_get_properties_list(parent);
	sysattrs = udev_device_get_sysattr_list(parent);
	udev_list_insert(props, "NAME", name);
	udev_list_insert(sysattrs, "name", name);
	if (product != NULL)
		udev_list_insert(props, "PRODUCT", product);
	if (pnp_id != NULL)
		udev_list_insert(sysattrs, "id", product);

	return (parent);
}

#ifdef HAVE_LIBEVDEV
void
create_evdev_handler(struct udev_device *ud)
{
	struct libevdev *evdev;
	struct udev_device *parent;
	struct udev *udev;
	const char *phys;
	char name[80], product[80];
	int fd, input_type = IT_NONE;
	bool is_keyboard, opened = false;

	fd = path_to_fd(udev_device_get_devnode(ud));
	if (fd == -1) {
		fd = open(udev_device_get_devnode(ud), O_RDONLY | O_CLOEXEC);
		opened = true;
	}
	if (fd == -1)
		return;

	if (libevdev_new_from_fd(fd, &evdev) != 0) {
		ERR("could not create evdev");
		return;
	}

	if (libevdev_has_event_code(evdev, EV_ABS, ABS_X) &&
	    libevdev_has_event_code(evdev, EV_ABS, ABS_Y) &&
	    libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_FINGER) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_STYLUS) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN)) {
		input_type = IT_TOUCHPAD;
	} else
	/* Its not rule of thumb but quite common that
	 * touchscreens do not advertise BTN_TOOL_FINGER event */
	if (libevdev_has_event_code(evdev, EV_ABS, ABS_X) &&
	    libevdev_has_event_code(evdev, EV_ABS, ABS_Y) &&
	    libevdev_has_event_code(evdev, EV_KEY, BTN_TOUCH) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_FINGER) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_STYLUS) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN)) {
		input_type = IT_TOUCHSCREEN;
	} else
	if (libevdev_has_event_code(evdev, EV_REL, REL_X) &&
	    libevdev_has_event_code(evdev, EV_REL, REL_Y) &&
	    libevdev_has_event_code(evdev, EV_KEY, BTN_MOUSE)) {
		input_type = IT_MOUSE;
	} else
	if (libevdev_has_event_code(evdev, EV_ABS, ABS_X) &&
	    libevdev_has_event_code(evdev, EV_ABS, ABS_Y) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_FINGER) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_STYLUS) &&
	    !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN) &&
	    libevdev_has_event_code(evdev, EV_KEY, BTN_MOUSE)) {
		input_type = IT_MOUSE;
	} else {
		is_keyboard = true;
		for (int i = KEY_ESC; i <= KEY_D; ++i) {
			if (!libevdev_has_event_code(evdev, EV_KEY, i)) {
				is_keyboard = false;
				break;
			}
		}
		if (is_keyboard)
			input_type = IT_KEYBOARD;
	}

	if (input_type == IT_NONE)
		goto bail_out;

	set_input_device_type(ud, input_type);

	phys = libevdev_get_phys(evdev);
	if (phys == NULL)
		goto bail_out;

	strlcpy(name, libevdev_get_name(evdev), sizeof(name));
	*(strchrnul(name, ',')) = '\0';	/* strip name */

	snprintf(product, sizeof(product), "%x/%x/%x/%x",
	    libevdev_get_id_bustype(evdev), libevdev_get_id_vendor(evdev),
	    libevdev_get_id_product(evdev), libevdev_get_id_version(evdev));

	parent = create_xorg_parent(ud, phys, name, product, NULL);
	if (parent != NULL)
		udev_device_set_parent(ud, parent);

bail_out:
	libevdev_free(evdev);
	if (opened)
		close(fd);
}
#endif

static size_t
syspathlen_wo_units(const char *path) {
	size_t len;

	len = strlen(path);
	while (len > 0) {
	if (path[len-1] < '0' || path[len-1] > '9')
		break;
		--len;
	}
	return len;
}

void
set_parent(struct udev_device *ud)
{
        struct udev_device *parent;
        struct udev *udev;
	char devname[DEV_PATH_MAX], mib[32], pnpinfo[1024];
	char name[80], product[80], parentname[80], *pnp_id;
	const char *sysname, *unit, *vendorstr, *prodstr, *devicestr;
	size_t len, buflen, vendorlen, prodlen, devicelen, pnplen;
	uint32_t bus, prod, vendor;

	sysname = udev_device_get_sysname(ud);
	len = syspathlen_wo_units(sysname);
	/* Check if device unit number found */
	if (strlen(sysname) == len)
		return;
	snprintf(devname, len + 1, "%s", sysname);
	unit = sysname + len;

	snprintf(mib, sizeof(mib), "dev.%s.%s.%%desc", devname, unit);
	len = sizeof(name);
	if (sysctlbyname(mib, name, &len, NULL, 0) < 0)
		return;
	*(strchrnul(name, ',')) = '\0';	/* strip name */

	snprintf(mib, sizeof(mib), "dev.%s.%s.%%pnpinfo", devname, unit);
	len = sizeof(pnpinfo);
	if (sysctlbyname(mib, pnpinfo, &len, NULL, 0) < 0)
		return;

	snprintf(mib, sizeof(mib), "dev.%s.%s.%%parent", devname, unit);
	len = sizeof(parentname);
	if (sysctlbyname(mib, parentname, &len, NULL, 0) < 0)
		return;

	vendorstr = get_kern_prop_value(pnpinfo, "vendor", &vendorlen);
	prodstr = get_kern_prop_value(pnpinfo, "product", &prodlen);
	devicestr = get_kern_prop_value(pnpinfo, "device", &devicelen);
	pnp_id = get_kern_prop_value(pnpinfo, "_HID", &pnplen);
	if (pnp_id != NULL && pnplen == 4 && strncmp(pnp_id, "none", 4) == 0)
		pnp_id = NULL;
	if (pnp_id != NULL)
		pnp_id[pnplen] = '\0';
	if (prodstr != NULL && vendorstr != NULL) {
		/* XXX: should parent be compared to uhub* to detect usb? */
		vendor = strtol(vendorstr, NULL, 0);
		prod = strtol(prodstr, NULL, 0);
		bus = BUS_USB;
	} else if (devicestr != NULL && vendorstr != NULL) {
		vendor = strtol(vendorstr, NULL, 0);
		prod = strtol(devicestr, NULL, 0);
		bus = BUS_PCI;
	} else if (strcmp(parentname, "atkbdc0") == 0) {
		if (strcmp(devname, "atkbd") == 0) {
			vendor = PS2_KEYBOARD_VENDOR;
			prod = PS2_KEYBOARD_PRODUCT;
		} else if (strcmp(devname, "psm") == 0) {
			vendor = PS2_MOUSE_VENDOR;
			prod = PS2_MOUSE_GENERIC_PRODUCT;
		} else {
			vendor = 0;
			prod = 0;
		}
		bus = BUS_I8042;
	} else {
		vendor = 0;
		prod = 0;
		bus = BUS_VIRTUAL;
	}
	snprintf(product, sizeof(product), "%x/%x/%x/0", bus, vendor, prod);
	parent = create_xorg_parent(ud, sysname, name, product, pnp_id);
	if (parent != NULL)
		udev_device_set_parent(ud, parent);

	return;
}

void
create_keyboard_handler(struct udev_device *ud)
{

	set_input_device_type(ud, IT_KEYBOARD);
	set_parent(ud);
}

void
create_mouse_handler(struct udev_device *ud)
{

	set_input_device_type(ud, IT_MOUSE);
	set_parent(ud);
}

void
create_kbdmux_handler(struct udev_device *ud)
{
	struct udev_device *parent;
	const char* sysname;

	set_input_device_type(ud, IT_KEYBOARD);
	sysname = udev_device_get_sysname(ud);
	parent = create_xorg_parent(ud, sysname,
	    "System keyboard multiplexor", "6/1/1/0", NULL);
	if (parent != NULL)
		udev_device_set_parent(ud, parent);
}

void
create_sysmouse_handler(struct udev_device *ud)
{
	struct udev_device *parent;
	const char* sysname;

	set_input_device_type(ud, IT_MOUSE);
	sysname = udev_device_get_sysname(ud);
	parent = create_xorg_parent(ud, sysname,
	    "System mouse", "6/2/1/0", NULL);
	if (parent != NULL)
		udev_device_set_parent(ud, parent);
}

void
create_joystick_handler(struct udev_device *ud)
{

	set_input_device_type(ud, IT_JOYSTICK);
	set_parent(ud);
}

void
create_touchpad_handler(struct udev_device *ud)
{

	set_input_device_type(ud, IT_TOUCHPAD);
	set_parent(ud);
}

void create_touchscreen_handler(struct udev_device *ud)
{

	set_input_device_type(ud, IT_TOUCHSCREEN);
	set_parent(ud);
}
