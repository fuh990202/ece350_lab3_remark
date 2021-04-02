/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/

/**
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
#ifdef DEBUG_0
#include "printf.h"
#endif  /* DEBUG_0 */
#define node_size 16    //sizeof(node_t) + 4 as padding

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = KERN_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.c
const U32 g_p_stack_size = PROC_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][KERN_STACK_SIZE >> 2] __attribute__((aligned(8)));

//process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][PROC_STACK_SIZE >> 2] __attribute__((aligned(8)));

extern TCB *gp_current_task;
// strcut to store each free region
typedef struct __node_t {
    unsigned int size;
    struct __node_t *next;
    task_t ownership;
} node_t;


// global head
node_t *head;
/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

U32* k_alloc_k_stack(task_t tid)
{
    return g_k_stacks[tid+1];
}

U32* k_alloc_p_stack(task_t tid)
{
    // return g_p_stacks[tid+1];
    TCB *task = &g_tcbs[tid];

    node_t *allocated_ptr = k_mem_alloc(task->u_stack_size);
    node_t *ptr_header = (node_t *)((unsigned int)allocated_ptr - node_size);
    ptr_header->ownership = tid;
    return (U32 *)((unsigned int)allocated_ptr + task->u_stack_size);
    //return k_mem_alloc(task->u_stack_size);
}

int k_mem_init(void) {
    unsigned int end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
#ifdef DEBUG_0
    printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
    printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END);
#endif /* DEBUG_0 */

    if (RAM_END - end_addr - node_size+1 <= 0) {
        return RTX_ERR;
    }
    // initiate the global head
    head = (node_t *)end_addr;
    head->size = 0;

    // instantiate the free region
    int currAddr = end_addr + node_size;
    node_t *freeRegion = (node_t *)currAddr;
    freeRegion->size = RAM_END - end_addr - node_size+1;
    freeRegion->next = NULL;

    // global head->next points to the initial free region
    head->next = freeRegion;
    return RTX_OK;
}

void* k_mem_alloc(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_alloc: requested memory size = %d\r\n", size);
#endif /* DEBUG_0 */
    if (size <= 0) { // size <= 0
        return NULL;
    }

    // create prev and curr pointer to keep track the free region
    node_t *prev = head;
    node_t *curr = head->next;
    unsigned int remainder = size % 8;
    unsigned int real_size = size;
    if (remainder != 0) {
        real_size = real_size + (8 - size % 8);
    }

    while (curr != NULL) {

        if (curr->size == real_size + node_size) {
            prev->next = curr->next;
            curr->size = real_size;
            return (node_t *)((unsigned int)curr+node_size);
        }
        else if (curr->size > real_size + node_size) {
            // define the updated free region (original size - used size)
            node_t *updated_free_region = (node_t *)((unsigned int)curr + real_size + node_size);
            updated_free_region->size = curr->size - real_size - node_size;
            // update the linked list of free regions
            updated_free_region->next = curr->next;
            prev->next = updated_free_region;
            // set curr->size to the real size to keep track how much memory the ptr is allocated
            curr->size = real_size;
            curr -> ownership = gp_current_task ->tid;
            // return (node_t *)((unsigned int)curr+node_size);
            return (node_t *)((unsigned int)curr+node_size);
        }
        prev = curr;
        curr = curr->next;
    }

    // No enough space to allocate
    return NULL;
}

int k_mem_dealloc(void *ptr) {
#ifdef DEBUG_0
    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32) ptr);
#endif /* DEBUG_0 */

    if (ptr == NULL) {
        return RTX_ERR;
    }

    node_t *ptr_header = (node_t *)((unsigned int)ptr - node_size);

    if (gp_current_task->tid != ptr_header->ownership) {
        return RTX_ERR;
    }

    node_t *curr = head;
    node_t *curr_next = head->next;
    while (curr_next != NULL) {

        if (curr < ptr && curr_next > ptr) {
            node_t *end_of_left = (node_t *)((unsigned int)curr + curr->size);
            node_t *ptr_header = (node_t *)((unsigned int)ptr - node_size);
            node_t *end_of_ptr = (node_t *)((unsigned int)ptr + ptr_header->size);



            if (end_of_left > ptr_header) {
                return RTX_ERR;
            }

            // Condition 1: merge both
            if ( end_of_left == ptr_header && end_of_ptr == curr_next) {
                #ifdef DEBUG_0
                    printf("Condition 1: merge both\r\n");
                #endif /* DEBUG_0 */

                curr->size = curr->size + ptr_header->size + curr_next->size + node_size;
                curr->next = curr_next->next;
            }

            //condition 2: merge left
            else if (end_of_left == ptr_header) {
                #ifdef DEBUG_0
                    printf("Condition 2: merge left\r\n");
                #endif /* DEBUG_0 */

                curr->size = curr->size + ptr_header->size + node_size;
            }

            //condition 3: merge right
            else if (end_of_ptr == curr_next) {
                #ifdef DEBUG_0
                    printf("Condition 3: merge right\r\n");
                #endif /* DEBUG_0 */

                ptr_header->size = ptr_header->size + curr_next->size + node_size;
                curr->next = ptr_header;
                ptr_header->next = curr_next->next;
            }

            //condtion4: base condition
            else {
                #ifdef DEBUG_0
                    printf("Condition 4: No merge\r\n");
                #endif /* DEBUG_0 */

                curr->next = ptr_header;
                ptr_header->size += node_size;
                ptr_header->next = curr_next;
            }
            return RTX_OK;
        }

        curr = curr_next;
        curr_next = curr_next->next;
    }

    if (ptr > curr) {
        node_t *end_of_left = (node_t *)((unsigned int)curr + curr->size);
        node_t *ptr_header = (node_t *)((unsigned int)ptr - node_size);
        // ... -> free -> ptr -> NULL
        if (end_of_left == ptr_header) {
            curr->size = curr->size + ptr_header->size + node_size;
            return RTX_OK;
        }

        // ... -> free -> ocuppied -> ptr -> NULL        curr->next = ptr_header;
        ptr_header->size += node_size;
        curr->next = ptr_header;
        ptr_header->next = NULL;
        return RTX_OK;
    }

    return RTX_OK;
}

int k_mem_count_extfrag(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
#endif /* DEBUG_0 */
    node_t *curr = head->next;
    int count = 0;
    while (curr != NULL) {
        if (curr->size < size) {
            count += 1;
        }
        curr = curr->next;
    }
    return count;
}

int get_free_memory() {
    node_t *curr = head;
    unsigned int sum = 0;
    while (curr != NULL) {
        sum += curr->size;
        curr = curr->next;
    }
    return sum;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
