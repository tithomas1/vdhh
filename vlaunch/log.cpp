#include "log.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>

int com_veertu_kext_netflt_loglevel = 4;

extern "C" void veertu_log(int level, const char* file, int line, const char* fmt, ...) {

    if (level > com_veertu_kext_netflt_loglevel)
        return;

    // TODO: need lock??

    size_t len = fmt ? strlen(fmt) : 0;

    if (level >= 3 && len > 0) {
        // print only last component
        int i = file ? (int)strlen(file) : 0;
        while(--i >= 0 && !(file[i] == '/' || file[i] == '\\'));
        printf("%s:%d: ", i >= 0 ? file + i + 1 : file, line);
    }

    // printf message
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    if (len > 0 && !(fmt[len-1] == '\n' || fmt[len-1] == '\r'))
        printf("\n");
}
