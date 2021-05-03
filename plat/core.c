#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "plat/core.h"

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

int64_t
file_size(const char *path)
{
	int64_t size;
	FILE *f;

	f = fopen(path, "r");
	if (f == NULL) {
		size = -1;
	} else {
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fclose(f);
	}

	return size;
}

int64_t
file_read(const char *path, void *buf, size_t size)
{
	int64_t ret = -1;
	FILE *f;

	if (buf) {
		f = fopen(path, "r");
		if (f == NULL) {
			fprintf(stderr, "fail to load '%s'\n", path);
			ret = -1;
		} else {
			ret = fread(buf, sizeof(char), size, f);
			fclose(f);
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
