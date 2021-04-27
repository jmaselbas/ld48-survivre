#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "core.h"

#ifndef WINDOWS
__asm__(".symver fstat,fstat@GLIBC_2.2.5");
#endif

void *
xvmalloc(void *base, size_t align, size_t size)
{
	void *addr;
	UNUSED(base);
	UNUSED(align);

	addr = calloc(1, size);
	if (!addr)
		die("xvmalloc: %s\n", strerror(errno));

	return addr;
}

ssize_t
file_size(const char *path)
{
	struct stat sb;
	if (stat(path, &sb) == 0)
		return sb.st_size;
	return -1;
}

ssize_t
file_read(const char *path, void *buf, size_t size)
{
	ssize_t ret = -1;
	int fd;

	if (buf) {
		fd = open(path, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "fail to load '%s'\n", path);
			ret = -1;
		} else {
			ret = read(fd, buf, size);
			close(fd);
		}
	}

	return ret;
}

time_t
file_time(const char *path)
{
#ifndef WINDOWS
	struct stat sb;
	if (stat(path, &sb) == 0)
		return sb.st_ctime;
#endif
	return 0;
}
