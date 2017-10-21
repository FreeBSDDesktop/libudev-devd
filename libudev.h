#ifndef LIBUDEV_H_
#define LIBUDEV_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct udev;
struct udev_list_entry;
struct udev_device;
struct udev_monitor;
struct udev_enumerate;

struct udev *udev_new(void);
struct udev *udev_ref(struct udev *udev);
void udev_unref(struct udev *udev);
const char *udev_get_dev_path(struct udev *udev);
void *udev_get_userdata(struct udev *udev);
void udev_set_userdata(struct udev *udev, void *userdata);

struct udev_device *udev_device_new_from_syspath(struct udev *udev,
    const char *syspath);
struct udev_device *udev_device_new_from_devnum(struct udev *udev,
    char type, dev_t devnum);
struct udev_device *udev_device_new_from_subsystem_sysname(struct udev *udev,
    const char *subsystem, const char *sysname);
struct udev_device *udev_device_ref(struct udev_device *udev_device);
void udev_device_unref(struct udev_device *udev_device);
char const *udev_device_get_devnode(struct udev_device *udev_device);
char const *udev_device_get_property_value(struct udev_device *udev_device,
    char const *property);
char const *udev_device_get_sysattr_value(
    struct udev_device *udev_device, const char *sysattr);
struct udev_list_entry * udev_device_get_properties_list_entry(
    struct udev_device *udev_device);
struct udev_list_entry * udev_device_get_sysattr_list_entry(
    struct udev_device *udev_device);
struct udev_list_entry * udev_device_get_tags_list_entry(
    struct udev_device *udev_device);
struct udev_list_entry * udev_device_get_devlinks_list_entry(
    struct udev_device *udev_device);
struct udev *udev_device_get_udev(struct udev_device *udev_device);
const char *udev_device_get_syspath(struct udev_device *udev_device);
const char *udev_device_get_sysname(struct udev_device *udev_device);
const char *udev_device_get_subsystem(struct udev_device *udev_device);
struct udev_device *udev_device_get_parent(struct udev_device *udev_device);
struct udev_device *udev_device_get_parent_with_subsystem_devtype(
    struct udev_device *udev_device, const char *subsystem, const char *devtype);
int udev_device_get_is_initialized(struct udev_device *udev_device);
dev_t udev_device_get_devnum(struct udev_device *udev_device);
const char *udev_device_get_devtype(struct udev_device *udev_device);
const char *udev_device_get_driver(struct udev_device *udev_device);
const char *udev_device_get_sysnum(struct udev_device *udev_device);
unsigned long long int udev_device_get_seqnum(struct udev_device *udev_device);
unsigned long long int udev_device_get_usec_since_initialized(
    struct udev_device *udev_device);

struct udev_enumerate *udev_enumerate_new(struct udev *udev);
struct udev_enumerate *udev_enumerate_ref(struct udev_enumerate *udev_enumerate);
void udev_enumerate_unref(struct udev_enumerate *udev_enumerate);
int udev_enumerate_add_match_subsystem(
    struct udev_enumerate *udev_enumerate, const char *subsystem);
int udev_enumerate_add_nomatch_subsystem(
    struct udev_enumerate *udev_enumerate, const char *subsystem);
int udev_enumerate_add_match_sysname(
    struct udev_enumerate *udev_enumerate, const char *sysname);
int udev_enumerate_add_match_sysattr(struct udev_enumerate *udev_enumerate,
    const char *sysattr, const char *value);
int udev_enumerate_add_nomatch_sysattr(struct udev_enumerate *udev_enumerate,
    const char *sysattr, const char *value);
int udev_enumerate_add_match_property(struct udev_enumerate *udev_enumerate,
    const char *property, const char *value);
int udev_enumerate_add_match_tag(struct udev_enumerate *udev_enumerate,
    const char *tag);
int udev_enumerate_add_match_is_initialized(
    struct udev_enumerate *udev_enumerate);
int udev_enumerate_scan_devices(struct udev_enumerate *udev_enumerate);
int udev_enumerate_scan_subsystems(struct udev_enumerate *udev_enumerate);
struct udev_list_entry *udev_enumerate_get_list_entry(
    struct udev_enumerate *udev_enumerate);
int udev_enumerate_add_syspath(struct udev_enumerate *udev_enumerate,
    const char *syspath);
struct udev *udev_enumerate_get_udev(struct udev_enumerate *udev_enumerate);

struct udev_list_entry *udev_list_entry_get_next(
    struct udev_list_entry *list_entry);
const char *udev_list_entry_get_name(
    struct udev_list_entry *list_entry);
const char *udev_list_entry_get_value(struct udev_list_entry *list_entry);
#define udev_list_entry_foreach(list_entry, first_entry)		\
	for (list_entry = first_entry;					\
	     list_entry != NULL;					\
	     list_entry = udev_list_entry_get_next(list_entry))

struct udev_monitor *udev_monitor_new_from_netlink(struct udev *udev,
    const char *name);
struct udev_monitor *udev_monitor_ref(struct udev_monitor *um);
void udev_monitor_unref(struct udev_monitor *udev_monitor);
int udev_monitor_filter_add_match_subsystem_devtype(
    struct udev_monitor *udev_monitor, const char *subsystem,
    const char *devtype);
int udev_monitor_enable_receiving(struct udev_monitor *udev_monitor);
int udev_monitor_get_fd(struct udev_monitor *udev_monitor);
struct udev_device *udev_monitor_receive_device(
    struct udev_monitor *udev_monitor);
const char *udev_device_get_action(struct udev_device *udev_device);
struct udev *udev_monitor_get_udev(struct udev_monitor *udev_monitor);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIBUDEV_H_ */
