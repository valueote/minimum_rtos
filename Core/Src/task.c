#include "task.h"
#include "config.h"
#include "core_cm3.h"
#include "list.h"
#include "mem.h"
#include <stdint.h>

// The current running task
__attribute__((used)) tcb_t *volatile current_tcb = NULL;
// handler for the idle task
static task_handler_t idle_task_handler = NULL;
// ready lists
static list_t ready_lists[configMaxPriority];
// ready bits for task table
uint32_t ready_bits = 0;
// zombie list
static list_t zombie_list;
// suspended_list
static list_t suspended_list;
// delay lists
static list_t actual_delay_list;
static list_t actual_delay_overflow_list;
static list_t *delay_list = NULL;
static list_t *delay_overflow_list = NULL;
// tick count
static uint32_t current_tick_count = 0;
// scheduelr suspended or not
static uint32_t scheduler_is_suspending = 0;
// Static functions

static void ready_lists_init(void);
static void add_to_ready_lists(tcb_t *handler, uint32_t priority);
static uint32_t *stack_init(uint32_t *stack_top, task_func_t func,
                            void *parameters);
static inline uint8_t get_highest_priority(void);
static void delay_list_init(void);
static void increment_tick(void);

// Used for context switch, from freertos
__attribute__((naked)) void xPortPendSVHandler(void) {
  __asm volatile("   mrs r0, psp                         \n"
                 "   isb                                 \n"
                 "                                       \n"
                 "   ldr r3, pxCurrentTCBConst           \n"
                 "   ldr r2, [r3]                        \n"
                 "                                       \n"
                 "   stmdb r0!, {r4-r11}                 \n"
                 "   str r0, [r2]                        \n"
                 "                                       \n"
                 "   stmdb sp!, {r3, r14}                \n"
                 "   mov r0, %0                          \n"
                 "   msr basepri, r0                     \n"
                 "   bl vTaskSwitchContext               \n"
                 "   mov r0, #0                          \n"
                 "   msr basepri, r0                     \n"
                 "   ldmia sp!, {r3, r14}                \n"
                 "                                       \n"
                 "   ldr r1, [r3]                        \n"
                 "   ldr r0, [r1]                        \n"
                 "   ldmia r0!, {r4-r11}                 \n"
                 "   msr psp, r0                         \n"
                 "   isb                                 \n"
                 "   bx r14                              \n"
                 "                                       \n"
                 "   .align 4                            \n"
                 "pxCurrentTCBConst: .word current_tcb  \n" ::"i"(
                     configMAX_SYSCALL_INTERRUPT_PRIORITY));
}

// SVC handler
__attribute__((naked)) void vPortSVCHandler(void) {
  __asm volatile(
      "   ldr r3, pxCurrentTCBConst2      \n" /* Restore the context. */
      "   ldr r1, [r3]                    \n" /* Use pxCurrentTCBConst to get
                                                 the pxCurrentTCB address. */
      "   ldr r0, [r1]                    \n" /* The first item in pxCurrentTCB
                                                 is the task top of stack. */
      "   ldmia r0!, {r4-r11}             \n" /* Pop the registers that are not
                                                 automatically saved on
                                                 exception entry and the
                                                 critical nesting count. */
      "   msr psp, r0                     \n" /* Restore the task stack pointer.
                                               */
      "   isb                             \n"
      "   mov r0, #0                      \n"
      "   msr basepri, r0                 \n"
      "   orr r14, #0xd                   \n"
      "   bx r14                          \n"
      "                                   \n"
      "   .align 4                        \n"
      "pxCurrentTCBConst2: .word current_tcb             \n");
}

// SysTick Handler
void SysTick_Handler(void) {
  uint32_t saved = critical_enter();
  if (scheduler_is_suspending == FALSE) {
    increment_tick();
  }
  critical_exit(saved);
}
// And config the SysTick and start the first task
__attribute__((always_inline)) inline static void StartFirstTask(void) {
  (*((volatile uint32_t *)0xe000ed20)) |= (((uint32_t)255UL) << 16UL);
  (*((volatile uint32_t *)0xe000ed20)) |= (((uint32_t)255UL) << 24UL);
  SysTick->CTRL = 0UL;
  SysTick->VAL = 0UL;
  SysTick->LOAD = (configSysTickClockHz / configSysTickClockRateHz) - 1UL;
  SysTick->CTRL = ((1UL << 2UL) | (1UL << 1UL) | (1UL << 0UL));
  __asm volatile(
      " ldr r0, =0xE000ED08   \n" /* Use the NVIC offset register to locate the
                                     stack. */
      " ldr r0, [r0]          \n"
      " ldr r0, [r0]          \n"
      " msr msp, r0           \n" /* Set the msp back to the start of the stack.
                                   */
      " cpsie i               \n" /* Globally enable interrupts. */
      " cpsie f               \n"
      " dsb                   \n"
      " isb                   \n"
      " svc 0                 \n" /* System call to start first task. */
      " nop                   \n"
      " .ltorg                \n");
}

