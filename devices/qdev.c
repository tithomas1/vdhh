/*
 *  Dynamic device configuration and creation.
 *
 *  Copyright (c) 2009 CodeSourcery
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

/* The theory here is that it should be possible to create a machine without
   knowledge of specific devices.  Historically board init routines have
   passed a bunch of arguments to each device, requiring the board know
   exactly which device it is dealing with.  This file provides an abstract
   API for device configuration and initialization.  Devices will generally
   inherit from a particular bus (e.g. PCI or I2C) rather than
   this API directly.  */

#include "nqdev.h"
#include "qsysbus.h"
#include "sysemu.h"
#include "qapi/error.h"
#include "qapi/qmp/qerror.h"
#include "qapi/visitor.h"
#include "qapi/qmp/qjson.h"
#include "boards.h"
#include "qapi-event.h"
#include "net/net.h"

int qdev_hotplug = 0;
static bool qdev_hot_added = false;
static bool qdev_hot_removed = false;


const VMStateDescription *qdev_get_vmsd(DeviceState *dev)
{
    DeviceClass *dc = DEVICE_GET_CLASS(dev);
    return dc->vmsd;
}

const char *qdev_fw_name(DeviceState *dev)
{
    DeviceClass *dc = DEVICE_GET_CLASS(dev);

    if (dc->fw_name) {
        return dc->fw_name;
    }

    return get_typename(VeertuTypeHold(dev));
}

static void bus_remove_child(BusState *bus, DeviceState *child)
{
    BusChild *kid;

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        if (kid->child == child) {
            char name[32];

            snprintf(name, sizeof(name), "silib[%d]", kid->index);
            QTAILQ_REMOVE(&bus->children, kid, sibling);

            /* This gives back ownership of kid->child back to us.  */
            g_free(kid);
            return;
        }
    }
}

static void bus_add_child(BusState *bus, DeviceState *child)
{
    char name[32];
    BusChild *kid = g_malloc0(sizeof(*kid));

    kid->index = bus->max_index++;
    kid->child = child;

    QTAILQ_INSERT_HEAD(&bus->children, kid, sibling);
}

void qdev_set_parent_bus(DeviceState *dev, BusState *bus)
{
    dev->parent_bus = bus;
    bus_add_child(bus, dev);
}

static void qbus_set_hotplug_handler_internal(BusState *bus, VeertuType *handler,
                                              Error **errp)
{

}

void qbus_set_hotplug_handler(BusState *bus, DeviceState *handler, Error **errp)
{
    qbus_set_hotplug_handler_internal(bus, VeertuTypeHold(handler), errp);
}

void qbus_set_bus_hotplug_handler(BusState *bus, Error **errp)
{
    qbus_set_hotplug_handler_internal(bus, VeertuTypeHold(bus), errp);
}

/* Create a new device.  This only initializes the device state structure
   and allows properties to be set.  qdev_init should be called to
   initialize the actual device emulation.  */
DeviceState *qdev_create(BusState *bus, const char *name)
{
    DeviceState *dev;

    dev = qdev_try_create(bus, name);
    if (!dev) {
        if (bus) {
            error_report("Unknown device '%s' for bus '%s'", name,
                         get_typename(VeertuTypeHold(bus)));
        } else {
            error_report("Unknown device '%s' for default sysbus", name);
        }
        abort();
    }

    return dev;
}

DeviceState *qdev_try_create(BusState *bus, const char *type)
{
    DeviceState *dev;

    if (get_type_class(type) == NULL) {
        return NULL;
    }
    dev = DEVICE(vtype_new(type));
    if (!dev) {
        return NULL;
    }

    if (!bus) {
        bus = sysbus_get_default();
    }

    qdev_set_parent_bus(dev, bus);
    return dev;
}

/* Initialize a device.  Device properties should be set before calling
   this function.  IRQs and MMIO regions should be connected/mapped after
   calling this function.
   On failure, destroy the device and return negative value.
   Return 0 on success.  */
int qdev_init(DeviceState *dev)
{
    Error *local_err = NULL;

    assert(!dev->realized);

    device_set_realized(VeertuTypeHold(dev), true, &local_err);

    return 0;
}

static void device_realize(DeviceState *dev, Error **errp)
{
    DeviceClass *dc = DEVICE_GET_CLASS(dev);

    if (dc->init) {
        int rc = dc->init(dev);
        if (rc < 0) {
            error_setg(errp, "Device initialization failed.");
            return;
        }
    }
}

