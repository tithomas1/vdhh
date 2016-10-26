#ifndef QDEV_CORE_H
#define QDEV_CORE_H

#include "qemu/queue.h"
#include "qemu/option.h"
#include "qemu/typedefs.h"
#include "qemu/bitmap.h"
#include "typeinfo.h"
#include "irq.h"
#include "qapi/error.h"

enum {
    DEV_NVECTORS_UNSPECIFIED = -1,
};

#define TYPE_DEVICE "device"
#define DEVICE(obj) ((DeviceState *)(obj))
#define DEVICE_CLASS(klass) klass
#define DEVICE_GET_CLASS(obj) ((VeertuType *)obj)->class

typedef enum DeviceCategory {
    DEVICE_CATEGORY_BRIDGE,
    DEVICE_CATEGORY_USB,
    DEVICE_CATEGORY_STORAGE,
    DEVICE_CATEGORY_NETWORK,
    DEVICE_CATEGORY_INPUT,
    DEVICE_CATEGORY_DISPLAY,
    DEVICE_CATEGORY_SOUND,
    DEVICE_CATEGORY_MISC,
    DEVICE_CATEGORY_MAX
} DeviceCategory;

typedef int (*qdev_initfn)(DeviceState *dev);
typedef int (*qdev_event)(DeviceState *dev);
typedef void (*qdev_resetfn)(DeviceState *dev);
typedef void (*DeviceRealize)(DeviceState *dev, Error **errp);
typedef void (*DeviceUnrealize)(DeviceState *dev, Error **errp);
typedef void (*BusRealize)(BusState *bus, Error **errp);
typedef void (*BusUnrealize)(BusState *bus, Error **errp);

struct VMStateDescription;

/**
 * DeviceClass:
 * @props: Properties accessing state fields.
 * @realize: Callback function invoked when the #DeviceState:realized
 * property is changed to %true. The default invokes @init if not %NULL.
 * @unrealize: Callback function invoked when the #DeviceState:realized
 * property is changed to %false.
 * @init: Callback function invoked when the #DeviceState::realized property
 * is changed to %true. Deprecated, new types inheriting directly from
 * TYPE_DEVICE should use @realize instead, new leaf types should consult
 * their respective parent type.
 * @hotpluggable: indicates if #DeviceClass is hotpluggable, available
 * as readonly "hotpluggable" property of #DeviceState instance
 *
 * # Realization #
 * Devices are constructed in two stages,
 * 1) object instantiation via vtype_init() and
 * 2) device realization via #DeviceState:realized property.
 * The former may not fail (it might assert or exit), the latter may return
 * error information to the caller and must be re-entrant.
 * Trivial field initializations should go into #VeertuTypeInfo.instance_init.
 * Operations depending on @props static properties should go into @realize.
 * After successful realization, setting static properties will fail.
 *
 * As an interim step, the #DeviceState:realized property is set by deprecated
 * functions qdev_init() and qdev_init_nofail().
 * In the future, devices will propagate this state change to their children
 * and along busses they expose.
 * The point in time will be deferred to machine creation, so that values
 * set in @realize will not be introspectable beforehand. Therefore devices
 * must not create children during @realize; they should initialize them via
 * vtype_init() in their own #VeertuTypeInfo.instance_init and forward the
 * realization events appropriately.
 *
 * The @init callback is considered private to a particular bus implementation
 * (immediate abstract child types of TYPE_DEVICE). Derived leaf types set an
 * "init" callback on their parent class instead.
 *
 * Any type may override the @realize and/or @unrealize callbacks but needs
 * to call the parent type's implementation if keeping their functionality
 * is desired. Refer to QOM documentation for further discussion and examples.
 *
 * <note>
 *   <para>
 * If a type derived directly from TYPE_DEVICE implements @realize, it does
 * not need to implement @init and therefore does not need to store and call
 * #DeviceClass' default @realize callback.
 * For other types consult the documentation and implementation of the
 * respective parent types.
 *   </para>
 * </note>
 */
