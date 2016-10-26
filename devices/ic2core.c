/*
 * QEMU I2C bus interface.
 *
 * Copyright (c) 2007 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licensed under the LGPL.
 */

#include "i2c.h"

struct I2CBus
{
    BusState qbus;
    I2CSlave *current_dev;
    I2CSlave *dev;
    uint8_t saved_address;
};

#define TYPE_I2C_BUS "i2c-bus"
#define I2C_BUS(obj) obj

static const VeertuTypeInfo i2c_bus_info = {
    .name = TYPE_I2C_BUS,
    .parent = TYPE_BUS,
    .instance_size = sizeof(I2CBus),
};

static void i2c_bus_pre_save(void *opaque)
{
    I2CBus *bus = opaque;

    bus->saved_address = bus->current_dev ? bus->current_dev->address : -1;
}

static int i2c_bus_post_load(void *opaque, int version_id)
{
    I2CBus *bus = opaque;

    /* The bus is loaded before attached devices, so load and save the
       current device id.  Devices will check themselves as loaded.  */
    bus->current_dev = NULL;
    return 0;
}

static const VMStateDescription vmstate_i2c_bus = {
    .name = "i2c_bus",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = i2c_bus_pre_save,
    .post_load = i2c_bus_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(saved_address, I2CBus),
        VMSTATE_END_OF_LIST()
    }
};

/* Create a new I2C bus.  */
I2CBus *i2c_init_bus(DeviceState *parent, const char *name)
{
    I2CBus *bus;

    bus = I2C_BUS(qbus_create(TYPE_I2C_BUS, parent, name));
    vmstate_register(NULL, -1, &vmstate_i2c_bus, bus);
    return bus;
}

void i2c_set_slave_address(I2CSlave *dev, uint8_t address)
{
    dev->address = address;
}

/* Return nonzero if bus is busy.  */
int i2c_bus_busy(I2CBus *bus)
{
    return bus->current_dev != NULL;
}

/* Returns non-zero if the address is not valid.  */
/* TODO: Make this handle multiple masters.  */
int i2c_start_transfer(I2CBus *bus, uint8_t address, int recv)
{
    BusChild *kid;
    I2CSlave *slave = NULL;
    I2CSlaveClass *sc;

    QTAILQ_FOREACH(kid, &bus->qbus.children, sibling) {
        DeviceState *qdev = kid->child;
        I2CSlave *candidate = I2C_SLAVE(qdev);
        if (candidate->address == address) {
            slave = candidate;
            break;
        }
    }

    if (!slave) {
        return 1;
    }

    sc = I2C_SLAVE_GET_CLASS(slave);
    /* If the bus is already busy, assume this is a repeated
       start condition.  */
    bus->current_dev = slave;
    if (sc->event) {
        sc->event(slave, recv ? I2C_START_RECV : I2C_START_SEND);
    }
    return 0;
}

void i2c_end_transfer(I2CBus *bus)
{
    I2CSlave *dev = bus->current_dev;
    I2CSlaveClass *sc;

    if (!dev) {
        return;
    }

    sc = I2C_SLAVE_GET_CLASS(dev);
    if (sc->event) {
        sc->event(dev, I2C_FINISH);
    }

    bus->current_dev = NULL;
}

int i2c_send(I2CBus *bus, uint8_t data)
{
    I2CSlave *dev = bus->current_dev;
    I2CSlaveClass *sc;

    if (!dev) {
        return -1;
    }

    sc = I2C_SLAVE_GET_CLASS(dev);
    if (sc->send) {
        return sc->send(dev, data);
    }

    return -1;
}

int i2c_recv(I2CBus *bus)
{
    I2CSlave *dev = bus->current_dev;
    I2CSlaveClass *sc;

    if (!dev) {
        return -1;
    }

    sc = I2C_SLAVE_GET_CLASS(dev);
    if (sc->recv) {
        return sc->recv(dev);
    }

    return -1;
}

void i2c_nack(I2CBus *bus)
{
    I2CSlave *dev = bus->current_dev;
    I2CSlaveClass *sc;

    if (!dev) {
        return;
    }

    sc = I2C_SLAVE_GET_CLASS(dev);
    if (sc->event) {
        sc->event(dev, I2C_NACK);
    }
}

static int i2c_slave_post_load(void *opaque, int version_id)
{
    I2CSlave *dev = opaque;
    I2CBus *bus;
    bus = I2C_BUS(qdev_get_parent_bus(DEVICE(dev)));
    if (bus->saved_address == dev->address) {
        bus->current_dev = dev;
    }
    return 0;
}

const VMStateDescription vmstate_i2c_slave = {
    .name = "I2CSlave",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = i2c_slave_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(address, I2CSlave),
        VMSTATE_END_OF_LIST()
    }
};

static int i2c_slave_qdev_init(DeviceState *dev)
{
    I2CSlave *s = I2C_SLAVE(dev);
    I2CSlaveClass *sc = I2C_SLAVE_GET_CLASS(s);

    return sc->init(s);
}

DeviceState *i2c_create_slave(I2CBus *bus, const char *name, uint8_t addr)
{
    DeviceState *dev;
    I2CSlave *s;

    dev = qdev_create(&bus->qbus, name);
    s = I2C_SLAVE(dev);
    s->address = addr;
    qdev_init_nofail(dev);
    return dev;
}

static void i2c_slave_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);
    k->init = i2c_slave_qdev_init;
    set_bit(DEVICE_CATEGORY_MISC, k->categories);
    k->bus_type = TYPE_I2C_BUS;
}

static const VeertuTypeInfo i2c_slave_type_info = {
    .name = TYPE_I2C_SLAVE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(I2CSlave),
    .class_size = sizeof(I2CSlaveClass),
    .class_init = i2c_slave_class_init,
};

void i2c_slave_register_types(void)
{
    register_type_internal(&i2c_bus_info);
    register_type_internal(&i2c_slave_type_info);
}