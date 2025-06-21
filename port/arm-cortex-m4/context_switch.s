/*
 * context_switch.s - ARM Cortex-M4 Context Switching
 *
 * This file implements the low-level context switching for the RTOS
 * using ARM Cortex-M4 specific instructions and registers
 *
 */

.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.text

// External symbols from scheduler
.extern current_task
.extern next_task

// System Control Block registers
.equ SCB_ICSR, 0xE000ED04 // Interrupt Control and State Register
.equ PENDSVSET, 0x10000000 // Bit 28: PendSV set-pending bit


