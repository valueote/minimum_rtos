#ifndef __CONFIG_H
#define __CONFIG_H

#define alignment_byte 0x07
#define MAX_DELAY 0xffffffffUL
#define configHeapSize 16 * 1024

#define configMaxPriority 32
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191
#define configSysTickClockHz ((unsigned long)7200000)
#define configSysTickClockRateHz ((uint32_t)1000)

#endif
