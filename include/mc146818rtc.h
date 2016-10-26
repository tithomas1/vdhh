#ifndef MC146818RTC_H
#define MC146818RTC_H

#include "isa.h"
#include "mc146818rtc_regs.h"

#define TYPE_MC146818_RTC "mc146818rtc"

ISADevice *rtc_init(ISABus *bus, int base_year, vmx_irq intercept_irq);
void rtc_set_memory(ISADevice *dev, int addr, int val);
int rtc_get_memory(ISADevice *dev, int addr);

#endif /* !MC146818RTC_H */
