#include "task.h"
#include "config.h"
#include "core_cm3.h"
#include "list.h"
#include "mem.h"
#include "printf.h"
#include "stm32f103xe.h"
#include <stdint.h>

// The current running task
__attribute__((used)) tcb_t *volatile current_tcb = NULL;

// Task Handler for the idle task
static task_handler_t idle_task_handler = NULL;

// Task Ready lists
static list_t ready_lists[configMaxPriority];

// Ready bits for task table
uint32_t ready_bits = 0;

// Zombie task list
static list_t zombie_list;

// Suspended task list
static list_t suspended_list;

// Delay lists
static list_t actual_delay_list;
static list_t actual_delay_overflow_list;
static list_t *delay_list = NULL;
static list_t *delay_overflow_list = NULL;

// Current tick count
static uint32_t current_tick_count = 0;

// scheduelr suspended or not
static uint32_t scheduler_is_suspending = FALSE;
static uint32_t scheduler_is_running = FALSE;

// Static functions

// Init the stack of the new task
static uint32_t *stack_init(uint32_t *stack_top, task_func_t func,
                            void *parameters);

// Free the tcb of the deleted task
static void free_tcb(tcb_t *tcb);

// Init the ready list
static void ready_lists_init(void);

// Init the delay list
static void delay_list_init(void);

// Add the new tcb to the corresponding ready list
static uint32_t add_new_tcb_to_ready_lists(tcb_t *tcb);

// Get the current highest priority
static inline uint8_t get_highest_priority(void);

// Increment the Current Tick count
static void increment_tick(void);

// Check stack overflow
static void check_stack_overflow(void);

// Used for context switch, modified from freertos code
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
      "   ldr r0, [r1]                    \n" /* The first item in
                                                 pxCurrentTCB is the task top
                                                 of stack. */
      "   ldmia r0!, {r4-r11}             \n" /* Pop the registers that are
                                                 not automatically saved on
                                                 exception entry and the
                                                 critical nesting count. */
      "   msr psp, r0                     \n" /* Restore the task stack
                                               * pointer.
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

// And config the SysTick and start the first task
__attribute__((always_inline)) inline static void StartFirstTask(void) {
  (*((volatile uint32_t *)0xe000ed20)) |= (((uint32_t)255UL) << 16UL);
  (*((volatile uint32_t *)0xe000ed20)) |= (((uint32_t)255UL) << 24UL);
  SysTick->CTRL = 0UL;
  SysTick->VAL = 0UL;
  SysTick->LOAD = (configSysTickClockHz / configSysTickClockRateHz) - 1UL;
  SysTick->CTRL = ((1UL << 2UL) | (1UL << 1UL) | (1UL << 0UL));
  __asm volatile(
      " ldr r0, =0xE000ED08   \n" /* Use the NVIC offset register to locate
                                     the stack. */
      " ldr r0, [r0]          \n"
      " ldr r0, [r0]          \n"
      " msr msp, r0           \n" /* Set the msp back to the start of the
                                   * stack.
                                   */
      " cpsie i               \n" /* Globally enable interrupts. */
      " cpsie f               \n"
      " dsb                   \n"
      " isb                   \n"
      " svc 0                 \n" /* System call to start first task. */
      " nop                   \n"
      " .ltorg                \n");
}

// SysTick Handler
void SysTick_Handler(void) {
  uint32_t saved = critical_enter();
  if (scheduler_is_suspending == FALSE) {
    increment_tick();
  }
  critical_exit(saved);
}

/* Task API part
 */

