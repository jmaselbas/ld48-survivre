#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <time.h>

#include "../engine/util.h"

#define UNUSED(arg) ((void)arg)

void *xvmalloc(void *base, size_t align, size_t size);
int64_t file_size(const char *path);
int64_t file_read(const char *path, void *buf, size_t size);
time_t file_time(const char *path);

#endif
