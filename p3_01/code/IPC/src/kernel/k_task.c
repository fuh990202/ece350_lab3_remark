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
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

extern void kcd_task(void);

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks;		// number of non-dormant tasks
int             degrees[4];
TCB             *pq[MAX_TASKS];
int             size;

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      PROC_STACK_SIZE      |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |     |
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |
              g_k_stacks[1]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address

---------------------------------------------------------------------------*/

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

void mbx_init(TCB *p_tcb) {
    p_tcb->mbx_size = 0;
    p_tcb->mbx = NULL;
    p_tcb->head = NULL;
    p_tcb->tail = NULL;
}

// returns the index of the parent node
int parent(int i) {
        return (i - 1) / 2;
}

// return the index of the left child
int left_child(int i) {
        return 2 * i + 1;
}

// return the index of the right child
int right_child(int i) {
        return 2 * i + 2;
}

void swap(TCB ** x, TCB ** y) {
        TCB *temp = *x;
        *x = *y;
        *y = temp;
}

BOOL compare(TCB *a, TCB *b){
    if (a->prio < b->prio){
        return TRUE;
    }
    if (a->prio == b->prio && a->degree < b->degree) {
        return TRUE;
    }

    return FALSE;
}

void max_heapify(int i, int size) {

    int left = 2*i + 1;
    int right = 2*i + 2;
    int largest = i;

    if (left < size && compare(pq[left], pq[largest])) {
        largest = left;
    }
    if (right < size && compare(pq[right], pq[largest])) {
        largest = right;
    }

    if (largest == i){
    	return;
    }
    else {
    	swap(&pq[i], &pq[largest]);
        max_heapify(largest, size);
    }
//    return;
}

void enqueue(TCB *p_tcb) {
    pq[size] = p_tcb;

    // starts from bottom, move up until the heap property satisfies
    int i = size;
    while (i != 0 && compare(pq[i], pq[parent(i)])) {
        swap( &pq[parent(i)], &pq[i]);
        i = parent(i);
    }
    size++;
    return;
}

void dequeue(void) {
    if (size == 1) {
        return;
    }

    // replace the first tcb with the last tcb
    pq[0] = pq[size-1];
    size--;

    // maintain the heap property by heapifying the first tcb
    max_heapify(0, size);
    return;

}

int yield_helper() {
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    int flag = 0;
    for (int i=0; i<size; i++) {
        if (pq[i]->prio == gp_current_task->prio){

            gp_current_task->degree = degrees[(gp_current_task->prio)-100];
            degrees[(gp_current_task->prio)-100] = degrees[(gp_current_task->prio)-100] + 1;
            max_heapify(0, size);
            flag = 1;
        }
    }
    if (flag == 1) {
        return k_tsk_run_new();
    }
    return RTX_OK;
}

