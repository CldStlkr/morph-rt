/* port/arm-cortex-m4/context_switch.s
 * ARM Cortex-M4 Context Switching for STM32F4
 *
 */

.syntax unified
.cpu cortex-m4
.thumb

/* System Control Block (SCB) registers */
.equ SCB_ICSR,      0xE000ED04  /* Interrupt Control and State Register */
.equ SCB_VTOR,      0xE000ED08  /* Vector Table Offset Register */
.equ SCB_AIRCR,     0xE000ED0C  /* Application Interrupt and Reset Control */
.equ SCB_CCR,       0xE000ED14  /* Configuration and Control Register */

/* ICSR register bits */
.equ ICSR_PENDSVSET, 0x10000000 /* Bit 28: PendSV set-pending bit */

/* SysTick registers */
.equ SYSTICK_CTRL,  0xE000E010  /* SysTick Control and Status */
.equ SYSTICK_LOAD,  0xE000E014  /* SysTick Reload Value */
.equ SYSTICK_VAL,   0xE000E018  /* SysTick Current Value */

.section .text

/*
 * void start_first_task(uint32_t *first_task_sp)
 *
 * Starts the very first task - called from scheduler_start()
 * Parameter: r0 = first task's stack pointer value
 */
.global start_first_task
.type start_first_task, %function
start_first_task:
    /* Set PSP to first task's stack */
    msr     psp, r0

    /* Switch to use PSP in thread mode */
    mrs     r0, control
    orr     r0, r0, #2
    msr     control, r0
    isb

    /* Explicitly enable interrupts (clears PRIMASK).
       st-flash often leaves interrupts disabled when resetting the board,
       which prevents SysTick and PendSV from ever firing! */
    cpsie   i

    /* Pop software-saved registers R4-R11 and EXC_RETURN (in LR) */
    pop     {r4-r11, lr}

    /* Pop hardware registers - CPU will use PSP now */
    pop     {r0-r3, r12}
    pop     {r1}      /* Discard hardware LR */
    pop     {pc}      /* Pop true PC and start task */

/*
 * void trigger_context_switch(void)
 *
 * Triggers a context switch by setting PendSV interrupt
 * Called from scheduler_yield()
 */
.global trigger_context_switch
.type trigger_context_switch, %function
trigger_context_switch:
    /* Set PendSV interrupt pending */
    ldr     r0, =SCB_ICSR
    ldr     r1, =ICSR_PENDSVSET
    str     r1, [r0]

    /* Ensure write completes before returning */
    dsb
    isb

    bx      lr

/*
 * PendSV_Handler
 * 
 * Context switch interrupt handler
 * This is where the actual context switching happens
 */
.global PendSV_Handler
.type PendSV_Handler, %function
PendSV_Handler:
    /* Disable interrupts during context switch */
    cpsid   i

    /* Check if this is the first context switch */
    ldr     r2, =current_task
    ldr     r1, [r2]            /* r1 = current_task */
    cbz     r1, switch_logic    /* If current_task == NULL, skip save */

save_context:
    /* Save software registers (R4-R11) and EXC_RETURN (LR) on current task's stack */
    mrs     r0, psp             /* Get process stack pointer */
    stmdb   r0!, {r4-r11, lr}   /* Push R4-R11 and LR onto stack */

    /* Save the new stack pointer back to current task's TCB */
    str     r0, [r1]            /* current_task->stack_pointer = r0 */

switch_logic:
    /* Call C function to select next task and handle instrumentation */
    bl      scheduler_switch_context
    /* r0 now contains the new current_task handle */

    /* Load next task's stack pointer */
    ldr     r0, [r0]            /* r0 = next_task->stack_pointer */

    /* Restore software registers (R4-R11) and EXC_RETURN */
    ldmia   r0!, {r4-r11, lr}   /* Pop R4-R11 and LR from stack */

    /* Update process stack pointer */
    msr     psp, r0

    /* Enable interrupts */
    cpsie   i

    /* Return using process stack via popped EXC_RETURN */
    bx      lr

/*
 * SysTick_Handler
 *
 * System timer interrupt - calls scheduler_tick()
 */
.global SysTick_Handler
.type SysTick_Handler, %function
SysTick_Handler:
    /* Save context on stack */
    push    {r7, lr}

    /* Call the C function scheduler_tick() */
    bl      scheduler_tick

    /* Restore context and return */
    pop     {r7, lr}
    bx      lr

/*
 * systick_init(uint32_t ticks_per_second)
 *
 * Initialize SysTick timer
 * Parameter: r0 = desired tick frequency (e.g., 1000 for 1ms ticks)
 */
.global systick_init
.type systick_init, %function
systick_init:
    push    {r4, lr}

    /* Calculate reload value: (SystemCoreClock / ticks_per_second) - 1 */
    /* For STM32F4 @ 168MHz (HSE): (168000000 / 1000) - 1 = 167999 for 1ms */
    ldr     r1, =SystemCoreClock
    ldr     r1, [r1]            /* Load actual system clock frequency */
    udiv    r2, r1, r0          /* r2 = SystemCoreClock / ticks_per_second */
    sub     r2, r2, #1          /* r2 = reload_value - 1 */

    /* Set SysTick reload value */
    ldr     r1, =SYSTICK_LOAD
    str     r2, [r1]

    /* Clear current value */
    ldr     r1, =SYSTICK_VAL
    mov     r2, #0
    str     r2, [r1]

    /* Enable SysTick: CLKSOURCE=1 (processor clock), TICKINT=1, ENABLE=1 */
    ldr     r1, =SYSTICK_CTRL
    mov     r2, #0x7            /* Bits 0,1,2 = ENABLE, TICKINT, CLKSOURCE */
    str     r2, [r1]

    pop     {r4, lr}
    bx      lr

/*
 * set_pendsv_priority(void)
 *
 * Set PendSV to lowest priority (highest priority value)
 * This ensures context switches happen after other interrupts
 */
.global set_pendsv_priority
.type set_pendsv_priority, %function
set_pendsv_priority:
    /* PendSV priority is in NVIC_SYSPRI14 (0xE000ED22) */
    ldr     r0, =0xE000ED22
    mov     r1, #0xFF           /* Lowest priority */
    strb    r1, [r0]
    bx      lr


.global HardFault_Handler
.type HardFault_Handler, %function
HardFault_Handler:
    b .

/* External symbols from C code */
.extern current_task
.extern scheduler_tick
.extern SystemCoreClock

.end
