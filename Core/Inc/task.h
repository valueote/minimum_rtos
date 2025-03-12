#ifndef __TASK_H
#define __TASK_H

#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include "mem.h"

#define INITIAL_XPSR            ( 0x01000000 )
#define START_ADDRESS_MASK      ( 0xfffffffeUL )
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define task_switch()  *( ( volatile uint32_t * ) 0xe000ed04 ) = 1UL << 28UL


typedef struct tcb{
    uint32_t* stack_top;
    uint32_t priority;
    uint32_t* stack;
}tcb_t;
typedef void (* task_func_t)( void * );
typedef tcb_t *task_handler_t;

void task_create_static(task_func_t func, void *func_parameters,
                       uint32_t stack_depth, uint32_t stack, tcb_t *tcb);
void task_create(task_func_t func, void *func_parameters, uint32_t stack_depth,
                 uint32_t priority, task_handler_t * handler);

uint32_t* stack_init(uint32_t* stack_top, task_func_t func,void* parameters);

void scheduler_init(void);
void scheduler_start(void);
uint32_t critical_enter(void);
void critical_exit(uint32_t ret);


#endif
