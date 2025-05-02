#ifndef __CONFIG_H
#define __CONFIG_H

#define TRUE 1
#define FALSE 0

#define alignment_byte 0x07
#define MAX_DELAY 0xffffffffUL

#define configHeapSize 32 * 1023
#define configMaxPriority 32
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191
#define configMAX_TASK_NAME_LEN 30
#define configSTACK_GUARD_SIZE (uint32_t)4
#define configSTACK_GUARD_MAGIC (uint32_t)0xdeadbeee
#define configSysTickClockHz ((unsigned long)7200000)
#define configSysTickClockRateHz ((uint32_t)1000)

#endif