// Create a new task
void task_create(task_func_t func, void *func_parameters, uint32_t stack_depth,
                 uint32_t priority, const char *const name,
                 task_handler_t *handler) {
  tcb_t *new_tcb;
  uint32_t *stack_top;
  uint32_t yield;

  // Allocate memory for the tcb
  new_tcb = (tcb_t *)halloc(sizeof(tcb_t));

  // Allocate memory for the task stack
  uint32_t stack_size = stack_depth + configSTACK_GUARD_SIZE;
  new_tcb->stack = (uint32_t *)halloc((size_t)stack_size * sizeof(uint32_t));

  // Set the guard bytes to detect stack overflow
  for (uint32_t i = 0; i < configSTACK_GUARD_SIZE; i++) {
    new_tcb->stack[i] = configSTACK_GUARD_MAGIC;
  }

  // Set stack top and align
  uint32_t *real_stack = new_tcb->stack + configSTACK_GUARD_SIZE;
  stack_top = real_stack + stack_depth - 1;
  stack_top = (uint32_t *)((uint32_t)stack_top & ~(uint32_t)(alignment_byte));

  // initialize the stack
  new_tcb->stack_top = stack_init(stack_top, func, func_parameters);
  new_tcb->priority = priority;
  new_tcb->base_priority = priority;

  // Initialize the state node and event node
  list_node_init(&(new_tcb->state_node));
  list_node_init(&(new_tcb->event_node));
  new_tcb->state_node.owner = new_tcb;
  new_tcb->event_node.val = configMaxPriority - priority;
  new_tcb->event_node.owner = new_tcb;

  // Set the name of the task
  for (size_t i = 0; i < configMAX_TASK_NAME_LEN; i++) {
    new_tcb->name[i] = name[i];
    if (name[i] == (char)0x00) {
      break;
    }
  }

  // set the task handler
  if (handler != NULL) {
    *handler = (task_handler_t)new_tcb;
  }

  // add the new tcb to the ready list
  if ((yield = add_new_tcb_to_ready_lists(new_tcb))) {
    if (scheduler_is_running == FALSE) {
      current_tcb = new_tcb;
    } else {
      task_yield();
    }
  }
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
  list_t *list = &ready_lists[tcb->priority];
  if (LIST_IS_EMPTY(list)) {
    ready_bits &= ~(1 << tcb->priority);
  }

  if (tcb == current_tcb) {
    list_insert_end(&zombie_list, &(tcb->state_node));
    critical_exit(saved);
  } else {
    critical_exit(saved);
    free_tcb(tcb);
  }

  if (yield && scheduler_is_running) {
    task_yield();
  }
}

void add_tcb_to_delay_list(tcb_t *tcb, uint32_t ticks) {
  uint32_t time_to_wake = ticks + current_tick_count;
  tcb->state_node.val = time_to_wake;
  uint32_t priority = tcb->priority;
  list_remove_node(&(current_tcb->state_node));

  // time_to wake overflow, put it in the overflow list
  if (time_to_wake < current_tick_count) {
    list_insert_node(delay_overflow_list, &(current_tcb->state_node));
  } else {
    list_insert_node(delay_list, &(current_tcb->state_node));
  }

  if (LIST_IS_EMPTY((&ready_lists[priority])))
    ready_bits = ready_bits & (~(1 << priority));
}

// Delay current task for given ticks
void task_delay(uint32_t ticks) {

  uint32_t saved = critical_enter();
  add_tcb_to_delay_list(current_tcb, ticks);
  critical_exit(saved);
  task_yield();
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
  if (LIST_IS_EMPTY(&(ready_lists[tcb->priority]))) {
    ready_bits &= ~(1 << tcb->priority);
  }
  list_insert_end(&suspended_list, &(tcb->state_node));

  critical_exit(saved);
  // we suspend current running tcb, so switch to another task
  if (yield && scheduler_is_running)
    task_yield();
}

void task_resume(task_handler_t *handler) {
  tcb_t *tcb = *handler;
  uint32_t yield;

  if (tcb != NULL && tcb != current_tcb) {
    uint32_t saved = critical_enter();

    list_remove_node(&(tcb->state_node));
    yield = add_tcb_to_ready_lists(tcb);

    critical_exit(saved);
    if (yield && scheduler_is_running) {
      task_yield();
    }
  }
}

// Get the current running tcb
tcb_t *get_current_tcb(void) { return current_tcb; }

