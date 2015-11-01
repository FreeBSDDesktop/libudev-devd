#ifndef UDEV_DEVICE_H_
#define UDEV_DEVICE_H_

#include "libudev.h"
#include "udev-list.h"

/* udev_device flags */
#define UDF_ACTION_MASK		0x00000003 /* Should be in range 0x00 - 0xFF */
#define UDF_ACTION_NONE		0x00000000
#define UDF_ACTION_ADD		0x00000001
#define UDF_ACTION_REMOVE	0x00000002
#define	UDF_IS_PARENT		0x00000004

struct udev_device *udev_device_new_common(struct udev *udev,
    const char *syspath, uint32_t flags);
struct udev_list *udev_device_get_properties_list(struct udev_device *ud);
struct udev_list *udev_device_get_sysattr_list(struct udev_device *ud);
void udev_device_set_parent(struct udev_device *ud, struct udev_device *parent);

#endif /* UDEV_DVICE_H_ */
