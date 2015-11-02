#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <unistd.h>

/*
#define	ENABLE_TRACE
#define	LOG_LEVEL       1
*/
#ifndef	LOG_LEVEL
#define	LOG_LEVEL -1
#endif

#ifdef ENABLE_TRACE
#define	TRC(msg, ...)							\
do {									\
	int saved_errno_ = errno;					\
	fprintf(stderr, "%s" msg "\n", __FUNCTION__, ##__VA_ARGS__);	\
	errno = saved_errno_;						\
} while (0)
#else
#define	TRC(msg, ...)
#endif

#define LOG(level, msg, ...) do {					\
	if (level < LOG_LEVEL) {					\
		if (level == 0 && errno != 0)				\
			fprintf(stderr, msg" %d(%s)\n", ##__VA_ARGS__,	\
			    errno, strerror(errno));			\
		else							\
			fprintf(stderr, msg"\n", ##__VA_ARGS__);	\
	}								\
} while (0)
#define	ERR(...)	LOG(0, __VA_ARGS__)
#define	DBG(...)	LOG(1, __VA_ARGS__)

#define	UNIMPL()	ERR("%s is unimplemented", __FUNCTION__)

typedef int (* scan_cb_t) (const char *path, int type, void *args);

struct scan_ctx {
	scan_cb_t cb;
	void *args;
};

char *strbase(const char *path);
char *get_kern_prop_value(const char *buf, const char *prop, size_t *len);
int match_kern_prop_value(const char *buf, const char *prop, const char *value);
int socket_connect(const char *path);
ssize_t socket_readline(int fd, char *buf, size_t len);
int path_to_fd(const char *path);
int scandir_recursive(char *path, size_t len, struct scan_ctx *ctx);
#ifdef HAVE_DEVINFO_H
int scandev_recursive(struct scan_ctx *ctx);
#endif

#endif /* UTILS_H_ */