static void device_unrealize(DeviceState *dev, Error **errp)
{
    DeviceClass *dc = DEVICE_GET_CLASS(dev);

    if (dc->exit) {
        int rc = dc->exit(dev);
        if (rc < 0) {
            error_setg(errp, "Device exit failed.");
            return;
        }
    }
}

void qdev_set_legacy_instance_id(DeviceState *dev, int alias_id,
                                 int required_for_version)
{
    assert(!dev->realized);
    dev->instance_id_alias = alias_id;
    dev->alias_required_for_version = required_for_version;
}

/* can be used as ->unplug() callback for the simple cases */
static int qdev_simple_unplug_cb(DeviceState *dev)
{
    /* just zap it */
    return 0;
}

void qdev_unplug(DeviceState *dev, Error **errp)
{
    device_unparent(dev, errp);
}

static int qdev_reset_one(DeviceState *dev, void *opaque)
{
    device_reset(dev);

    return 0;
}

static int qbus_reset_one(BusState *bus, void *opaque)
{
    BusClass *bc = BUS_GET_CLASS(bus);
    if (bc->reset) {
        bc->reset(bus);
    }
    return 0;
}

void qdev_reset_all(DeviceState *dev)
{
    qdev_walk_children(dev, NULL, NULL, qdev_reset_one, qbus_reset_one, NULL);
}

void qbus_reset_all(BusState *bus)
{
    qbus_walk_children(bus, NULL, NULL, qdev_reset_one, qbus_reset_one, NULL);
}

void qbus_reset_all_fn(void *opaque)
{
    BusState *bus = opaque;
    qbus_reset_all(bus);
}

/* Like qdev_init(), but terminate program via error_report() instead of
   returning an error value.  This is okay during machine creation.
   Don't use for hotplug, because there callers need to recover from
   failure.  Exception: if you know the device's init() callback can't
   fail, then qdev_init_nofail() can't fail either, and is therefore
   usable even then.  But relying on the device implementation that
   way is somewhat unclean, and best avoided.  */
void qdev_init_nofail(DeviceState *dev)
{
    const char *typename = get_typename(VeertuTypeHold(dev));

    if (qdev_init(dev) < 0) {
        error_report("Initialization of device %s failed", typename);
        exit(1);
    }
}

static void(*machine_creation_handler)(void*) = NULL;
static void* machine_creation_opaque = NULL;

void qdev_on_machine_creation_done(void(*handler)(void*), void* opaque) {
    assert(NULL == machine_creation_handler);
    machine_creation_handler = handler;
    machine_creation_opaque = opaque;
}

void qdev_machine_creation_done(void)
{
    /*
     * ok, initial machine setup is done, starting from now we can
     * only create hotpluggable devices
     */
    qdev_hotplug = 1;

    // notify others
    if (machine_creation_handler)
        machine_creation_handler(machine_creation_opaque);
}

bool qdev_machine_modified(void)
{
    return qdev_hot_added || qdev_hot_removed;
}

BusState *qdev_get_parent_bus(DeviceState *dev)
{
    return dev->parent_bus;
}

void qdev_signal_event(DeviceState* dev, int event) {
    BusState* bus = dev->parent_bus;
    while (bus) {
        if (bus->monitor)
            bus->monitor(dev, event, bus->monitor_opaque);
        bus = bus->parent ? qdev_get_parent_bus(bus->parent) : NULL;
    }
}

void qdev_set_event_monitor(BusState* bus, DeviceEventMonitor monitor, void* opaque) {
    bus->monitor = monitor;
    bus->monitor_opaque = opaque;
}

void qdev_pass_gpios(DeviceState *dev, DeviceState *container,
                     const char *name)
{
}

BusState *qdev_get_child_bus(DeviceState *dev, const char *name)
{
    BusState *bus;

    QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        if (strcmp(name, bus->name) == 0) {
            return bus;
        }
    }
    return NULL;
}

int qbus_walk_children(BusState *bus,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque)
{
    BusChild *kid;
    int err;

    if (pre_busfn) {
        err = pre_busfn(bus, opaque);
        if (err) {
            return err;
        }
    }

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        err = qdev_walk_children(kid->child,
                                 pre_devfn, pre_busfn,
                                 post_devfn, post_busfn, opaque);
        if (err < 0) {
            return err;
        }
    }

    if (post_busfn) {
        err = post_busfn(bus, opaque);
        if (err) {
            return err;
        }
    }

    return 0;
}

