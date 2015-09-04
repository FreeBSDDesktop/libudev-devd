#include <sys/types.h>
#include <sys/queue.h>

enum {
	UDEV_FILTER_TYPE_SUBSYSTEM,
	UDEV_FILTER_TYPE_SYSNAME,
};
STAILQ_HEAD(udev_filter_head, udev_filter_entry);

void udev_filter_init(struct udev_filter_head *ufh);
int udev_filter_match(struct udev_filter_head *ufh, const char *syspath);
int udev_filter_add(struct udev_filter_head *ufh, int type, int neg,
    const char *expr);
void udev_filter_free(struct udev_filter_head *ufh);