void mbx_unblock(TCB *p_tcb) {
    p_tcb->degree = degrees[(p_tcb->prio)-100];
    degrees[(p_tcb->prio)-100] = degrees[(p_tcb->prio)-100] + 1;
    p_tcb->state = READY;
    enqueue(p_tcb);
    g_num_active_tasks++;
    k_tsk_run_new();
}

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void)
{
    // task_t tid = gp_current_task->tid;
    // return &g_tcbs[(++tid)%g_num_active_tasks];
    return pq[0];
}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
    // init global variables
	size = 0;
    g_num_active_tasks = 0;
    for (int i=0; i<MAX_TASKS; i++){
    	g_tcbs[i].state = DORMANT;
    }
    for (int i=0; i<4; i++){
        // degrees[i] = k_mem_alloc(sizeof(int));
        degrees[i] = 0;
    }
    extern U32 SVC_RESTORE;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    // create the first null task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;

    mbx_init(p_tcb);

    pq[0] = p_tcb;
    size++;
    g_num_active_tasks++;
    gp_current_task = p_tcb;

    // create the rest of the tasks
    p_taskinfo = task_info;
    task_t curr_tid = 0;
    for ( int i = 0; i < num_tasks; i++ ) {
        TCB *p_tcb = NULL;
        if (p_taskinfo->ptask == &kcd_task) {
            p_tcb = &g_tcbs[TID_KCD];
        } else {
            curr_tid++;
            p_tcb = &g_tcbs[curr_tid];
        }
        p_tcb->u_stack_size = 0;
        p_tcb->u_sp = 0;
        p_tcb->u_stack_hi = 0;
        p_tcb->prio = p_taskinfo->prio;
        p_tcb->degree = degrees[(p_taskinfo->prio)-100];
        degrees[(p_taskinfo->prio)-100] = degrees[(p_taskinfo->prio)-100] + 1;

        mbx_init(p_tcb);
        int task_id = 0;
        if (p_taskinfo->ptask == &kcd_task) {
            task_id = TID_KCD;
        } else {
            task_id = curr_tid;
        }

        if (k_tsk_create_new(p_taskinfo, p_tcb, task_id) == RTX_OK) {
            enqueue(p_tcb);
        	g_num_active_tasks++;
        }
        p_taskinfo++;
    }
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;
    extern U32 K_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    p_tcb ->tid = tid;
    p_tcb->state = READY;

    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (KERN_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }
    p_tcb->k_stack_hi = (U32)sp;

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);
        p_tcb->ptask = p_taskinfo->ptask;

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        *(--sp) = (U32) k_alloc_p_stack(tid);
        if (sp == NULL) {
            return RTX_ERR;
        }
        p_tcb->u_stack_hi = *sp;
        p_tcb->u_sp = sp;

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // Set msp points to the kernal stack
    p_tcb->msp = sp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        STR     SP, [R0, #TCB_MSP_OFFSET]   ; save SP to p_old_tcb->msp
K_RESTORE
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_MSP_OFFSET]   ; restore msp of the gp_current_task

        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;

    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();

    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        #ifdef DEBUG_0
    	printf("============= run new task \n\r");
        #endif /* DEBUG_0 */

        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        if (p_tcb_old->state != BLK_MSG) {
        	p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
        }
        k_tsk_switch(p_tcb_old);            // switch stacks
    }
    else {
        #ifdef DEBUG_0
        printf("============= keep old task \n\r");
        #endif /* DEBUG_0 */
    }

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
    #ifdef DEBUG_0
    printf("================ task yielded\n\r");
    #endif /* DEBUG_0 */
	if (gp_current_task->prio == PRIO_NULL){
		return k_tsk_run_new();
	}
    return yield_helper();
    // return k_tsk_run_new();
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

    // if capacity reached maximum, cannot create more task and return err
    if (g_num_active_tasks >= MAX_TASKS) {
        return RTX_ERR;
    }

    //  when the stack size is too small, return err
    //  note: maybe not RAM_SIZE
    if (stack_size < PROC_STACK_SIZE || stack_size > RAM_SIZE || stack_size%8 != 0) {
        return RTX_ERR;
    }

    // return error if invalid priority level
    if (prio != HIGH && prio != MEDIUM && prio != LOW && prio != LOWEST) {
        return RTX_ERR;
    }

    // find a empty space for tcb, store the index as tid
    for (int i=0; i<MAX_TASKS; i++) {
        if (g_tcbs[i].state == DORMANT) {
            *task = i;
            break;
        }
    }

    TCB * p_tcb = &g_tcbs[*task];
    p_tcb->prio = prio;
    p_tcb->priv = 0;
    p_tcb->u_stack_size = stack_size;
    // p_tcb->degree = *(degrees[prio-100]);
    // *(degrees[prio-100]) = *(degrees[prio-100]) + 1;
    p_tcb->degree = degrees[prio-100];
    degrees[prio-100] = degrees[prio-100] + 1;

    mbx_init(p_tcb);

    RTX_TASK_INFO   task_info;
    task_info.ptask = task_entry;
    task_info.priv = 0;

    if (k_tsk_create_new(&task_info, p_tcb, *task) == RTX_OK) {
        // push the new task into priority queue
        enqueue(p_tcb);

        g_num_active_tasks++;
        #ifdef DEBUG_0
        printf("============= task created: %d \n\r", *task);
        #endif /* DEBUG_0 */
        return k_tsk_run_new();
    }

    return RTX_ERR;

}

