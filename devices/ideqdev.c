/*
 * ide bus support for qdev.
 *
 * Copyright (c) 2009 Gerd Hoffmann <kraxel@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include <hw.h>
#include "emudma.h"
#include "qemu/error-report.h"
#include <devices/ideinternal.h>
#include "emublock-backend.h"
#include "emublockdev.h"
#include "sysemu.h"
#include "qapi/visitor.h"
#include "emublockdev.h"
#include "typedefs.h"

/* --------------------------------- */

static char *idebus_get_fw_dev_path(DeviceState *dev);


static void ide_bus_class_init(VeertuTypeClassHold *klass, void *data)
{
    BusClass *k = BUS_CLASS(klass);

    k->get_fw_dev_path = idebus_get_fw_dev_path;
}

static const VeertuTypeInfo ide_bus_info = {
    .name = TYPE_IDE_BUS,
    .parent = TYPE_BUS,
    .instance_size = sizeof(IDEBus),
    .class_init = ide_bus_class_init,
};

void ide_bus_new(IDEBus *idebus, size_t idebus_size, DeviceState *dev,
                 int bus_id, int max_units)
{
    qbus_create_inplace(idebus, idebus_size, TYPE_IDE_BUS, dev, NULL);
    idebus->bus_id = bus_id;
    idebus->max_units = max_units;
}

static char *idebus_get_fw_dev_path(DeviceState *dev)
{
    char path[30];

    snprintf(path, sizeof(path), "%s@%x", qdev_fw_name(dev),
             ((IDEBus*)dev->parent_bus)->bus_id);

    return g_strdup(path);
}

static int disk_num = 0;

extern char *get_current_conf_name();

static int ide_qdev_init(DeviceState *qdev)
{
    IDEDevice *dev = IDE_DEVICE(qdev);
    IDEDeviceClass *dc = IDE_DEVICE_GET_CLASS(dev);
    IDEBus *bus = DO_UPCAST(IDEBus, qbus, qdev->parent_bus);
    char name[256];
    
    snprintf(name, 256, "cdrom.%d", disk_num++);
    //printf("name is %s\n", get_current_conf_name());
    dev->conf.blk = blk_by_name(get_current_conf_name());
    if (dev->conf.blk)
        blk_attach_dev(dev->conf.blk, dev);
    
    dev->unit = -1;

    if (!dev->conf.blk) {
        error_report("No drive specified");
        goto err;
    }
    if (dev->unit == -1) {
        dev->unit = bus->master ? 1 : 0;
    }

    if (dev->unit >= bus->max_units) {
        error_report("Can't create IDE unit %d, bus supports only %d units",
                     dev->unit, bus->max_units);
        goto err;
    }

    switch (dev->unit) {
    case 0:
        if (bus->master) {
            error_report("IDE unit %d is in use", dev->unit);
            goto err;
        }
        bus->master = dev;
        break;
    case 1:
        if (bus->slave) {
            error_report("IDE unit %d is in use", dev->unit);
            goto err;
        }
        bus->slave = dev;
        break;
    default:
        error_report("Invalid IDE unit %d", dev->unit);
        goto err;
    }
    return dc->init(dev);

err:
    return -1;
}

IDEDevice *ide_create_drive(IDEBus *bus, int unit, DriveInfo *drive)
{
    DeviceState *dev;
    Error *error;
    struct BlockBackend *block;
    IDEDevice *idedev;

    dev = qdev_create(&bus->qbus, drive->media_cd ? "ide-cd" : "ide-hd");
    idedev = IDE_DEVICE(dev);
    //"unit"
    idedev->unit = unit;
    
    block = blk_by_legacy_dinfo(drive);
    
    abort(); //deal with drive

    qdev_init_nofail(dev);
    return DO_UPCAST(IDEDevice, qdev, dev);
}

int ide_get_geometry(BusState *bus, int unit,
                     int16_t *cyls, int8_t *heads, int8_t *secs)
{
    IDEState *s = &DO_UPCAST(IDEBus, qbus, bus)->ifs[unit];

    if (s->drive_kind != IDE_HD || !s->blk) {
        return -1;
    }

    *cyls = s->cylinders;
    *heads = s->heads;
    *secs = s->sectors;
    return 0;
}

int ide_get_bios_chs_trans(BusState *bus, int unit)
{
    return DO_UPCAST(IDEBus, qbus, bus)->ifs[unit].chs_trans;
}

/* --------------------------------- */

typedef struct IDEDrive {
    IDEDevice dev;
} IDEDrive;

