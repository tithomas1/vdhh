#include "monitor/monitor.h"
#include "sysemu.h"
#include "emublockdev.h"

int pci_drive_hot_add(Monitor *mon, const QDict *qdict, DriveInfo *dinfo)
{
    /* On non-x86 we don't do PCI hotplug */
    return -1;
}
