#include "task.h"
#include "config.h"

//The current running task
__attribute__((used)) volatile tcb_t* current_tcb = NULL;
//handler for the idle task
static task_handler_t idle_task_handler = NULL;
// task table
static task_handler_t ready_list[configMaxPriority];
// ready bits for task table
uint32_t ready_bits = 0;
//delay lists
static uint32_t delay_lst[configMaxPriority];
static uint32_t delay_overflow_lst[configMaxPriority];
static uint32_t* delay_list;
static uint32_t* delay_overflow_list;

static uint32_t max_priority = 0;
static uint32_t current_tick_count;

//used for context switch, from freertos
__attribute__((naked)) void xPortPendSVHandler( void )
{
    __asm volatile
    (
        "   mrs r0, psp                         \n"
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
        "pxCurrentTCBConst: .word current_tcb  \n"
        ::"i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY )
    );
}
//SCV handler
__attribute__((naked)) void vPortSVCHandler( void )
{
    __asm volatile (
        "   ldr r3, pxCurrentTCBConst2      \n" /* Restore the context. */
        "   ldr r1, [r3]                    \n" /* Use pxCurrentTCBConst to get the pxCurrentTCB address. */
        "   ldr r0, [r1]                    \n" /* The first item in pxCurrentTCB is the task top of stack. */
        "   ldmia r0!, {r4-r11}             \n" /* Pop the registers that are not automatically saved on exception entry and the critical nesting count. */
        "   msr psp, r0                     \n" /* Restore the task stack pointer. */
        "   isb                             \n"
        "   mov r0, #0                      \n"
        "   msr basepri, r0                 \n"
        "   orr r14, #0xd                   \n"
        "   bx r14                          \n"
        "                                   \n"
        "   .align 4                        \n"
        "pxCurrentTCBConst2: .word current_tcb             \n"
        );
}




static void increment_tick(void);
void SysTick_Handler(void)
{
    uint32_t ret = critical_enter();
    increment_tick();
    critical_exit(ret);
}


//start the first task
__attribute__((always_inline)) inline static void StartFirstTask( void )
{
    ( *( ( volatile uint32_t * ) 0xe000ed20 ) ) |= ( ( ( uint32_t ) 255UL ) << 16UL );
    ( *( ( volatile uint32_t * ) 0xe000ed20 ) ) |= ( ( ( uint32_t ) 255UL ) << 24UL );
    SysTick->CTRL = 0UL;
    SysTick->VAL  = 0UL;
    SysTick->LOAD = (configSysTickClockHz / configSysTickClockRateHz) - 1UL;
    SysTick->CTRL = ( ( 1UL << 2UL ) | ( 1UL << 1UL ) | ( 1UL << 0UL ) );
    __asm volatile (
        " ldr r0, =0xE000ED08   \n" /* Use the NVIC offset register to locate the stack. */
        " ldr r0, [r0]          \n" " ldr r0, [r0]          \n"
        " msr msp, r0           \n" /* Set the msp back to the start of the stack. */
        " cpsie i               \n" /* Globally enable interrupts. */
        " cpsie f               \n"
        " dsb                   \n"
        " isb                   \n"
        " svc 0                 \n" /* System call to start first task. */
        " nop                   \n"
        " .ltorg                \n"
        );
}


void add_to_ready_list(task_handler_t* handler, uint32_t priority){
    if(priority < max_priority)
        max_priority = priority;

    ready_bits |= (1 << priority);
    ready_list[priority] = *handler;
}


void task_create(task_func_t func, void* func_parameters, uint32_t stack_depth,
                 uint32_t priority, task_handler_t* handler) {
    tcb_t* new_tcb;
    uint32_t* stack_top;
    //allocate memory for the tcb and stack
    new_tcb = (tcb_t*)halloc(sizeof(tcb_t));
    new_tcb->stack= (uint32_t*)halloc((size_t)stack_depth * sizeof(uint32_t));
    //get the stack top addresss and align
    stack_top = new_tcb->stack + (stack_depth - (uint32_t)1);
    stack_top = (uint32_t*)((uint32_t)stack_top & ~(uint32_t)(alignment_byte));
    //initialize the stack
    new_tcb->stack_top = stack_init(stack_top, func, func_parameters);
    new_tcb->priority = priority;
    //set the task handler
    *handler = (task_handler_t)new_tcb;
    //put the tcb into task table
    add_to_ready_list(handler, priority);
}

