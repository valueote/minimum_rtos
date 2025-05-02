#ifndef __TASK_H
#define __TASK_H

#include "config.h"
#include "list.h"
#include "mutex.h"
#include "stm32f1xx.h"
#include <stddef.h>
#include <stdint.h>

#define INITIAL_XPSR (0x01000000)
#define START_ADDRESS_MASK (0xfffffffeUL)
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define task_yield() *((volatile uint32_t *)0xe000ed04) = 1UL << 28UL
#define task_yield_isr(yield)                                                  \
  do {                                                                         \
    if (yield) {                                                               \
      task_yield();                                                            \
    }                                                                          \
  } while (0)

typedef struct mutex mutex_t;

typedef struct tcb {
  uint32_t *stack_top;
  uint32_t priority;
  uint32_t base_priority;
  uint32_t *stack;
  list_node_t state_node;
  list_node_t event_node;
  char name[configMAX_TASK_NAME_LEN];
} tcb_t;

typedef struct block_timer {
  uint32_t start_tick;
} block_timer_t;

typedef void (*task_func_t)(void *);
typedef tcb_t *task_handler_t;

void task_create(task_func_t func, void *func_parameters, uint32_t stack_depth,
                 uint32_t priority, const char *const name,
                 task_handler_t *handler);
void task_delete(task_handler_t *handler);
void task_delay(uint32_t ticks);
void task_suspend(task_handler_t *handler);
void task_resume(task_handler_t *handler);
tcb_t *get_current_tcb(void);
uint32_t add_tcb_to_ready_lists(tcb_t *tcb);
void add_tcb_to_delay_list(tcb_t *tcb, uint32_t ticks);
uint32_t get_current_tick(void);
uint32_t task_resume_from_block(list_t *block_list);
void task_priority_inherit(mutex_t *mutex);
void task_priority_disinherit(mutex_t *mutex);
void task_priority_disinherit_timeout(mutex_t *mutex);

void scheduler_init(void);
void scheduler_start(void);
void scheduler_suspend(void);
void scheduler_resume(void);

uint32_t critical_enter(void);
void critical_exit(uint32_t saved);

uint32_t block_timer_set(block_timer_t *timer);
uint32_t block_timer_check(block_timer_t *timer, uint32_t *block_ticks);

list_t *get_ready_list(uint32_t priority);
#endif
