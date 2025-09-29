#include "memory.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

// Test fixtures
static void *allocated_ptrs[64]; // Track allocations for cleanup
static size_t alloc_count = 0;

void setUp(void) {
    memory_pools_init();

    // Clear allocation tracking
    memset(allocated_ptrs, 0, sizeof(allocated_ptrs));
    alloc_count = 0;
}

void tearDown(void) {
    // Free any tracked allocations
    for (size_t i = 0; i < alloc_count; i++) {
        if (allocated_ptrs[i]) {
            // Try to free from all pool types (brute force cleanup)
            for (pool_type_t type = 0; type < POOL_COUNT; type++) {
                pool_free(type, allocated_ptrs[i]);
            }
        }
    }
    alloc_count = 0;
}

// Helper function to track allocations for cleanup
static void track_allocation(void *ptr) {
    if (alloc_count < sizeof(allocated_ptrs)/sizeof(allocated_ptrs[0])) {
        allocated_ptrs[alloc_count++] = ptr;
    }
}

//=============================================================================
// MEMORY POOL INITIALIZATION TESTS
//=============================================================================

void test_memory_pools_init_should_initialize_all_pools(void) {
    // Verify all pools have expected initial state
    for (pool_type_t type = 0; type < POOL_COUNT; type++) {
        pool_stats_t stats = pool_get_stats(type);
        TEST_ASSERT_GREATER_THAN(0, stats.total_objects);
        TEST_ASSERT_EQUAL(stats.total_objects, stats.free_objects);
        TEST_ASSERT_EQUAL(0, stats.used_objects);
        TEST_ASSERT_EQUAL(0, stats.peak_usage);
    }
}

void test_memory_pools_init_should_set_correct_pool_sizes(void) {
    pool_stats_t tcb_stats = pool_get_stats(POOL_TCB);
    TEST_ASSERT_EQUAL(MAX_TASKS, tcb_stats.total_objects);
    
    pool_stats_t small_stack_stats = pool_get_stats(POOL_STACK_SMALL);
    TEST_ASSERT_EQUAL(MAX_SMALL_STACKS, small_stack_stats.total_objects);
    
    pool_stats_t queue_stats = pool_get_stats(POOL_QCB);
    TEST_ASSERT_EQUAL(MAX_QUEUES, queue_stats.total_objects);
}

//=============================================================================
// BASIC ALLOCATION/DEALLOCATION TESTS
//=============================================================================

void test_pool_alloc_should_return_valid_pointer(void) {
    void *ptr = pool_alloc(POOL_TCB);
    TEST_ASSERT_NOT_NULL(ptr);
    track_allocation(ptr);
    
    // Verify stats updated
    pool_stats_t stats = pool_get_stats(POOL_TCB);
    TEST_ASSERT_EQUAL(1, stats.used_objects);
    TEST_ASSERT_EQUAL(MAX_TASKS - 1, stats.free_objects);
}

void test_pool_alloc_should_return_zeroed_memory(void) {
    uint8_t *ptr = (uint8_t *)pool_alloc(POOL_TCB);
    TEST_ASSERT_NOT_NULL(ptr);
    track_allocation(ptr);
    
    // Check that memory is zeroed
    for (size_t i = 0; i < sizeof(task_control_block); i++) {
        TEST_ASSERT_EQUAL(0, ptr[i]);
    }
}

void test_pool_alloc_should_return_different_pointers(void) {
    void *ptr1 = pool_alloc(POOL_TCB);
    void *ptr2 = pool_alloc(POOL_TCB);
    
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr2);
    
    track_allocation(ptr1);
    track_allocation(ptr2);
}

void test_pool_free_should_return_true_for_valid_pointer(void) {
    void *ptr = pool_alloc(POOL_TCB);
    TEST_ASSERT_NOT_NULL(ptr);
    
    bool result = pool_free(POOL_TCB, ptr);
    TEST_ASSERT_TRUE(result);
    
    // Verify stats updated
    pool_stats_t stats = pool_get_stats(POOL_TCB);
    TEST_ASSERT_EQUAL(0, stats.used_objects);
    TEST_ASSERT_EQUAL(MAX_TASKS, stats.free_objects);
}

void test_pool_free_should_return_false_for_null_pointer(void) {
    bool result = pool_free(POOL_TCB, NULL);
    TEST_ASSERT_FALSE(result);
}

void test_pool_free_should_return_false_for_invalid_pointer(void) {
    int stack_var = 42;
    bool result = pool_free(POOL_TCB, &stack_var);
    TEST_ASSERT_FALSE(result);
}

void test_pool_free_should_return_false_for_double_free(void) {
    void *ptr = pool_alloc(POOL_TCB);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // First free should succeed
    bool result1 = pool_free(POOL_TCB, ptr);
    TEST_ASSERT_TRUE(result1);
    
    // Second free should fail
    bool result2 = pool_free(POOL_TCB, ptr);
    TEST_ASSERT_FALSE(result2);
}

//=============================================================================
// POOL EXHAUSTION TESTS
//=============================================================================