static void task_exit_error(){
    while(1){
    }
}

uint32_t* stack_init(uint32_t* stack_top, task_func_t func,void* parameters){
    //set the XPSR
    stack_top--;
    *stack_top = INITIAL_XPSR;
    //set the task func
    stack_top--;
    *stack_top = (uint32_t)func & (uint32_t)START_ADDRESS_MASK;
    //set the error handler
    stack_top--;
    *stack_top = (uint32_t)task_exit_error;
    //set the r12, r1-3 to zero, and set r0 to parameters
    stack_top -= 5;
    *stack_top = (uint32_t)parameters;
    //leave space for r4-r11
    stack_top -= 8;
    return stack_top;
}

uint32_t enter_idle = 0;
void idle_task(){
    while(1){
        enter_idle++;
        task_switch();
    }
}

static void delay_list_init(void);
void scheduler_init( void )
{
    current_tick_count = 0;
    delay_list_init();
    task_create(idle_task, NULL, 64, 0, &idle_task_handler);
    current_tcb = idle_task_handler;
}

void scheduler_start(void){
    StartFirstTask();
}

static inline uint8_t get_highest_priority(void);
void vTaskSwitchContext(void){
    current_tcb = ready_list[get_highest_priority()];
}

__attribute__((always_inline)) inline uint32_t  critical_enter( void )
{
    uint32_t ret;
    uint32_t temp;
    __asm volatile(
            " cpsid i               \n"
            " mrs %0, basepri       \n"
            " mov %1, %2            \n"
            " msr basepri, %1       \n"
            " dsb                   \n"
            " isb                   \n"
            " cpsie i               \n"
            : "=r" (ret), "=r"(temp)
            : "r" (configMAX_SYSCALL_INTERRUPT_PRIORITY)
            : "memory"
            );
    return ret;
}

__attribute__((always_inline)) inline void critical_exit(uint32_t ret)
{
    __asm volatile(
            " cpsid i               \n"
            " msr basepri, %0       \n"
            " dsb                   \n"
            " isb                   \n"
            " cpsie i               \n"
            :: "r" (ret)
            : "memory"
            );
}

__attribute__( ( always_inline ) ) static inline uint8_t get_highest_priority( void )
{
    uint8_t top_zero;
    uint8_t temp;

    __asm volatile
            (
            "clz %0, %2\n"
            "mov %1, #31\n"
            "sub %0, %1, %0\n"
            :"=r" (top_zero),"=r"(temp)
            :"r" (ready_bits)
            );
    return top_zero;
}

static void delay_list_init(void){
    current_tick_count = 0;
    delay_list = delay_lst;
    delay_overflow_list = delay_overflow_lst;
}

void delay_list_switch(void){
    uint32_t* tmp;
    tmp = delay_list;
    delay_list = delay_overflow_list;
    delay_overflow_list = tmp;
}

void task_delay(uint32_t ticks){uint32_t time_to_wake = ticks + current_tick_count; uint32_t priority = current_tcb->priority;
    //overflow
    if(time_to_wake < current_tick_count){
        delay_overflow_list[priority] = time_to_wake;
    }else{
        delay_list[priority] = time_to_wake;
    }

    ready_bits = ready_bits & (~(1 << priority));
    task_switch();
}

static void increment_tick(void){
    current_tick_count += 1;

    //the tick count have overflowed
    if(current_tick_count == 0){
        delay_list_switch();
    }

    for(int i = 0; i < configMaxPriority;i++){
        if(delay_list[i] > 0){
            if(current_tick_count >= delay_list[i]){
              delay_list[i]  = 0;
              ready_bits |= (1 << i);
            }
        }
    }

    task_switch();
}
