#include "sysemu.h"
#include "veertuemu.h"
#include "qom/cpu.h"
#include "qemu-common.h"

void default_cpu_reset(CPUState *cpu_state)
{
    cpu_state->interrupt_request = 0;
    cpu_state->can_do_io = 0;
    cpu_state->halted = 0;
}

bool default_cpu_has_work(CPUState *cpu_state)
{
    return 0;
}

void cpu_class_init(VeertuTypeClassHold *class, void *data)
{
    CPUClass *cpuclass = CPU_CLASS(class);
    DeviceClass *deviceclass = DEVICE_CLASS(class);
    cpuclass->has_work = default_cpu_has_work;
    cpuclass->reset = default_cpu_reset;
    deviceclass->cannot_instantiate_with_device_add_yet = 1;
}

VeertuTypeInfo cpu_type_info = {
    .name = "vcpu",
    .parent = TYPE_DEVICE,
    .class_init = cpu_class_init,
    .class_size = sizeof(CPUClass),
    .instance_size = sizeof(CPUState),
};

void cpu_register_types(void)
{
    register_type_internal(&cpu_type_info);
}