// Add the task handler to the ready list
uint32_t add_tcb_to_ready_lists(tcb_t *tcb) {
  uint32_t yield = FALSE;
  ready_bits |= (1 << (tcb->priority));
  list_insert_end(&ready_lists[(tcb->priority)], &(tcb->state_node));
  if (tcb->priority > current_tcb->priority) {
    yield = TRUE;
  }
  return yield;
}

static uint32_t add_new_tcb_to_ready_lists(tcb_t *tcb) {
  uint32_t saved = critical_enter();
  uint32_t yield = FALSE;
  ready_bits |= (1 << (tcb->priority));
  list_insert_end(&ready_lists[(tcb->priority)], &(tcb->state_node));
  if (current_tcb == NULL) {
    current_tcb = tcb;
  } else {
    if (tcb->priority > current_tcb->priority && scheduler_is_running) {
      yield = TRUE;
    }
  }
  critical_exit(saved);
  return yield;
}

// NOTE: Task should never return, if so, it will reach here
// wichi indicates an error
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
    while (!LIST_IS_EMPTY(&zombie_list)) {
      list_node_t *node = list_get_next_index(&zombie_list);
      tcb_t *tcb = node->owner;
      list_remove_node(node);
      if (LIST_IS_EMPTY(&ready_lists[tcb->priority])) {
        ready_bits &= ~(1 << tcb->priority);
      }
      free_tcb(tcb);
    }

    critical_exit(saved);
  }
}

static void free_tcb(tcb_t *tcb) {
  hfree(tcb->stack);
  hfree(tcb);
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
  task_create(idle_task, NULL, 128, 0, "idle_task", &idle_task_handler);
}

// Start
void scheduler_start(void) {
  current_tcb = idle_task_handler;
  scheduler_is_running = TRUE;
  StartFirstTask();
}

void scheduler_suspend(void) {
  uint32_t saved = critical_enter();
  scheduler_is_suspending++;
  scheduler_is_running = FALSE;
  critical_exit(saved);
}

void scheduler_resume(void) {
  uint32_t saved = critical_enter();
  scheduler_is_suspending--;
  if (scheduler_is_suspending == FALSE) {
    scheduler_is_running = TRUE;
  }
  critical_exit(saved);
  return;
}

// Switch task
void vTaskSwitchContext(void) {
  check_stack_overflow();

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

  while (!LIST_IS_EMPTY(delay_list)) {
    list_node_t *first_node = delay_list->head.next;
    if (first_node->val <= current_tick_count) {
      tcb_t *tcb = (tcb_t *)(first_node->owner);
      list_remove_node(first_node);

      list_insert_end(&(ready_lists[tcb->priority]), first_node);
      ready_bits |= (1 << tcb->priority);
    } else {
      break;
    }
  }

  task_yield();
}

uint32_t get_current_tick(void) { return current_tick_count; }

/* Util for ipc*/

uint32_t block_timer_check(block_timer_t *timer, uint32_t *block_ticks) {
  if (*block_ticks == MAX_DELAY) {
    return FALSE;
  }

  uint32_t elapse_ticks = 0;

  // current tick count has overflowed
  if (current_tick_count < timer->start_tick) {
    elapse_ticks = (uint32_t)(-1) - timer->start_tick + current_tick_count;
  } else {
    elapse_ticks = current_tick_count - timer->start_tick;
  }

  if (elapse_ticks > *block_ticks) {
    *block_ticks = 0;
    return TRUE;
  } else {
    timer->start_tick = current_tick_count;
    block_ticks -= elapse_ticks;
    return FALSE;
  }
}

uint32_t task_resume_from_block(list_t *block_list) {
  uint32_t yield = 0;
  list_node_t *block_node = list_remove_next_node(block_list);
  tcb_t *block_tcb = block_node->owner;
  list_remove_node(&(block_tcb->state_node));
  yield = add_tcb_to_ready_lists(block_tcb);
  return yield;
}

