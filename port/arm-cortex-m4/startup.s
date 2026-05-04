// startup.s - Basic boot sequence with RTOS support
.syntax unified
.arch armv7-m
.cpu cortex-m4
.thumb

// Vector table
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
    .word PendSV_Handler             // PendSV handler
    .word SysTick_Handler            // SysTick handler

    // External interrupts
    .word Default_Handler            // IRQ 0: WWDG
    .word Default_Handler            // IRQ 1: PVD
    .word Default_Handler            // IRQ 2: TAMP_STAMP
    .word Default_Handler            // IRQ 3: RTC_WKUP
    .word Default_Handler            // IRQ 4: FLASH
    .word Default_Handler            // IRQ 5: RCC
    .word EXTI0_IRQHandler           // IRQ 6: EXTI Line 0 (PA0)
    .word Default_Handler            // IRQ 7: EXTI1
    .word Default_Handler            // IRQ 8: EXTI2
    .word Default_Handler            // IRQ 9: EXTI3
    .word Default_Handler            // IRQ 10: EXTI4
    .word Default_Handler            // IRQ 11: DMA1 Stream 0
    .word Default_Handler            // IRQ 12: DMA1 Stream 1
    .word Default_Handler            // IRQ 13: DMA1 Stream 2
    .word Default_Handler            // IRQ 14: DMA1 Stream 3
    .word Default_Handler            // IRQ 15: DMA1 Stream 4
    .word Default_Handler            // IRQ 16: DMA1 Stream 5
    .word Default_Handler            // IRQ 17: DMA1 Stream 6
    .word Default_Handler            // IRQ 18: ADC
    .word Default_Handler            // IRQ 19: CAN1_TX
    .word Default_Handler            // IRQ 20: CAN1_RX0
    .word Default_Handler            // IRQ 21: CAN1_RX1
    .word Default_Handler            // IRQ 22: CAN1_SCE
    .word Default_Handler            // IRQ 23: EXTI9_5
    .word Default_Handler            // IRQ 24: TIM1_BRK_TIM9
    .word Default_Handler            // IRQ 25: TIM1_UP_TIM10
    .word Default_Handler            // IRQ 26: TIM1_TRG_COM_TIM11
    .word Default_Handler            // IRQ 27: TIM1_CC
    .word Default_Handler            // IRQ 28: TIM2
    .word Default_Handler            // IRQ 29: TIM3
    .word Default_Handler            // IRQ 30: TIM4
    .word I2C1_EV_IRQHandler         // IRQ 31: I2C1 Event
    .word I2C1_ER_IRQHandler         // IRQ 32: I2C1 Error
    .word Default_Handler            // IRQ 33: I2C2 Event
    .word Default_Handler            // IRQ 34: I2C2 Error
    .word Default_Handler            // IRQ 35: SPI1
    .word Default_Handler            // IRQ 36: SPI2
    .word Default_Handler            // IRQ 37: USART1
    .word Default_Handler            // IRQ 38: USART2
    .word Default_Handler            // IRQ 39: USART3
    .word Default_Handler            // IRQ 40: EXTI15_10
    .word Default_Handler            // IRQ 41: RTC_Alarm
    .word Default_Handler            // IRQ 42: OTG_FS_WKUP
    .word Default_Handler            // IRQ 43: TIM8_BRK_TIM12
    .word Default_Handler            // IRQ 44: TIM8_UP_TIM13
    .word Default_Handler            // IRQ 45: TIM8_TRG_COM_TIM14
    .word Default_Handler            // IRQ 46: TIM8_CC
    .word DMA1_Stream7_IRQHandler    // IRQ 47: DMA1 Stream 7

.text
.thumb_func
.global Reset_Handler
Reset_Handler:
    // Due to ARM Thumb instruction encoding...
    ldr r0, =_estack                 // Load into low register first
    mov sp, r0                       // Then move to stack pointer

    // Copy data section from flash to RAM
    bl copy_data_init

    // Zero out BSS section
    bl zero_bss_init

    // Initialize system clocks
    bl SystemInit

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
.weak EXTI0_IRQHandler
.thumb_func
EXTI0_IRQHandler:
    b .

.weak I2C1_EV_IRQHandler
.thumb_func
I2C1_EV_IRQHandler:
    b .

.weak I2C1_ER_IRQHandler
.thumb_func
I2C1_ER_IRQHandler:
    b .

.weak DMA1_Stream7_IRQHandler
.thumb_func
DMA1_Stream7_IRQHandler:
    b .

.weak NMI_Handler
.weak MemManage_Handler
.weak BusFault_Handler
.weak UsageFault_Handler
.weak SVC_Handler
.weak DebugMon_Handler
.weak PendSV_Handler
.weak SysTick_Handler
.weak Default_Handler

.thumb_func
NMI_Handler:
.thumb_func
HardFault_Handler:
.thumb_func
MemManage_Handler:
.thumb_func
BusFault_Handler:
.thumb_func
UsageFault_Handler:
.thumb_func
SVC_Handler:
.thumb_func
DebugMon_Handler:
.thumb_func
Default_Handler:
    b .                              // Infinite loop for unhandled exceptions

// These will be implemented in context_switch.s
.thumb_func
.global PendSV_Handler               // Context switch handler
.thumb_func
.global SysTick_Handler              // Timer tick handler


// Add this to the bottom of port/arm-cortex-m4/startup.s

.global copy_data_init
.type copy_data_init, %function
copy_data_init:
    ldr r0, =_sdata         // Start of .data in RAM
    ldr r1, =_edata         // End of .data in RAM
    ldr r2, =_sidata        // Start of .data in FLASH (Load address)

    // Check if there is data to copy
    cmp r0, r1
    beq copy_data_done

copy_data_loop:
    ldr r3, [r2], #4        // Read word from flash and increment pointer
    str r3, [r0], #4        // Store word to RAM and increment pointer
    cmp r0, r1              // Are we done?
    blt copy_data_loop      // If not, keep copying

copy_data_done:
    bx lr

.global zero_bss_init
.type zero_bss_init, %function
zero_bss_init:
    ldr r0, =_sbss          // Start of .bss
    ldr r1, =_ebss          // End of .bss
    mov r2, #0              // We want to write zeros

    // Check if there is bss to zero
    cmp r0, r1
    beq zero_bss_done

zero_bss_loop:
    str r2, [r0], #4        // Store zero and increment pointer
    cmp r0, r1              // Are we done?
    blt zero_bss_loop       // If not, keep zeroing

zero_bss_done:
    bx lr

