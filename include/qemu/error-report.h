#ifndef JUNK_ERROR
#define JUNK_ERROR

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

typedef struct Location {
    
} Location;

static void inline error_vprintf(char *fmt, va_list valist)
{
    vfprintf(stderr, fmt, valist);
}

static void inline error_printf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
}

static void inline error_printf_unless_qmp(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
}

static inline Location *loc_push_restore(Location *location)
{
    return NULL;
}

static inline Location *loc_push_none(Location *location)
{
    return NULL;
}

static inline Location *loc_pop(Location *location)
{
    return NULL;
}

static inline Location *loc_save(Location *location)
{
    return NULL;
}


static void inline error_report(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf("\n");
}

static void inline error_vreport(char *fmt, va_list valist)
{
    error_vprintf(fmt, valist);
    error_printf("\n");
}

static inline char *error_get_progname()
{
    return "veertu";
}

static inline void error_set_progname(char *name)
{
    
}

static inline void loc_restore(Location *location)
{
    
}

static inline void loc_set_node()
{
    
}

static inline void loc_set_none()
{
    
}

static void inline loc_set_cmdline(char **cmd, int index, int count)
{
    
}

static inline void loc_set_file(char *filename, int line)
{
    
}

#endif
