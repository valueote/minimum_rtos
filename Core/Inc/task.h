#ifndef __TASK_H
#define __TASK_H

#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include "mem.h"

#define INITIAL_XPSR            ( 0x01000000 )
#define START_ADDRESS_MASK      ( 0xfffffffeUL )


typedef struct tcb{
    uint32_t* stack_top;
    uint32_t priority;
    uint32_t* stack;
}tcb_t;
typedef void (* task_func_t)( void * );
typedef tcb_t *task_handler_t;

void create_task_static(task_func_t func, void *func_parameters,
                       uint32_t stack_depth, uint32_t stack, tcb_t *tcb);
void create_task(task_func_t func, void *func_parameters, uint32_t stack_depth,
                 uint32_t priority, task_handler_t * handler);
void initialize_task();
uint32_t* initialize_stack(uint32_t* stack_top, task_func_t func,void* parameters);

#endif
