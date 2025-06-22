# Morph-RT

A custom RTOS kernel built from scratch for ARM Cortex-M4.

## What is this?

I'm building a real-time operating system kernel to understand how these systems work under the hood. Starting with the basics: task scheduling, memory management, and inter-process communication.

**Current status:** Task implementation complete, working on task scheduling.

## Structure

```
na-rtos/
├── kernel/
│   ├── inc/           # Public headers
│   └── src/           # Implementation
├── port/arm-cortex-m4/ # Hardware-specific code
├── examples/          # Demo applications
└── tests/             # Unit tests
```

## Building

```bash
# Native build (for testing)
make

# ARM build (for hardware)
make TARGET=stm32f4

# Run tests
make test
```

## Design decisions

**Memory management:** Static allocation only. The kernel doesn't do dynamic allocation - that's the application's job.

**Error handling:** Functions return status codes, use output parameters for data. No exceptions or global error state.

**Circular buffer optimization:** Using bit masking instead of modulo for index wrapping. Requires power-of-2 buffer sizes but avoids expensive division operations.

```c
// Instead of: index = (index + 1) % capacity
// We do: index = (index + 1) & mask
```

**API design:** Keep it simple. `cb_*` functions for initialization, `self` parameter for operations on existing objects.

## Current implementation

### Circular Buffer
Generic circular buffer that works with any data type. Used for message queues, UART buffers, etc.

Key features:
- Generic void* design
- Power-of-2 size optimization
- Complete API: put, get, peek, clear, size checks
- Proper error handling and memory safety

### What's next
1. Task Control Blocks and task lifecycle management
2. Priority-based scheduler with round-robin
3. Context switching (ARM assembly)
4. Basic kernel services
5. Traffic light demo to tie it all together

## Hardware target

STM32F4 Discovery board (ARM Cortex-M4). Planning to keep the hardware abstraction clean so porting to other Cortex-M chips should be straightforward.

## Why build this?

Mostly curiosity. RTOSes like many things, don't make sense to me, so I thought what better way to understand than to build one.
Plus, understanding scheduler internals and memory management at this level is useful even for application development.

## License

MIT