int qdev_walk_children(DeviceState *dev,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque)
{
    BusState *bus;
    int err;

    if (pre_devfn) {
        err = pre_devfn(dev, opaque);
        if (err) {
            return err;
        }
    }

    QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        err = qbus_walk_children(bus, pre_devfn, pre_busfn,
                                 post_devfn, post_busfn, opaque);
        if (err < 0) {
            return err;
        }
    }

    if (post_devfn) {
        err = post_devfn(dev, opaque);
        if (err) {
            return err;
        }
    }

    return 0;
}

DeviceState *qdev_find_recursive(BusState *bus, const char *id)
{
    BusChild *kid;
    DeviceState *ret;
    BusState *child;

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        DeviceState *dev = kid->child;

        if (dev->id && strcmp(dev->id, id) == 0) {
            return dev;
        }

        QLIST_FOREACH(child, &dev->child_bus, sibling) {
            ret = qdev_find_recursive(child, id);
            if (ret) {
                return ret;
            }
        }
    }
    return NULL;
}

static void qbus_realize(BusState *bus, DeviceState *parent, const char *name)
{
    const char *typename = get_typename(VeertuTypeHold(bus));
    BusClass *bc;
    char *buf;
    int i, len, bus_id;

    bus->parent = parent;

    if (name) {
        bus->name = g_strdup(name);
    } else if (bus->parent && bus->parent->id) {
        /* parent device has id -> use it plus parent-bus-id for bus name */
        bus_id = bus->parent->num_child_bus;

        len = strlen(bus->parent->id) + 16;
        buf = g_malloc(len);
        snprintf(buf, len, "%s.%d", bus->parent->id, bus_id);
        bus->name = buf;
    } else {
        /* no id -> use lowercase bus type plus global bus-id for bus name */
        bc = BUS_GET_CLASS(bus);
        bus_id = bc->automatic_ids++;

        len = strlen(typename) + 16;
        buf = g_malloc(len);
        len = snprintf(buf, len, "%s.%d", typename, bus_id);
        for (i = 0; i < len; i++) {
            buf[i] = vmx_tolower(buf[i]);
        }
        bus->name = buf;
    }

    if (bus->parent) {
        QLIST_INSERT_HEAD(&bus->parent->child_bus, bus, sibling);
        bus->parent->num_child_bus++;
    } else if (bus != sysbus_get_default()) {
        /* TODO: once all bus devices are qdevified,
           only reset handler for main_system_bus should be registered here. */
        vmx_register_reset(qbus_reset_all_fn, bus);
    }
}

static void bus_unparent(VeertuType *obj)
{
    BusState *bus = BUS(obj);
    BusChild *kid;


    if (bus->parent) {
        QLIST_REMOVE(bus, sibling);
        bus->parent->num_child_bus--;
        bus->parent = NULL;
    } else {
        assert(bus != sysbus_get_default()); /* main_system_bus is never freed */
        vmx_unregister_reset(qbus_reset_all_fn, bus);
    }
}

static bool bus_get_realized(VeertuType *obj, Error **errp)
{
    BusState *bus = BUS(obj);

    return bus->realized;
}

static void bus_set_realized(VeertuType *obj, bool value, Error **errp)
{
    BusState *bus = BUS(obj);
    BusClass *bc = BUS_GET_CLASS(bus);
    BusChild *kid;
    Error *local_err = NULL;

    if (value && !bus->realized) {
        if (bc->realize) {
            bc->realize(bus, &local_err);
        }

        /* TODO: recursive realization */
    } else if (!value && bus->realized) {
        QTAILQ_FOREACH(kid, &bus->children, sibling) {
            DeviceState *dev = kid->child;
            device_set_realized(VeertuTypeHold(dev), false, &local_err);
            if (local_err != NULL) {
                break;
            }
        }
        if (bc->unrealize && local_err == NULL) {
            bc->unrealize(bus, &local_err);
        }
    }

    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

    bus->realized = value;
}

void qbus_create_inplace(void *bus, size_t size, const char *typename,
                         DeviceState *parent, const char *name)
{
    vtype_init(bus, size, typename);
    qbus_realize(bus, parent, name);
}

BusState *qbus_create(const char *typename, DeviceState *parent, const char *name)
{
    BusState *bus;

    bus = BUS(vtype_new(typename));
    qbus_realize(bus, parent, name);

    return bus;
}

static char *bus_get_fw_dev_path(BusState *bus, DeviceState *dev)
{
    BusClass *bc = BUS_GET_CLASS(bus);

    if (bc->get_fw_dev_path) {
        return bc->get_fw_dev_path(dev);
    }

    return NULL;
}