typedef struct DeviceClass {
    /*< private >*/
    VeertuTypeClassHold parent_class;
    /*< public >*/

    DECLARE_BITMAP(categories, DEVICE_CATEGORY_MAX);
    const char *fw_name;
    const char *desc;

    /*
     * Shall we hide this device model from -device / device_add?
     * All devices should support instantiation with device_add, and
     * this flag should not exist.  But we're not there, yet.  Some
     * devices fail to instantiate with cryptic error messages.
     * Others instantiate, but don't work.  Exposing users to such
     * behavior would be cruel; this flag serves to protect them.  It
     * should never be set without a comment explaining why it is set.
     * TODO remove once we're there
     */
    bool cannot_instantiate_with_device_add_yet;
    bool hotpluggable;

    /* callbacks */
    void (*reset)(DeviceState *dev);
    DeviceRealize realize;
    DeviceUnrealize unrealize;

    /* device state */
    const struct VMStateDescription *vmsd;

    /* Private to qdev / bus.  */
    qdev_initfn init; /* TODO remove, once users are converted to realize */
    qdev_event exit; /* TODO remove, once users are converted to unrealize */
    const char *bus_type;
} DeviceClass;

typedef struct NamedGPIOList NamedGPIOList;

struct NamedGPIOList {
    char *name;
    vmx_irq *in;
    int num_in;
    int num_out;
    QLIST_ENTRY(NamedGPIOList) node;
};

/**
 * DeviceState:
 * @realized: Indicates whether the device has been fully constructed.
 *
 * This structure should not be accessed directly.  We declare it here
 * so that it can be embedded in individual device state structures.
 */
struct DeviceState {
    /*< private >*/
    VeertuType parent;
    /*< public >*/

    const char *id;
    bool realized;
    bool pending_deleted_event;
    QemuOpts *opts;
    int hotplugged;
    BusState *parent_bus;
    QLIST_HEAD(, NamedGPIOList) gpios;
    QLIST_HEAD(, BusState) child_bus;
    int num_child_bus;
    int instance_id_alias;
    int alias_required_for_version;
};

#define TYPE_BUS "bus"
#define BUS(obj) obj
#define BUS_CLASS(klass) klass
#define BUS_GET_CLASS(obj) ((VeertuType *)obj)->class

struct BusClass {
    VeertuTypeClassHold parent_class;

    /* FIXME first arg should be BusState */
    void (*print_dev)(Monitor *mon, DeviceState *dev, int indent);
    char *(*get_dev_path)(DeviceState *dev);
    /*
     * This callback is used to create Open Firmware device path in accordance
     * with OF spec http://forthworks.com/standards/of1275.pdf. Individual bus
     * bindings can be found at http://playground.sun.com/1275/bindings/.
     */
    char *(*get_fw_dev_path)(DeviceState *dev);
    void (*reset)(BusState *bus);
    BusRealize realize;
    BusUnrealize unrealize;

    /* maximum devices allowed on the bus, 0: no limit. */
    int max_dev;
    /* number of automatically allocated bus ids (e.g. ide.0) */
    int automatic_ids;
};

typedef struct BusChild {
    DeviceState *child;
    int index;
    QTAILQ_ENTRY(BusChild) sibling;
} BusChild;

#define QDEV_HOTPLUG_HANDLER_PROPERTY "hotplug-handler"

typedef void (*DeviceEventMonitor)(DeviceState* dev, int event, void* opaque);

/**
 * BusState:
 * @hotplug_device: link to a hotplug device associated with bus.
 */
struct BusState {
    VeertuType obj;
    DeviceState *parent;
    const char *name;
    int max_index;
    bool realized;
    DeviceEventMonitor monitor;
    void* monitor_opaque;
    QTAILQ_HEAD(ChildrenHead, BusChild) children;
    QLIST_ENTRY(BusState) sibling;
};

