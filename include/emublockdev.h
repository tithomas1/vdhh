/*
 * QEMU host block devices
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include "block.h"
#include "qapi/error.h"
#include "qemu/queue.h"

void blockdev_mark_auto_del(BlockBackend *blk);
void blockdev_auto_del(BlockBackend *blk);

typedef enum {
    IF_DEFAULT = -1,            /* for use with drive_add() only */
    /*
     * IF_IDE must be zero, because we want QEMUMachine member
     * block_default_type to default-initialize to IF_IDE
     */
    IF_IDE = 0,
    IF_NONE,
    IF_SCSI, IF_FLOPPY, IF_PFLASH, IF_MTD, IF_SD, IF_XEN,
    IF_COUNT
} BlockInterfaceType;

struct DriveInfo {
    const char *devaddr;
    BlockInterfaceType type;
    int bus;
    int unit;
    int auto_del;               /* see blockdev_mark_auto_del() */
    bool is_default;            /* Added by default_drive() ?  */
    int media_cd;
    int cyls, heads, secs, trans;
    QemuOpts *opts;
    char *serial;
    QTAILQ_ENTRY(DriveInfo) next;
};

DriveInfo *blk_legacy_dinfo(BlockBackend *blk);
DriveInfo *blk_set_legacy_dinfo(BlockBackend *blk, DriveInfo *dinfo);
BlockBackend *blk_by_legacy_dinfo(DriveInfo *dinfo);

void override_max_devs(BlockInterfaceType type, int max_devs);

DriveInfo *drive_get(BlockInterfaceType type, int bus, int unit);
bool drive_check_orphaned(void);
DriveInfo *drive_get_by_index(BlockInterfaceType type, int index);
int drive_get_max_bus(BlockInterfaceType type);
int drive_get_max_devs(BlockInterfaceType type);
DriveInfo *drive_get_next(BlockInterfaceType type);

QemuOpts *drive_def(const char *optstr);
QemuOpts *drive_add(BlockInterfaceType type, int index, const char *file,
                    const char *optstr);
DriveInfo *drive_new(QemuOpts *arg, BlockInterfaceType block_default_type);

/* device-hotplug */

//DriveInfo *add_init_drive(const char *opts);

void qmp_change_blockdev(const char *device, const char *filename,
                         const char *format, Error **errp);
void do_commit(Monitor *mon, const QDict *qdict);
int do_drive_del(Monitor *mon, const QDict *qdict, QObject **ret_data);
#endif
