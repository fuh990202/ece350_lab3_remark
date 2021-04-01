/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "k_task.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */

    // Fail if the calling task already have a mailbox
    if (gp_current_task->mbx != NULL) {
        return RTX_ERR;
    }

    // Fail if size is less than MIN_MBX_SIZE
    if (size < MIN_MBX_SIZE) {
        return RTX_ERR;
    }

    gp_current_task->mbx_size = size;
    gp_current_task->mbx = k_mem_alloc(size);

    // Fail if no memory available
    if (gp_current_task->mbx == NULL) {
        return RTX_ERR;
    }

    return 0;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */

    TCB *receiver = &g_tcbs[receiver_tid];

    if (receiver == NULL || receiver->state == DORMANT) {
        return RTX_ERR;
    }

    if (receiver->mbx == NULL) {
        return RTX_ERR;
    }

    if (buf == NULL) {
        return RTX_ERR;
    }

    // some variables will be used multiple times
    unsigned int end_of_mbx = (unsigned int)receiver->mbx + receiver->mbx_size;
    unsigned int buf_length = ((RTX_MSG_HDR *)buf)->length;

    unsigned int real_size = get_real_size(buf_length+1);

    if (buf_length < MIN_MSG_SIZE || real_size > receiver->mbx_size) {
        return RTX_ERR;
    }

    // insert first message
    if (receiver->head == NULL && receiver->tail == NULL) {
        #ifdef DEBUG_0
        printf("============== Condition 1: Inserting first message");
        #endif /* DEBUG_0 */

        // store the message (hard copy)
        RTX_MSG_HDR *msg_buffer = receiver->mbx;
        msg_buffer->length = buf_length;
        msg_buffer->type = ((RTX_MSG_HDR *)buf)->type;
        for (int i=sizeof(RTX_MSG_HDR); i<buf_length; i++) {
            *((U8 *)msg_buffer+i) = *((U8 *)buf+i);
        }

        task_t *msg_ptr = (task_t *)(receiver->mbx);
        *(msg_ptr + buf_length) = gp_current_task->tid;

        //should we consider byte alignment, for example 4 bytes align??
        receiver->head = receiver->mbx;
        receiver->tail = (RTX_MSG_HDR *)((unsigned int)(receiver->mbx) + real_size);

    }

    // Condition for Tail exceeding Head (mbx full)
    else if ((unsigned int)receiver->tail == (unsigned int)receiver->head || ((unsigned int)receiver->head > (unsigned int)receiver->tail && (unsigned int)receiver->head - (unsigned int)receiver->tail < real_size)) {
        #ifdef DEBUG_0
        printf("============== Condition 2: Tail Exceeds Head, Space not enough");
        #endif /* DEBUG_0 */
        return RTX_ERR;
    }

    // Condition for Tail is full, but head has space
    //we should subtract the length of sender_tid
    else if ((unsigned int)receiver->tail == end_of_mbx || end_of_mbx - (unsigned int)receiver->tail < real_size) {
        if (end_of_mbx > (unsigned int)receiver->tail) {
            *((U8 *)receiver->tail) = '#';
        }
        // check if front got space
        if ((unsigned int)receiver->head - (unsigned int)receiver->mbx >= real_size) {
            #ifdef DEBUG_0
            printf("============== Condition 3: Inserting message to the front");
            #endif /* DEBUG_0 */

            // store the message (hard copy)
            RTX_MSG_HDR *msg_buffer = receiver->mbx;
            msg_buffer->length = buf_length;
            msg_buffer->type = ((RTX_MSG_HDR *)buf)->type;
            for (int i=sizeof(RTX_MSG_HDR); i<buf_length; i++) {
                *((U8 *)msg_buffer+i) = *((U8 *)buf+i);
            }

            task_t *msg_ptr = (task_t *)(receiver->mbx);
            *(msg_ptr + buf_length) = gp_current_task->tid;
            receiver->tail = (RTX_MSG_HDR *)((unsigned int)(receiver->mbx) + real_size);

        } else {
            #ifdef DEBUG_0
            printf("============== Condition 4: Space not enough");
            #endif /* DEBUG_0 */
            // No space for the message
            return RTX_ERR;
        }
    }
    // Condition for appending the message to the Tail
    else {
        #ifdef DEBUG_0
        printf("============== Condition 5: Normal Condition");
        #endif /* DEBUG_0 */

        // store the message (hard copy)
        RTX_MSG_HDR *msg_buffer = receiver->tail;
        msg_buffer->length = buf_length;
        msg_buffer->type = ((RTX_MSG_HDR *)buf)->type;
        for (int i=sizeof(RTX_MSG_HDR); i<buf_length; i++) {
            *((U8 *)msg_buffer+i) = *((U8 *)buf+i);
        }

        task_t *msg_ptr = (task_t *)(receiver->tail);
        *(msg_ptr + buf_length) = gp_current_task->tid;
        receiver->tail = (RTX_MSG_HDR *)((unsigned int)(receiver->tail) + real_size);

    }

    if (receiver->state == BLK_MSG) {
        #ifdef DEBUG_0
        printf("========== receiver mbx released: %d\n\r", receiver_tid);
        #endif /* DEBUG_0 */
        mbx_unblock(receiver);
    }
    return RTX_OK;
}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */

    if (gp_current_task->mbx == NULL || buf == NULL) {
        return RTX_ERR;
    }

    // if mbx is empty, set state to BLK_MSG
    if (gp_current_task->head == NULL) {
        gp_current_task->state = BLK_MSG;
        dequeue();
        g_num_active_tasks--;
        k_tsk_run_new();
        return RTX_ERR;
    }

    RTX_MSG_HDR *msg_recv = gp_current_task->head;
    unsigned int end_of_mbx = (unsigned int)gp_current_task->mbx + gp_current_task->mbx_size;

    if (msg_recv->length <= len) {
        *sender_tid = *((task_t *)msg_recv + msg_recv->length);

        ((RTX_MSG_HDR *)buf)->length = msg_recv->length;
        ((RTX_MSG_HDR *)buf)->type = msg_recv->type;
        for (int i=sizeof(RTX_MSG_HDR); i<msg_recv->length; i++) {
            *((U8 *)buf+i) = *((U8 *)msg_recv+i);
        }
    }

    // move head to the next message
    gp_current_task->head = (RTX_MSG_HDR *)((unsigned int)gp_current_task->head + get_real_size(msg_recv->length + 1));
    // mbx becomes empty
    if (gp_current_task->head == gp_current_task->tail) {
        #ifdef DEBUG_0
        printf(" ============== mbx become empty");
        #endif /* DEBUG_0 */
        
        gp_current_task->head = NULL;
        gp_current_task->tail = NULL;
    }
    // Tail is at front (circular queue)
    else if ((unsigned int)gp_current_task->head == end_of_mbx || *((U8 *)gp_current_task->head) == '#') {
    // else if ((unsigned int)(gp_current_task->head) > (unsigned int)(gp_current_task->tail)) {
        gp_current_task->head = gp_current_task->mbx;
    }

    if (msg_recv->length > len) {
        return RTX_ERR;
    }

    return 0;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

unsigned int get_real_size(int size) {
    unsigned int remainder = size % 4;
    unsigned int real_size = size;
    if (remainder != 0) {
        real_size = real_size + (4 - size % 4);
    }
    return real_size;
}