/* Task API part
 */

// Create a new task
void task_create(task_func_t func, void *func_parameters, uint32_t stack_depth,
                 uint32_t priority, task_handler_t *handler) {
  tcb_t *new_tcb;
  uint32_t *stack_top;
  // allocate memory for the tcb and stack
  new_tcb = (tcb_t *)halloc(sizeof(tcb_t));
  new_tcb->stack = (uint32_t *)halloc((size_t)stack_depth * sizeof(uint32_t));
  // get the stack top addresss and align
  stack_top = new_tcb->stack + (stack_depth - (uint32_t)1);
  stack_top = (uint32_t *)((uint32_t)stack_top & ~(uint32_t)(alignment_byte));
  // initialize the stack
  new_tcb->stack_top = stack_init(stack_top, func, func_parameters);
  new_tcb->priority = priority;
  // put the tcb into ready_lists
  list_node_init(&(new_tcb->state_node));
  new_tcb->state_node.owner = new_tcb;
  // set the task handler
  *handler = (task_handler_t)new_tcb;
  // add the new tcb to the ready list
  add_to_ready_lists(new_tcb, priority);
}

// delete task
void task_delete(task_handler_t *handler) {
  tcb_t *tcb;
  uint32_t yield = FALSE;
  uint32_t saved = critical_enter();

  if (handler == NULL) {
    tcb = current_tcb;
    yield = TRUE;
  } else {
    tcb = *handler;
  }

  list_remove_node(&(tcb->state_node));
  if (tcb == current_tcb) {
    list_insert_end(&zombie_list, &(tcb->state_node));
  } else {
    hfree(tcb->stack);
    hfree(tcb);
  }

  critical_exit(saved);

  if (yield) {
    task_switch();
  }
}

// Delay current task for given ticks
void task_delay(uint32_t ticks) {
  uint32_t saved = critical_enter();

  uint32_t time_to_wake = ticks + current_tick_count;
  current_tcb->state_node.val = time_to_wake;
  uint32_t priority = current_tcb->priority;
  list_t *prev_ready_list = &ready_lists[priority];

  list_remove_node(&(current_tcb->state_node));
  // time_to wake overflow, put it in the overflow list
  if (time_to_wake < current_tick_count) {
    list_insert_node(delay_overflow_list, &(current_tcb->state_node));
  } else {
    list_insert_node(delay_list, &(current_tcb->state_node));
  }

  if (list_is_empty(prev_ready_list))
    ready_bits = ready_bits & (~(1 << priority));

  critical_exit(saved);
  task_switch();
}

// Suspend the task, if the handler is NULL, suspend the current running task
void task_suspend(task_handler_t *handler) {
  tcb_t *tcb;
  uint32_t yield = FALSE;
  uint32_t saved = critical_enter();

  if (handler == NULL) {
    tcb = current_tcb;
    yield = TRUE;
  } else {
    tcb = *handler;
  }

  list_remove_node(&(tcb->state_node));
  if (list_is_empty(&(ready_lists[tcb->priority]))) {
    ready_bits &= ~(1 << tcb->priority);
  }
  list_insert_end(&suspended_list, &(tcb->state_node));

  critical_exit(saved);
  // we suspend current running tcb, so switch to another task
  if (yield)
    task_switch();
}

void task_resume(task_handler_t *handler) {
  tcb_t *tcb = *handler;
  if (tcb != NULL && tcb != current_tcb) {
    uint32_t saved = critical_enter();

    list_remove_node(&(tcb->state_node));
    list_insert_node(&(ready_lists[tcb->priority]), &(tcb->state_node));
    ready_bits |= (1 << tcb->priority);

    if (tcb->priority > current_tcb->priority) {
      task_switch();
    }

    critical_exit(saved);
  }
}

// Get the current running tcb
tcb_t *get_current_tcb(void) { return current_tcb; }

// Add the task handler to the ready list
static void add_to_ready_lists(tcb_t *tcb, uint32_t priority) {
  uint32_t ret = critical_enter();

  ready_bits |= (1 << priority);
  list_insert_end(&ready_lists[priority], &(tcb->state_node));

  critical_exit(ret);
}

// Task should never return, if so, it will reach here
static void task_exit_error() {
  while (1) {
  }
}

