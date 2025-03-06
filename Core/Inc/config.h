#ifndef __CONFIG_H
#define __CONFIG_H

#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#define alignment_byte 0x07
#define configHeapSize 16*1024
#define configMaxPriority 10

#define configMAX_SYSCALL_INTERRUPT_PRIORITY 0
#endif
