#pragma once

#include <sys/cdefs.h>

__BEGIN_DECLS

extern void veertu_log(int level, const char* file, int line, const char* fmt, ...);

__END_DECLS

#ifndef LOG
#define LOG(...) veertu_log(4, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#ifndef ERR
#define ERR(...) veertu_log(0, __FILE__, __LINE__, ##__VA_ARGS__)
#endif