void k_tsk_exit(void)
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */

    TCB *p_tcb_old = gp_current_task;

    if (p_tcb_old->tid == TID_NULL) {
        return;
    }

    // do we really need NULL check???
    if (p_tcb_old!= NULL) {
        p_tcb_old->state = DORMANT;
    }
    g_num_active_tasks--;

    if (p_tcb_old->priv == 0 && p_tcb_old->state == DORMANT) {
        k_mem_dealloc((U32 *)((unsigned int)p_tcb_old->u_stack_hi - p_tcb_old->u_stack_size));
    }

    // get next task from priority queue
    if (gp_current_task != pq[0]) {
        for (int i=1; i<size; i++) {
            if (pq[i] == gp_current_task) {
                pq[i] = pq[size-1];
                size--;
                max_heapify(i, size);
                break;
            }
        }
    } else {
        dequeue();
    }
    gp_current_task = scheduler();
    gp_current_task->state = RUNNING;

    #ifdef DEBUG_0
    printf("============= task exited: %d \n\r", p_tcb_old->tid);
    printf("============= task started: %d \n\r", gp_current_task->tid);
    #endif /* DEBUG_0 */

    k_tsk_switch(p_tcb_old);
    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio)
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */

    // return error if invalid TID
    if (task_id <= 0 || task_id >= MAX_TASKS) {
        return RTX_ERR;
    }

    // return error if invalid priority level
    if (prio != HIGH && prio != MEDIUM && prio != LOW && prio != LOWEST) {
        return RTX_ERR;
    }

    TCB *update_task = &g_tcbs[task_id];

    // A user-mode task cannot change priority of kernel-mode task
    if (gp_current_task->priv == 0 && update_task->priv == 1) {
        return RTX_ERR;
    }

    update_task->prio = prio;
    // update_task->degree = *(degrees[prio-100]);
    // *(degrees[prio-100]) = *(degrees[prio-100]) + 1;
    update_task->degree = degrees[prio-100];
    degrees[prio-100] = degrees[prio-100] + 1;

    for (int i=0; i<size; i++) {
        if (pq[i]->tid == task_id) {
            swap( &pq[i], &pq[size-1]);
            size--;
            enqueue(update_task);
            break;
        }
    }
    return k_tsk_run_new();
}

int k_tsk_get(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */
    if (buffer == NULL) {
        return RTX_ERR;
    }
    /* The code fills the buffer with some fake task information.
       You should fill the buffer with correct information    */

    if (task_id < 0 || task_id >= MAX_TASKS) {
        return RTX_ERR;
    }
    TCB *curr_tcb = &g_tcbs[task_id];

    if (curr_tcb == NULL || curr_tcb->state == DORMANT) {
        return RTX_ERR;
    }

    if (curr_tcb->priv == 0) {
        buffer->u_sp = *(curr_tcb->u_sp);
        buffer->u_stack_size = curr_tcb->u_stack_size;
        buffer->u_stack_hi = curr_tcb->u_stack_hi;
    } else {
        buffer->u_sp = 0;
        buffer->u_stack_size = 0;
        buffer->u_stack_hi = 0;
    }

    buffer->tid = curr_tcb->tid;
    buffer->prio = curr_tcb->prio;
    buffer->state = curr_tcb->state;
    buffer->priv = curr_tcb->priv;
    buffer->ptask = curr_tcb->ptask;
    buffer->k_sp = (U32) (curr_tcb->msp);
    buffer->k_stack_size = KERN_STACK_SIZE;
    buffer->k_stack_hi = curr_tcb->k_stack_hi;


    return RTX_OK;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task, RTX_MSG_HDR *msg_hdr, U32 num_msgs)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(struct timeval_rt *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
