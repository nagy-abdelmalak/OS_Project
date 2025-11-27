#include <stdlib.h>
#include "disastrOS.h"
#include "disastrOS_mq.h"
#include "disastrOS_pcb.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_constants.h"

static int last_mq_id = 100;

/****************************************
 *  Allocazione e Lookup Message Queue
 ****************************************/
MsgQueue* MsgQueue_alloc(int max_msgs) {
    MsgQueue* mq = malloc(sizeof(MsgQueue));
    if(!mq) return NULL;

    mq->max_msgs = max_msgs;
    mq->curr_msgs = 0;
    mq->id = ++last_mq_id;

    List_init(&mq->messages);
    List_init(&mq->waiting_senders);
    List_init(&mq->waiting_receivers);

    List_insert(&mq_list, mq_list.last, (ListItem*) mq);
}