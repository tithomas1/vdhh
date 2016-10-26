#ifndef QEMU_IRQ_H
#define QEMU_IRQ_H

/* Generic IRQ/GPIO pin infrastructure.  */

#define TYPE_IRQ "irq"

typedef struct IRQState *vmx_irq;

typedef void (*vmx_irq_handler)(void *opaque, int n, int level);

void vmx_set_irq(vmx_irq irq, int level);

static inline void vmx_irq_raise(vmx_irq irq)
{
    vmx_set_irq(irq, 1);
}

static inline void vmx_irq_lower(vmx_irq irq)
{
    vmx_set_irq(irq, 0);
}

static inline void vmx_irq_pulse(vmx_irq irq)
{
    vmx_set_irq(irq, 1);
    vmx_set_irq(irq, 0);
}

/* Returns an array of N IRQs. Each IRQ is assigned the argument handler and
 * opaque data.
 */
vmx_irq *vmx_allocate_irqs(vmx_irq_handler handler, void *opaque, int n);

/*
 * Allocates a single IRQ. The irq is assigned with a handler, an opaque
 * data and the interrupt number.
 */
vmx_irq vmx_allocate_irq(vmx_irq_handler handler, void *opaque, int n);

/* Extends an Array of IRQs. Old IRQs have their handlers and opaque data
 * preserved. New IRQs are assigned the argument handler and opaque data.
 */
vmx_irq *vmx_extend_irqs(vmx_irq *old, int n_old, vmx_irq_handler handler,
                                void *opaque, int n);

void vmx_free_irqs(vmx_irq *s, int n);
void vmx_free_irq(vmx_irq irq);

/* Returns a new IRQ with opposite polarity.  */
vmx_irq vmx_irq_invert(vmx_irq irq);

/* Returns a new IRQ which feeds into both the passed IRQs */
vmx_irq vmx_irq_split(vmx_irq irq1, vmx_irq irq2);

/* Returns a new IRQ set which connects 1:1 to another IRQ set, which
 * may be set later.
 */
vmx_irq *vmx_irq_proxy(vmx_irq **target, int n);

/* For internal use in qtest.  Similar to vmx_irq_split, but operating
   on an existing vector of vmx_irq.  */
void vmx_irq_intercept_in(vmx_irq *gpio_in, vmx_irq_handler handler, int n);

#endif