/**
 * GlobalProperty:
 * @user_provided: Set to true if property comes from user-provided config
 * (command-line or config file).
 * @used: Set to true if property was used when initializing a device.
 */
typedef struct GlobalProperty {
    const char *driver;
    const char *property;
    const char *value;
    bool user_provided;
    bool used;
    QTAILQ_ENTRY(GlobalProperty) next;
} GlobalProperty;

/*** Board API.  This should go away once we have a machine config file.  ***/

DeviceState *qdev_create(BusState *bus, const char *name);
DeviceState *qdev_try_create(BusState *bus, const char *name);
int qdev_init(DeviceState *dev) QEMU_WARN_UNUSED_RESULT;
void qdev_init_nofail(DeviceState *dev);
void qdev_set_legacy_instance_id(DeviceState *dev, int alias_id,
                                 int required_for_version);
void qdev_on_machine_creation_done(void(*handler)(void*), void* opaque);
void qdev_machine_creation_done(void);
bool qdev_machine_modified(void);

void qdev_signal_event(DeviceState* dev, int event);
void qdev_set_event_monitor(BusState* bus, DeviceEventMonitor handler, void* opaque);

BusState *qdev_get_child_bus(DeviceState *dev, const char *name);



void qdev_pass_gpios(DeviceState *dev, DeviceState *container,
                     const char *name);

BusState *qdev_get_parent_bus(DeviceState *dev);

/*** BUS API. ***/

DeviceState *qdev_find_recursive(BusState *bus, const char *id);

/* Returns 0 to walk children, > 0 to skip walk, < 0 to terminate walk. */
typedef int (qbus_walkerfn)(BusState *bus, void *opaque);
typedef int (qdev_walkerfn)(DeviceState *dev, void *opaque);

void qbus_create_inplace(void *bus, size_t size, const char *typename,
                         DeviceState *parent, const char *name);
BusState *qbus_create(const char *typename, DeviceState *parent, const char *name);
/* Returns > 0 if either devfn or busfn skip walk somewhere in cursion,
 *         < 0 if either devfn or busfn terminate walk somewhere in cursion,
 *           0 otherwise. */
int qbus_walk_children(BusState *bus,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);
int qdev_walk_children(DeviceState *dev,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);

void qdev_reset_all(DeviceState *dev);

/**
 * @qbus_reset_all:
 * @bus: Bus to be reset.
 *
 * Reset @bus and perform a bus-level ("hard") reset of all devices connected
 * to it, including recursive processing of all buses below @bus itself.  A
 * hard reset means that qbus_reset_all will reset all state of the device.
 * For PCI devices, for example, this will include the base address registers
 * or configuration space.
 */
void qbus_reset_all(BusState *bus);
void qbus_reset_all_fn(void *opaque);

/* This should go away once we get rid of the NULL bus hack */
BusState *sysbus_get_default(void);

char *qdev_get_fw_dev_path(DeviceState *dev);

/**
 * @qdev_machine_init
 *
 * Initialize platform devices before machine init.  This is a hack until full
 * support for composition is added.
 */
void qdev_machine_init(void);

/**
 * @device_reset
 *
 * Reset a single device (by calling the reset method).
 */
void device_reset(DeviceState *dev);

const struct VMStateDescription *qdev_get_vmsd(DeviceState *dev);

const char *qdev_fw_name(DeviceState *dev);

VeertuType *qdev_get_machine(void);

/* FIXME: make this a link<> */
void qdev_set_parent_bus(DeviceState *dev, BusState *bus);

extern int qdev_hotplug;

char *qdev_get_dev_path(DeviceState *dev);

GSList *qdev_build_hotpluggable_device_list(VeertuType *peripheral);

void qbus_set_hotplug_handler(BusState *bus, DeviceState *handler,
                              Error **errp);

void qbus_set_bus_hotplug_handler(BusState *bus, Error **errp);

void device_unparent(DeviceState *obj, Error** errp);

static inline bool qbus_is_hotpluggable(BusState *bus)
{
   return /*false*/true;
}
#endif