void task_priority_inherit(mutex_t *mutex) {
  if (mutex->holder != NULL) {
    if (mutex->holder->priority < current_tcb->priority) {
      if (list_contain(&(ready_lists[mutex->holder->priority]),
                       &(mutex->holder->state_node))) {

        if (list_remove_node(&(mutex->holder->state_node)) == 0) {
          ready_bits &= ~(1 << mutex->holder->priority);
        }

        mutex->holder->priority = current_tcb->priority;
        add_tcb_to_ready_lists(mutex->holder);
      } else {
        mutex->holder->priority = current_tcb->priority;
      }
    }
  }
}

void task_priority_disinherit(mutex_t *mutex) {
  tcb_t *holder = mutex->holder;

  if (holder->base_priority != holder->priority) {

    if (list_remove_node(&(mutex->holder->state_node)) == 0) {
      ready_bits &= ~(1 << mutex->holder->priority);
    }

    holder->priority = holder->base_priority;
    add_tcb_to_ready_lists(holder);
  }
}

void task_priority_disinherit_timeout(mutex_t *mutex) {
  tcb_t *holder = mutex->holder;
  uint32_t restore_priority;

  if (holder->base_priority != holder->priority) {

    if (!LIST_IS_EMPTY(&(mutex->block_list))) {
      list_node_t *node = list_remove_next_node(&(mutex->block_list));
      restore_priority = ((tcb_t *)node->owner)->priority;
    } else {
      restore_priority = holder->base_priority;
    }

    if (list_remove_node(&(holder->state_node)) == 0) {
      ready_bits &= ~(1 << (holder->priority));
    }

    holder->priority = restore_priority;

    add_tcb_to_ready_lists(holder);
  }
}

/* Util for stack overflow checking */

static void task_overflow_hander(void) {
  while (1)
    ;
}

static void check_stack_overflow(void) {
  for (uint32_t i = 0; i < configSTACK_GUARD_SIZE; i++) {
    if (current_tcb->stack[i] != configSTACK_GUARD_MAGIC) {
      task_overflow_hander();
    }
  }
}

/* Functions that make other files can access task lists*/

list_t *get_ready_list(uint32_t priority) {
  if (priority > configMaxPriority) {
    return NULL;
  }

  return &ready_lists[priority];
}

list_t *get_suspended_list(void) { return &suspended_list; }

/* Functions to print task lists  */

uint32_t print_tasks_of_list(list_t *list, char *list_name) {
  uint32_t found = 0;
  if (!LIST_IS_EMPTY(list)) {
    list_node_t *node = list->head.next;
    while (node != &(list->head)) {
      tcb_t *tcb = (tcb_t *)node->owner;
      printf_("Task: %s | Priority: %u\r\n", list_name, tcb->name,
              tcb->priority);
      node = node->next;
      found++;
    }
  }
  return found;
}

void print_tasks(int type) {
  uint32_t found = 0;
  const char *header = NULL;

  switch (type) {
  case PRINT_READY:
    header = "READY TASKS";
    break;
  case PRINT_DELAY:
    header = "DELAYED TASKS";
    break;
  case PRINT_SUSPENDED:
    header = "SUSPENDED TASKS";
    break;
  }

  if (header) {
    printf_("\n========== %s ==========\n", header);
  }

  switch (type) {
  case PRINT_READY:
    for (size_t i = 0; i < configMaxPriority; i++) {
      found += print_tasks_of_list(&(ready_lists[i]), "ready");
    }
    break;
  case PRINT_DELAY:
    found += print_tasks_of_list(delay_list, "delayed");
    found += print_tasks_of_list(delay_overflow_list, "delayed");
    break;
  case PRINT_SUSPENDED:
    found += print_tasks_of_list(&suspended_list, "suspended");
    break;
  }

  if (found == 0) {
    printf_("None\n");
  } else {
    printf_("Total tasks found: %u\n", found);
  }
  printf_("=====================================\n");
}
