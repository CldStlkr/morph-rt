// startup.s - Basic boot sequence with RTOS support
.syntax unified
.arch armv7-m
.cpu cortex-m4
.thumb

// Vector table (should be at the beginning)
.section .vectors, "a"
.global vector_table
vector_table:
    .word _estack                    // Initial stack pointer
    .word Reset_Handler              // Reset handler
    .word NMI_Handler                // NMI handler
    .word HardFault_Handler          // Hard fault handler
    .word MemManage_Handler          // Memory management fault
    .word BusFault_Handler           // Bus fault handler
    .word UsageFault_Handler         // Usage fault handler
    .word 0                          // Reserved
    .word 0                          // Reserved
    .word 0                          // Reserved
    .word 0                          // Reserved
    .word SVC_Handler                // SVCall handler
    .word DebugMon_Handler           // Debug monitor handler
    .word 0                          // Reserved
    .word PendSV_Handler             // PendSV handler (CRITICAL for RTOS!)
    .word SysTick_Handler            // SysTick handler (CRITICAL for RTOS!)
    
    // External interrupts (add as needed)
    .word Default_Handler            // IRQ 0
    .word Default_Handler            // IRQ 1
    // ... add more as needed for your specific MCU

.text
.global Reset_Handler
Reset_Handler:
    // Due to ARM Thumb instruction encoding...
    ldr r0, =_estack                 // Load into low register first
    mov sp, r0                       // Then move to stack pointer
    
    // Copy data section from flash to RAM
    bl copy_data_init
    
    // Zero out BSS section
    bl zero_bss_init
    
    // RTOS-specific initialization
    // Set PendSV and SysTick to lowest priority (so they don't preempt other interrupts)
    ldr r0, =0xE000ED20              // SHPR3 register address
    ldr r1, [r0]
    orr r1, r1, #0xFF000000          // Set PendSV priority to 0xFF (lowest)
    orr r1, r1, #0x00FF0000          // Set SysTick priority to 0xFF (lowest)
    str r1, [r0]
    
    // Jump to main
    bl main
    b .

// Default exception handlers (weak symbols so they can be overridden)
.weak NMI_Handler
.weak HardFault_Handler
.weak MemManage_Handler
.weak BusFault_Handler
.weak UsageFault_Handler
.weak SVC_Handler
.weak DebugMon_Handler
.weak PendSV_Handler
.weak SysTick_Handler
.weak Default_Handler

NMI_Handler:
HardFault_Handler:
MemManage_Handler:
BusFault_Handler:
UsageFault_Handler:
SVC_Handler:
DebugMon_Handler:
Default_Handler:
    b .                              // Infinite loop for unhandled exceptions

// These will be implemented in context_switch.s
.global PendSV_Handler               // Context switch handler
.global SysTick_Handler              // Timer tick handler

// Placeholder implementations (remove when you implement them)
PendSV_Handler:
    bx lr                            // Just return for now

SysTick_Handler:
    // Call your scheduler_tick() function
    push {lr}
    bl scheduler_tick
    pop {lr}
    bx lr
