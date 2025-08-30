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
    /* Disable interrupts during startup */
    cpsid   i
    
    /* Set the process stack pointer to the first task's stack */
    msr     psp, r0             /* PSP = first_task_sp */
    
    /* Enable interrupts */
    cpsie   i
    
    /* Pop the software-saved registers (R4-R11) */
    ldmia   r0!, {r4-r11}
    
    /* Update PSP after popping software registers */
    msr     psp, r0
    
    /* Return using process stack */
    /* EXC_RETURN = 0xFFFFFFFD (return to Thread mode, use PSP) */
    ldr     lr, =0xFFFFFFFD
    bx      lr

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
    cbz     r1, restore_context /* If current_task == NULL, just restore */
    
save_context:
    /* Save software registers (R4-R11) on current task's stack */
    /* Hardware registers (R0-R3, R12, LR, PC, xPSR) already saved by CPU */
    mrs     r0, psp             /* Get process stack pointer */
    stmdb   r0!, {r4-r11}       /* Push R4-R11 onto stack */
    
    /* Save the new stack pointer back to current task's TCB */
    str     r0, [r1]            /* current_task->stack_pointer = r0 */

restore_context:
    /* Get the next task to run */
    ldr     r1, =next_task
    ldr     r2, [r1]            /* r2 = next_task */
    
    /* Update current_task = next_task */
    ldr     r3, =current_task
    str     r2, [r3]
    
    /* Load next task's stack pointer */
    ldr     r0, [r2]            /* r0 = next_task->stack_pointer */
    
    /* Restore software registers (R4-R11) */
    ldmia   r0!, {r4-r11}       /* Pop R4-R11 from stack */
    
    /* Update process stack pointer */
    msr     psp, r0
    
    /* Enable interrupts */
    cpsie   i
    
    /* Return using process stack */
    /* EXC_RETURN = 0xFFFFFFFD (return to Thread mode, use PSP) */
    ldr     lr, =0xFFFFFFFD
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
    push    {lr}
    
    /* Call the C function scheduler_tick() */
    bl      scheduler_tick
    
    /* Check if we need to context switch */
    bl      scheduler_get_next_task
    ldr     r1, =current_task
    ldr     r2, [r1]
    cmp     r0, r2              /* Compare next_task with current_task */
    beq     systick_exit        /* If same, no context switch needed */
    
    /* Store next_task for PendSV handler */
    ldr     r1, =next_task
    str     r0, [r1]
    
    /* Trigger context switch */
    bl      trigger_context_switch

systick_exit:
    /* Restore context and return */
    pop     {lr}
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
    /* For STM32F4 @ 168MHz: (168000000 / 1000) - 1 = 167999 for 1ms */
    ldr     r1, =168000000      /* STM32F4 max frequency */
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

/* External symbols from C code */
.extern current_task
.extern next_task
.extern scheduler_tick
.extern scheduler_get_next_task

.end
