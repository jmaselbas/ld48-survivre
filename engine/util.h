#ifndef UTIL_H
#define UTIL_H

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

void warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