// Init the task stack
static uint32_t *stack_init(uint32_t *stack_top, task_func_t func,
                            void *parameters) {
  // set the XPSR
  stack_top--;
  *stack_top = INITIAL_XPSR;
  // set the task func
  stack_top--;
  *stack_top = (uint32_t)func & (uint32_t)START_ADDRESS_MASK;
  // set the error handler
  stack_top--;
  *stack_top = (uint32_t)task_exit_error;
  // set the r12, r1-3 to zero, and set r0 to parameters
  stack_top -= 5;
  *stack_top = (uint32_t)parameters;
  // leave space for r4-r11
  stack_top -= 8;
  return stack_top;
}

void idle_task() {
  while (1) {
    uint32_t saved = critical_enter();
    while (!list_is_empty(&zombie_list)) {
      list_node_t *node = list_get_next_index(&zombie_list);
      tcb_t *tcb = node->owner;
      hfree(tcb->stack);
      hfree(tcb);
    }
    critical_exit(saved);
  }
}

/*Scheduler API
 */

// Init the scheduler
void scheduler_init(void) {
  current_tick_count = 0;
  ready_lists_init();
  delay_list_init();
  list_init(&suspended_list);
  list_init(&zombie_list);
  task_create(idle_task, NULL, 128, 0, &idle_task_handler);
}

// Start
void scheduler_start(void) {
  current_tcb = idle_task_handler;
  StartFirstTask();
}

// NOT FINISHED suspended the scheduler
void scheduler_suspend(void) {
  uint32_t saved = critical_enter();
  scheduler_is_suspending++;
  critical_exit(saved);
}
// NOT FINISHED
void scheduler_resume(void) {
  uint32_t saved = critical_enter();
  scheduler_is_suspending--;
  if (scheduler_is_suspending == 0) {
  }
  critical_exit(saved);
  return;
}

// Switch task
void vTaskSwitchContext(void) {
  uint8_t priority = get_highest_priority();
  if (priority == configMaxPriority) {
    current_tcb = idle_task_handler;
    return;
  }

  list_t *list = &ready_lists[priority];
  list_node_t *next_node = list_get_next_index(list);
  current_tcb = next_node->owner;
}

__attribute__((always_inline)) inline uint32_t critical_enter(void) {
  uint32_t saved;
  uint32_t temp;
  __asm volatile(" cpsid i               \n"
                 " mrs %0, basepri       \n"
                 " mov %1, %2            \n"
                 " msr basepri, %1       \n"
                 " dsb                   \n"
                 " isb                   \n"
                 " cpsie i               \n"
                 : "=r"(saved), "=r"(temp)
                 : "r"(configMAX_SYSCALL_INTERRUPT_PRIORITY)
                 : "memory");
  return saved;
}

__attribute__((always_inline)) inline void critical_exit(uint32_t saved) {
  __asm volatile(" cpsid i               \n"
                 " msr basepri, %0       \n"
                 " dsb                   \n"
                 " isb                   \n"
                 " cpsie i               \n" ::"r"(saved)
                 : "memory");
}

__attribute__((always_inline)) static inline uint8_t
get_highest_priority(void) {
  uint8_t top_zero;
  uint8_t temp;

  __asm volatile("clz %0, %2\n"
                 "mov %1, #31\n"
                 "sub %0, %1, %0\n"
                 : "=r"(top_zero), "=r"(temp)
                 : "r"(ready_bits));
  return top_zero;
}

static void ready_lists_init(void) {
  for (int i = 0; i < configMaxPriority; i++) {
    list_init(&(ready_lists[i]));
  }
}

static void delay_list_init(void) {
  list_init(&actual_delay_list);
  list_init(&actual_delay_overflow_list);
  delay_list = &actual_delay_list;
  delay_overflow_list = &actual_delay_overflow_list;
}

void delay_list_switch(void) {
  list_t *tmp;
  tmp = delay_list;
  delay_list = delay_overflow_list;
  delay_overflow_list = tmp;
}

static void increment_tick(void) {
  current_tick_count += 1;

  // the tick count have overflowed
  if (current_tick_count == 0) {
    delay_list_switch();
  }

  for (;;) {
    if (list_is_empty(delay_list)) {
      break;
    }
    list_node_t *first_node = delay_list->head.next;
    if (first_node->val <= current_tick_count) {
      tcb_t *tcb = (tcb_t *)(first_node->owner);
      list_remove_node(first_node);

      list_insert_node(&(ready_lists[tcb->priority]), first_node);
      ready_bits |= (1 << tcb->priority);
    } else {
      break;
    }
  }
  task_switch();
}