static char *qdev_get_fw_dev_path_from_handler(BusState *bus, DeviceState *dev)
{
    VeertuType *obj = VeertuTypeHold(dev);
    char *d = NULL;

    while (!d && obj->father) {
        obj = obj->father;
        //d = fw_path_provider_try_get_dev_path(obj, bus, dev);
        abort();
    }
    return d;
}

static int qdev_get_fw_dev_path_helper(DeviceState *dev, char *p, int size)
{
    int l = 0;

    if (dev && dev->parent_bus) {
        char *d;
        l = qdev_get_fw_dev_path_helper(dev->parent_bus->parent, p, size);
        d = qdev_get_fw_dev_path_from_handler(dev->parent_bus, dev);
        if (!d) {
            d = bus_get_fw_dev_path(dev->parent_bus, dev);
        }
        if (d) {
            l += snprintf(p + l, size - l, "%s", d);
            g_free(d);
        } else {
            return l;
        }
    }
    l += snprintf(p + l , size - l, "/");

    return l;
}

char* qdev_get_fw_dev_path(DeviceState *dev)
{
    char path[128];
    int l;

    l = qdev_get_fw_dev_path_helper(dev, path, 128);

    path[l-1] = '\0';

    return g_strdup(path);
}

char *qdev_get_dev_path(DeviceState *dev)
{
    BusClass *bc;

    if (!dev || !dev->parent_bus) {
        return NULL;
    }

    bc = BUS_GET_CLASS(dev->parent_bus);
    if (bc->get_dev_path) {
        return bc->get_dev_path(dev);
    }

    return NULL;
}

/**
 * @qdev_property_add_static - add a @Property to a device.
 *
 * Static properties access data in a struct.  The actual type of the
 * property and the field depends on the property type.
 */
void qdev_property_add_static(DeviceState *dev, Property *prop,
                              Error **errp)
{


}

/* @qdev_alias_all_properties - Add alias properties to the source object for
 * all qdev properties on the target DeviceState.
 */
void qdev_alias_all_properties(DeviceState *target, VeertuType *source)
{
}

static int qdev_add_hotpluggable_device(VeertuType *obj, void *opaque)
{
    return 0;
}

GSList *qdev_build_hotpluggable_device_list(VeertuType *peripheral)
{
    GSList *list = NULL;

    return list;
}

static bool device_get_realized(VeertuType *obj, Error **errp)
{
    DeviceState *dev = DEVICE(obj);
    abort();
    return dev->realized;
}

void device_set_realized(VeertuType *obj, bool value, Error **errp)
{
    DeviceState *dev = DEVICE(obj);
    DeviceClass *dc = DEVICE_GET_CLASS(dev);
    BusState *bus;
    Error *local_err = NULL;

    if (dev->hotplugged && !dc->hotpluggable) {
        error_set(errp, QERR_DEVICE_NO_HOTPLUG, get_typename(obj));
        return;
    }

    if (value && !dev->realized) {
        if (dc->realize) {
            dc->realize(dev, &local_err);
        }

        if (local_err != NULL) {
            goto fail;
        }

        if (local_err != NULL) {
            goto post_realize_fail;
        }

        if (qdev_get_vmsd(dev)) {
            vmstate_register_with_alias_id(dev, -1, qdev_get_vmsd(dev), dev,
                                           dev->instance_id_alias,
                                           dev->alias_required_for_version);
        }

        QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        }
        if (dev->hotplugged) {
            device_reset(dev);
        }
        dev->pending_deleted_event = false;
    } else if (!value && dev->realized) {
        Error **local_errp = NULL;
        QLIST_FOREACH(bus, &dev->child_bus, sibling) {
            device_set_realized(VeertuTypeHold(bus), false, &local_err);
        }
        if (qdev_get_vmsd(dev)) {
            vmstate_unregister(dev, qdev_get_vmsd(dev), dev);
        }
        if (dc->unrealize) {
            local_errp = local_err ? NULL : &local_err;
            dc->unrealize(dev, local_errp);
        }
        dev->pending_deleted_event = true;
    }

    if (local_err != NULL) {
        goto fail;
    }

    dev->realized = value;
    return;

child_realize_fail:
    QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        device_set_realized(VeertuTypeHold(bus), false, &local_err);
    }

    if (qdev_get_vmsd(dev)) {
        vmstate_unregister(dev, qdev_get_vmsd(dev), dev);
    }

