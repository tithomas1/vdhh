#ifndef QDEV_H
#define QDEV_H

#include "hw.h"
#include "qdev-core.h"

void device_set_realized(VeertuType *obj, bool value, Error **errp);

#endif