static int ide_dev_initfn(IDEDevice *dev, IDEDriveKind kind)
{
    IDEBus *bus = DO_UPCAST(IDEBus, qbus, dev->qdev.parent_bus);
    IDEState *s = bus->ifs + dev->unit;
    Error *err = NULL;

    if (dev->conf.discard_granularity == -1) {
        dev->conf.discard_granularity = 512;
    } else if (dev->conf.discard_granularity &&
               dev->conf.discard_granularity != 512) {
        error_report("discard_granularity must be 512 for ide");
        return -1;
    }

    if (dev->conf.logical_block_size != 512) {
        error_report("logical_block_size must be 512 for IDE");
        return -1;
    }

    blkconf_serial(&dev->conf, &dev->serial);
    if (kind != IDE_CD) {
        blkconf_geometry(&dev->conf, &dev->chs_trans, 65535, 16, 255, &err);
        if (err) {
            error_report("%s", error_get_pretty(err));
            error_free(err);
            return -1;
        }
    }

    if (ide_init_drive(s, dev->conf.blk, kind,
                       dev->version, dev->serial, dev->model, dev->wwn,
                       dev->conf.cyls, dev->conf.heads, dev->conf.secs,
                       dev->chs_trans) < 0) {
        return -1;
    }

    if (!dev->version) {
        dev->version = g_strdup(s->version);
    }
    if (!dev->serial) {
        dev->serial = g_strdup(s->drive_serial_str);
    }

    add_boot_device_path(dev->conf.bootindex, &dev->qdev,
                         dev->unit ? "/disk@1" : "/disk@0");

    return 0;
}

static void ide_dev_set_bootindex(IDEDevice *d, int32_t boot_index, Error** errp)
{
    Error *local_err = NULL;

    /* check whether bootindex is present in fw_boot_order list  */
    check_boot_index(boot_index, &local_err);
    if (local_err) {
        goto out;
    }
    /* change bootindex to a new one */
    d->conf.bootindex = boot_index;

    if (d->unit != -1) {
        add_boot_device_path(d->conf.bootindex, &d->qdev,
                             d->unit ? "/disk@1" : "/disk@0");
    }
out:
    if (local_err) {
        error_propagate(errp, local_err);
    }
}

static void ide_dev_instance_init(VeertuType *obj)
{
    ide_dev_set_bootindex(IDE_DEVICE(obj), -1, &error_abort);
}

static int ide_hd_initfn(IDEDevice *dev)
{
    dev->chs_trans = BIOS_ATA_TRANSLATION_AUTO;
    
    dev->conf.logical_block_size = 512;
    dev->conf.physical_block_size = 512;
    dev->conf.min_io_size = 0;
    dev->conf.opt_io_size = 0;
    dev->conf.discard_granularity = -1;

    return ide_dev_initfn(dev, IDE_HD);
}

static int ide_cd_initfn(IDEDevice *dev)
{
    dev->conf.logical_block_size = 512;
    dev->conf.physical_block_size = 512;
    dev->conf.min_io_size = 0;
    dev->conf.opt_io_size = 0;
    dev->conf.discard_granularity = -1;
    
    return ide_dev_initfn(dev, IDE_CD);
}

static int ide_drive_initfn(IDEDevice *dev)
{
    DriveInfo *dinfo = blk_legacy_dinfo(dev->conf.blk);
    
    dev->conf.logical_block_size = 512;
    dev->conf.physical_block_size = 512;
    dev->conf.min_io_size = 0;
    dev->conf.opt_io_size = 0;
    dev->conf.discard_granularity = -1;

    return ide_dev_initfn(dev, dinfo && dinfo->media_cd ? IDE_CD : IDE_HD);
}

static void ide_hd_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    IDEDeviceClass *k = IDE_DEVICE_CLASS(klass);
    k->init = ide_hd_initfn;
    dc->fw_name = "drive";
    dc->desc = "virtual IDE disk";
}

static const VeertuTypeInfo ide_hd_info = {
    .name          = "ide-hd",
    .parent        = TYPE_IDE_DEVICE,
    .instance_size = sizeof(IDEDrive),
    .class_init    = ide_hd_class_init,
};



static void ide_cd_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    IDEDeviceClass *k = IDE_DEVICE_CLASS(klass);
    k->init = ide_cd_initfn;
    dc->fw_name = "drive";
    dc->desc = "virtual IDE CD-ROM";
}

static const VeertuTypeInfo ide_cd_info = {
    .name          = "ide-cd",
    .parent        = TYPE_IDE_DEVICE,
    .instance_size = sizeof(IDEDrive),
    .class_init    = ide_cd_class_init,
};

static void ide_drive_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    IDEDeviceClass *k = IDE_DEVICE_CLASS(klass);
    k->init = ide_drive_initfn;
    dc->fw_name = "drive";
    dc->desc = "virtual IDE disk or CD-ROM (legacy)";
}

static const VeertuTypeInfo ide_drive_info = {
    .name          = "ide-drive",
    .parent        = TYPE_IDE_DEVICE,
    .instance_size = sizeof(IDEDrive),
    .class_init    = ide_drive_class_init,
};

static void ide_device_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);
    k->init = ide_qdev_init;
    set_bit(DEVICE_CATEGORY_STORAGE, k->categories);
    k->bus_type = TYPE_IDE_BUS;
}

static const VeertuTypeInfo ide_device_type_info = {
    .name = TYPE_IDE_DEVICE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(IDEDevice),
    .class_size = sizeof(IDEDeviceClass),
    .class_init = ide_device_class_init,
    .instance_init = ide_dev_instance_init,
};

void ide_register_types(void)
{
    register_type_internal(&ide_bus_info);
    register_type_internal(&ide_hd_info);
    register_type_internal(&ide_cd_info);
    register_type_internal(&ide_drive_info);
    register_type_internal(&ide_device_type_info);
}