void test_pool_alloc_should_fail_when_pool_exhausted(void) {
    // Allocate all objects from small stack pool
    void *ptrs[MAX_SMALL_STACKS];
    
    // Should succeed for MAX_SMALL_STACKS allocations
    for (int i = 0; i < MAX_SMALL_STACKS; i++) {
        ptrs[i] = pool_alloc(POOL_STACK_SMALL);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
        track_allocation(ptrs[i]);
    }
    
    // Next allocation should fail
    void *overflow_ptr = pool_alloc(POOL_STACK_SMALL);
    TEST_ASSERT_NULL(overflow_ptr);
    
    // Verify stats show pool is full
    pool_stats_t stats = pool_get_stats(POOL_STACK_SMALL);
    TEST_ASSERT_EQUAL(MAX_SMALL_STACKS, stats.used_objects);
    TEST_ASSERT_EQUAL(0, stats.free_objects);
}

void test_pool_alloc_should_succeed_after_free(void) {
    // Fill the pool
    void *ptrs[MAX_SMALL_STACKS];
    for (int i = 0; i < MAX_SMALL_STACKS; i++) {
        ptrs[i] = pool_alloc(POOL_STACK_SMALL);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }
    
    // Verify pool is full
    void *overflow = pool_alloc(POOL_STACK_SMALL);
    TEST_ASSERT_NULL(overflow);
    
    // Free one object
    bool free_result = pool_free(POOL_STACK_SMALL, ptrs[0]);
    TEST_ASSERT_TRUE(free_result);
    
    // Should be able to allocate again
    void *new_ptr = pool_alloc(POOL_STACK_SMALL);
    TEST_ASSERT_NOT_NULL(new_ptr);
    track_allocation(new_ptr);
    
    // Clean up remaining
    for (int i = 1; i < MAX_SMALL_STACKS; i++) {
        pool_free(POOL_STACK_SMALL, ptrs[i]);
    }
}

//=============================================================================
// PEAK USAGE TRACKING TESTS
//=============================================================================

void test_pool_should_track_peak_usage(void) {
    // Initial peak should be 0
    pool_stats_t initial_stats = pool_get_stats(POOL_TCB);
    TEST_ASSERT_EQUAL(0, initial_stats.peak_usage);
    
    // Allocate 3 objects
    void *ptr1 = pool_alloc(POOL_TCB);
    void *ptr2 = pool_alloc(POOL_TCB);
    void *ptr3 = pool_alloc(POOL_TCB);
    
    track_allocation(ptr1);
    track_allocation(ptr2);
    track_allocation(ptr3);
    
    pool_stats_t peak_stats = pool_get_stats(POOL_TCB);
    TEST_ASSERT_EQUAL(3, peak_stats.peak_usage);
    
    // Free one object - peak should remain 3
    pool_free(POOL_TCB, ptr2);
    
    pool_stats_t after_free_stats = pool_get_stats(POOL_TCB);
    TEST_ASSERT_EQUAL(3, after_free_stats.peak_usage);
    TEST_ASSERT_EQUAL(2, after_free_stats.used_objects);
}

//=============================================================================
// TYPE-SPECIFIC ALLOCATION TESTS
//=============================================================================

void test_task_pool_alloc_tcb_should_return_task_control_block(void) {
    task_control_block *tcb = (task_control_block *)task_pool_alloc_tcb();
    TEST_ASSERT_NOT_NULL(tcb);
    track_allocation(tcb);
    
    // Verify it's properly zeroed
    TEST_ASSERT_EQUAL(0, tcb->base_priority);
    TEST_ASSERT_EQUAL(0, tcb->effective_priority);
    TEST_ASSERT_EQUAL(0, tcb->state);
    TEST_ASSERT_NULL(tcb->stack_pointer);
}

void test_task_pool_alloc_stack_should_select_appropriate_size(void) {
    // Test small stack allocation
    void *small_stack = task_pool_alloc_stack(256);
    TEST_ASSERT_NOT_NULL(small_stack);
    track_allocation(small_stack);
    
    // Test default stack allocation
    void *default_stack = task_pool_alloc_stack(800);
    TEST_ASSERT_NOT_NULL(default_stack);
    track_allocation(default_stack);
    
    // Test large stack allocation  
    void *large_stack = task_pool_alloc_stack(1500);
    TEST_ASSERT_NOT_NULL(large_stack);
    track_allocation(large_stack);
    
    // Test oversized request
    void *oversized_stack = task_pool_alloc_stack(4096);
    TEST_ASSERT_NULL(oversized_stack);
}


//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

void test_pool_alloc_should_handle_invalid_pool_type(void) {
    void *ptr = pool_alloc(POOL_COUNT); // Invalid type
    TEST_ASSERT_NULL(ptr);
    
    void *ptr2 = pool_alloc((pool_type_t)999); // Way out of range
    TEST_ASSERT_NULL(ptr2);
}

void test_pool_free_should_handle_invalid_pool_type(void) {
    void *ptr = pool_alloc(POOL_TCB);
    TEST_ASSERT_NOT_NULL(ptr);
    
    bool result = pool_free(POOL_COUNT, ptr); // Invalid type
    TEST_ASSERT_FALSE(result);
    
    // Clean up properly
    pool_free(POOL_TCB, ptr);
}

