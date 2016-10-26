#pragma once

#include <vlaunch/vobj.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

int vmsg_read(int fd, vobj_t msg);
int vmsg_write(int fd, const vobj_t msg);

__END_DECLS
