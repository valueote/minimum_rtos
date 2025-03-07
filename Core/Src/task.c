#include "task.h"
#include "config.h"

__attribute__((used)) volatile tcb_t* current_tcb = NULL;

task_handler_t idle_task_handler = NULL;

task_handler_t task_table[configMaxPriority];


//from freertos
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

__attribute__((always_inline)) inline static void StartFirstTask( void )
{
    ( *( ( volatile uint32_t * ) 0xe000ed20 ) ) |= ( ( ( uint32_t ) 255UL ) << 16UL );
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
    //set the task handler
    *handler = (task_handler_t)new_tcb;
    //put the tcb into task table
    task_table[priority] = new_tcb;
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

void scheduler_init( void )
{
    task_create(idle_task, NULL, 64, 0, &idle_task_handler);
    current_tcb = idle_task_handler;
}

void scheduler_start(void){
    StartFirstTask();
}

uint32_t x = 0;
void vTaskSwitchContext(void){
    x++;
    current_tcb = task_table[x % 3];
}
