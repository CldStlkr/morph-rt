#ifndef CONFIG_H
#define CONFIG_H

// RTOS Configuration
#define MAX_PRIORITY 7

// Pool config
#define MAX_TASKS 8
#define MAX_QUEUES 4
#define MAX_SEMAPHORES 8
#define MAX_MUTEXES 4

// Stack sizes
#define SMALL_STACK_SIZE 512
#define DEFAULT_STACK_SIZE 1024
#define LARGE_STACK_SIZE 2048

#define MAX_SMALL_STACKS 4
#define MAX_DEFAULT_STACKS 6
#define MAX_LARGE_STACKS 2

// Buffer sizes for queues
#define SMALL_BUFFER_SIZE 64
#define DEFAULT_BUFFER_SIZE 256
#define LARGE_BUFFER_SIZE 1024 // 1KB

#define MAX_SMALL_BUFFERS 8
#define MAX_MEDIUM_BUFFERS 4
#define MAX_LARGE_BUFFERS 2

#endif // CONFIG_H
