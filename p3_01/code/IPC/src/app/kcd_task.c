/* The KCD Task Template File */
#include "ae_usr_tasks.h"
#include "rtx.h"
#include "Serial.h"
#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */
#include "kcd_task.h"

typedef struct identifier {
    U8      val;
    task_t  tid;
} Identifier;

typedef struct key {
    struct key *next;
    U8         val;
} Key;

void kcd_task(void)
{
    mbx_create(KCD_MBX_SIZE);
    // init array to store all regitered identifiers
    Identifier *identifies[62];
    int count = 0;

    // init header for current input
    Key *head = mem_alloc(sizeof(Key));
    head->val = '%';
    head->next = NULL;
    Key *curr = head;
    int length = 0;
    while (1) {
        #ifdef DEBUG_0
        printf("kcd_task while loop \n\r");
        #endif /* ! DEBUG_0 */
        // TODO: what size should each buffer allocate?
        U8 *message = mem_alloc(sizeof(RTX_MSG_HDR)+1);
        task_t tid;
        int res = recv_msg(&tid, message, sizeof(RTX_MSG_HDR)+1);

        if (res == RTX_ERR) {
            continue;
        }

        // Command registration
        if (((RTX_MSG_HDR *)message)->type == KCD_REG && ((RTX_MSG_HDR *)message)->length == sizeof(RTX_MSG_HDR)+1) {
            #ifdef DEBUG_0
            printf("======== received registered identifier by: %d\n\r", tid);
            #endif /* ! DEBUG_0 */

            int flag = 0;
            for (int i=0; i<count; i++) {
                if (identifies[i]->val == *(message+sizeof(RTX_MSG_HDR))) {
                    identifies[i]->tid = tid;
                    flag = 1;
                    break;
                }
            }
            if (flag == 0) {
                Identifier *curr_identifier = mem_alloc(sizeof(Identifier));
                curr_identifier->val = *(message+sizeof(RTX_MSG_HDR));
                curr_identifier->tid = tid;
                identifies[count] = curr_identifier;
                count++;
            }
        }
        else if (((RTX_MSG_HDR *)message)->type == KEY_IN) {
            U8 input = *(message+sizeof(RTX_MSG_HDR));
            if (input == 0x0d) {
                int buffer_length = sizeof(RTX_MSG_HDR) + length-1;
                // create message for the corresponding registered task
                U8 *buf = mem_alloc(buffer_length);
                RTX_MSG_HDR *msg_ptr = (void *)buf;
                msg_ptr->length = buffer_length;
                msg_ptr->type = KCD_CMD;
                buf += sizeof(RTX_MSG_HDR);

                // get identifier and search through registered identifier
                U8 msg_identifier;
                // first input is return key -> invalid command
                if (head->next == NULL) {
                    SER_PutStr(0,"Invalid Command\n\r");
                    continue;
                } else {
                    msg_identifier = head->next->next->val;
                }

                curr = head->next->next;
                while (curr != NULL) {
                    *buf = curr->val;
                    buf++;
                    curr = curr->next;
                }


                int receiver_tid = -1;
                for (int i=0; i<count; i++) {
                    if (identifies[i]->val == msg_identifier) {
                        receiver_tid = identifies[i]->tid;
                    }
                }
                
                if (head->next->val != '%' || length > 63) {
                    SER_PutStr(0,"Invalid Command\n\r");
                }

                else if (receiver_tid <0 || receiver_tid >= MAX_TASKS) {
                    SER_PutStr(0,"Command cannot be processed\n\r");
                }
                else {
                    int res = send_msg(receiver_tid, (void *)msg_ptr);
                    if (res != 0) {
                        SER_PutStr(0,"Command cannot be processed\n\r");
                    } else {
                        #ifdef DEBUG_0
                        printf("Message sent to: %d\n\r", receiver_tid);
                        #endif /* ! DEBUG_0 */
                    }
                }

                head->next = NULL;
                curr = head;
                length = 0;

            }
            else {
                #ifdef DEBUG_0
            	printf("========== Interrupt val: %c\n\r", input);
                #endif /* ! DEBUG_0 */
                Key *tmp = mem_alloc(sizeof(Key));
                tmp->val = input;
                tmp->next = NULL;
                curr->next = tmp;
                curr = curr->next;
                length++;
            }
        }
    }
}
