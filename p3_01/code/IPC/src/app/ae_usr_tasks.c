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
 * @file        a usr_tasks.c
 * @brief       Two user/unprivileged  tasks: task1 and task2
 *
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *****************************************************************************/

#include "ae_usr_tasks.h"
#include "rtx.h"
#include "Serial.h"
#include "printf.h"

typedef struct identifier {
    U8      val;
    task_t  tid;
} Identifier;

typedef struct key {
    struct key *next;
    U8         val;
} Key;

/**
 * @brief: a dummy task1
 */
void task1(void)
{
    task_t tid;
    RTX_TASK_INFO task_info;
    
    SER_PutStr (0,"task1: entering \n\r");
    /* do something */
    tsk_create(&tid, &task2, LOW, 0x200);  /*create a user task */
    tsk_get(tid, &task_info);
    tsk_set_prio(tid, LOWEST);
    /* terminating */
    tsk_exit();
}

/**
 * @brief: a dummy task2
 */
void task2(void)
{
    SER_PutStr (0,"task2: entering \n\r");
    /* do something */
    long int x = 0;
    int ret_val = 10;
    int i = 0;
    int j = 0;
    for (i = 1;;i++) {
            char out_char = 'a' + i%10;
            for (j = 0; j < 5; j++ ) {
                SER_PutChar(0,out_char);
            }
            SER_PutStr(0,"\n\r");

            for ( x = 0; x < 5000000; x++); // some artifical delay
            if ( i%6 == 0 ) {
                SER_PutStr(0,"usr_task2 before yielding cpu.\n\r");
                ret_val = tsk_yield();
                SER_PutStr(0,"usr_task2 after yielding cpu.\n\r");
                printf("usr_task2: ret_val=%d\n\r", ret_val);
            }
        }
    /* terminating */
    //tsk_exit();
}


void task3 (void) {
    mbx_create(30);
    
    // register Identifier
    U8 *buf = mem_alloc(sizeof(RTX_MSG_HDR)+1);
    RTX_MSG_HDR *ptr = (void *)buf;
    ptr->length = sizeof(RTX_MSG_HDR)+1;
    ptr->type = KCD_REG;
    buf += sizeof(RTX_MSG_HDR);
    *(buf) = 'a';
    send_msg(TID_KCD, ptr);
    mem_dealloc((void *)ptr);
    

    U8 *buf2 = mem_alloc(sizeof(RTX_MSG_HDR)+1);
    RTX_MSG_HDR *ptr2 = (void *)buf2;
    ptr2->length = sizeof(RTX_MSG_HDR)+1;
    ptr2->type = KCD_REG;
    buf2 += sizeof(RTX_MSG_HDR);
    *(buf2) = 'b';

    while (1) {
        #ifdef DEBUG_0
        printf("======== task3 entering\n\r");
        #endif /* ! DEBUG_0 */
        RTX_MSG_HDR *message = mem_alloc(sizeof(RTX_MSG_HDR)+10);
        task_t tid;
        recv_msg(&tid, message, sizeof(RTX_MSG_HDR)+10);

        #ifdef DEBUG_0
        if (message->type == KCD_CMD) {
            printf("========= Got KCD_CMD message: \n\r");

            U8 *msg_ptr = (void *)message;
            msg_ptr += sizeof(RTX_MSG_HDR);
            for (int i=0; i<message->length - sizeof(RTX_MSG_HDR); i++) {
                printf("%c", *msg_ptr);
                msg_ptr++;
            }
        }
        #endif /* ! DEBUG_0 */
    }
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
