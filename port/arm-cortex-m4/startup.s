// startup.s - Basic boot sequence
.syntax unified
.arch armv7-m
.cpu cortex-m4
.thumb
.global Reset_Handler


Reset_Handler:
  // Due to ARM Thumb instruction encoding...
  ldr r0, =_estack // Load into low register first
  mov sp, r0 // Then move to stack pointer

  // Copy data section from flash to RAM
  bl copy_data_init

  // Zero out BSS section
  bl zero_bss_init

  // Jump to main
  bl main
  b .