void test_pool_get_stats_should_handle_invalid_pool_type(void) {
    pool_stats_t stats = pool_get_stats(POOL_COUNT);
    TEST_ASSERT_EQUAL(0, stats.total_objects);
    TEST_ASSERT_EQUAL(0, stats.free_objects);
    TEST_ASSERT_EQUAL(0, stats.used_objects);
    TEST_ASSERT_EQUAL(0, stats.peak_usage);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

void test_pool_should_handle_allocation_deallocation_cycles(void) {
    // Repeatedly allocate and free to test for corruption
    for (int cycle = 0; cycle < 10; cycle++) {
        void *ptrs[4];
        
        // Allocate multiple objects
        for (int i = 0; i < 4; i++) {
            ptrs[i] = pool_alloc(POOL_TCB);
            TEST_ASSERT_NOT_NULL(ptrs[i]);
        }
        
        // Free them in different order
        pool_free(POOL_TCB, ptrs[2]);
        pool_free(POOL_TCB, ptrs[0]);
        pool_free(POOL_TCB, ptrs[3]);
        pool_free(POOL_TCB, ptrs[1]);
        
        // Verify pool state is consistent
        pool_stats_t stats = pool_get_stats(POOL_TCB);
        TEST_ASSERT_EQUAL(0, stats.used_objects);
        TEST_ASSERT_EQUAL(MAX_TASKS, stats.free_objects);
    }
}

void test_pool_should_maintain_consistency_across_multiple_types(void) {
    // Allocate from different pool types simultaneously
    void *tcb = pool_alloc(POOL_TCB);
    void *small_stack = pool_alloc(POOL_STACK_SMALL);
    void *queue = pool_alloc(POOL_QCB);
    void *buffer = pool_alloc(POOL_BUFFER_SMALL);
    
    TEST_ASSERT_NOT_NULL(tcb);
    TEST_ASSERT_NOT_NULL(small_stack);
    TEST_ASSERT_NOT_NULL(queue);
    TEST_ASSERT_NOT_NULL(buffer);
    
    // All should be different pointers
    TEST_ASSERT_NOT_EQUAL(tcb, small_stack);
    TEST_ASSERT_NOT_EQUAL(tcb, queue);
    TEST_ASSERT_NOT_EQUAL(small_stack, buffer);
    
    // Clean up
    pool_free(POOL_TCB, tcb);
    pool_free(POOL_STACK_SMALL, small_stack);
    pool_free(POOL_QCB, queue);
    pool_free(POOL_BUFFER_SMALL, buffer);
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

void test_task_stack_allocation_integration(void) {
    // Test the complete task allocation flow
    task_control_block *tcb = (task_control_block *)task_pool_alloc_tcb();
    TEST_ASSERT_NOT_NULL(tcb);

    uint32_t *stack = (uint32_t *)task_pool_alloc_stack(DEFAULT_STACK_SIZE);
    TEST_ASSERT_NOT_NULL(stack);

    // Verify they're different memory regions
    TEST_ASSERT_NOT_EQUAL((void *)tcb, (void *)stack);

    // Test cleanup
    TEST_ASSERT_TRUE(task_pool_free_tcb(tcb));
    TEST_ASSERT_TRUE(task_pool_free_stack(stack));
}

//=============================================================================
// TEST RUNNER
//=============================================================================

int main(void) {
    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_memory_pools_init_should_initialize_all_pools);
    RUN_TEST(test_memory_pools_init_should_set_correct_pool_sizes);

    // Basic allocation/deallocation tests
    RUN_TEST(test_pool_alloc_should_return_valid_pointer);
    RUN_TEST(test_pool_alloc_should_return_zeroed_memory);
    RUN_TEST(test_pool_alloc_should_return_different_pointers);
    RUN_TEST(test_pool_free_should_return_true_for_valid_pointer);
    RUN_TEST(test_pool_free_should_return_false_for_null_pointer);
    RUN_TEST(test_pool_free_should_return_false_for_invalid_pointer);
    RUN_TEST(test_pool_free_should_return_false_for_double_free);

    // Pool exhaustion tests
    RUN_TEST(test_pool_alloc_should_fail_when_pool_exhausted);
    RUN_TEST(test_pool_alloc_should_succeed_after_free);

    // Peak usage tracking tests
    RUN_TEST(test_pool_should_track_peak_usage);

    // Type-specific allocation tests
    RUN_TEST(test_task_pool_alloc_tcb_should_return_task_control_block);
    RUN_TEST(test_task_pool_alloc_stack_should_select_appropriate_size);

    // Error handling tests
    RUN_TEST(test_pool_alloc_should_handle_invalid_pool_type);
    RUN_TEST(test_pool_free_should_handle_invalid_pool_type);
    RUN_TEST(test_pool_get_stats_should_handle_invalid_pool_type);

    // Stress tests
    RUN_TEST(test_pool_should_handle_allocation_deallocation_cycles);
    RUN_TEST(test_pool_should_maintain_consistency_across_multiple_types);

    // Integration tests
    RUN_TEST(test_task_stack_allocation_integration);

    return UNITY_END();
}