post_realize_fail:
    if (dc->unrealize) {
        dc->unrealize(dev, NULL);
    }

fail:
    error_propagate(errp, local_err);
    return;
}

static bool device_get_hotpluggable(VeertuType *obj, Error **errp)
{
    DeviceClass *dc = DEVICE_GET_CLASS(obj);
    DeviceState *dev = DEVICE(obj);

    return dc->hotpluggable && (dev->parent_bus == NULL ||
                                qbus_is_hotpluggable(dev->parent_bus));
}

static bool device_get_hotplugged(VeertuType *obj, Error **err)
{
    DeviceState *dev = DEVICE(obj);

    return dev->hotplugged;
}

static void device_set_hotplugged(VeertuType *obj, bool value, Error **err)
{
    DeviceState *dev = DEVICE(obj);

    dev->hotplugged = value;
}

static void device_initfn(VeertuType *obj)
{
    DeviceState *dev = DEVICE(obj);
    VeertuTypeClassHold *class;
    Property *prop;

    if (qdev_hotplug) {
        dev->hotplugged = 1;
        qdev_hot_added = true;
    }

    dev->instance_id_alias = -1;
    dev->realized = false;


    QLIST_INIT(&dev->gpios);
}

static void device_post_init(VeertuType *obj)
{
    Error *err = NULL;
    qdev_prop_set_globals(DEVICE(obj), &err);
    if (err) {
        qerror_report_err(err);
        error_free(err);
        exit(EXIT_FAILURE);
    }
}

/* Unlink device from bus and free the structure.  */
static void device_finalize(VeertuType *obj)
{
    NamedGPIOList *ngl, *next;

    DeviceState *dev = DEVICE(obj);
    vmx_opts_del(dev->opts);

    QLIST_FOREACH_SAFE(ngl, &dev->gpios, node, next) {
        QLIST_REMOVE(ngl, node);
        vmx_free_irqs(ngl->in, ngl->num_in);
        g_free(ngl->name);
        g_free(ngl);
        /* ngl->out irqs are owned by the other end and should not be freed
         * here
         */
    }
}

static void device_class_base_init(VeertuTypeClassHold *class, void *data)
{
    DeviceClass *klass = DEVICE_CLASS(class);
}

void device_unparent(DeviceState *dev, Error** errp)
{
    if (dev->realized) {
        device_set_realized(dev, false, errp);
    }

    if (dev->parent_bus) {
        bus_remove_child(dev->parent_bus, dev);
        dev->parent_bus = NULL;
    }
}

static void device_class_init(VeertuTypeClassHold *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);

    dc->realize = device_realize;
    dc->unrealize = device_unrealize;

    /* by default all devices were considered as hotpluggable,
     * so with intent to check it in generic qdev_unplug() /
     * device_set_realized() functions make every device
     * hotpluggable. Devices that shouldn't be hotpluggable,
     * should override it in their class_init()
     */
    dc->hotpluggable = true;
}

void device_reset(DeviceState *dev)
{
    DeviceClass *klass = DEVICE_GET_CLASS(dev);

    if (klass->reset) {
        klass->reset(dev);
    }
}

VeertuType *qdev_get_machine(void)
{
    return NULL;
}

static const VeertuTypeInfo device_type_info = {
    .name = TYPE_DEVICE,
    .parent = VtypeBase,
    .instance_size = sizeof(DeviceState),
    .instance_init = device_initfn,
    .class_init = device_class_init,
    .class_size = sizeof(DeviceClass),
};

static void qbus_initfn(VeertuType *obj)
{
    BusState *bus = BUS(obj);

    QTAILQ_INIT(&bus->children);
}

static char *default_bus_get_fw_dev_path(DeviceState *dev)
{
    return g_strdup(get_typename(VeertuTypeHold(dev)));
}

static void bus_class_init(VeertuTypeClassHold *class, void *data)
{
    BusClass *bc = BUS_CLASS(class);
    bc->get_fw_dev_path = default_bus_get_fw_dev_path;
}

static void qbus_finalize(VeertuType *obj)
{
    BusState *bus = BUS(obj);

    g_free((char *)bus->name);
}

static const VeertuTypeInfo bus_info = {
    .name = TYPE_BUS,
    .parent = VtypeBase,
    .instance_size = sizeof(BusState),
    .class_size = sizeof(BusClass),
    .instance_init = qbus_initfn,
    .class_init = bus_class_init,
};

void qdev_register_types(void)
{
    register_type_internal(&bus_info);
    register_type_internal(&device_type_info);
}
