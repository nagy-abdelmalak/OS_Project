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

void MsgQueue_free(MsgQueue* mq){
    // libera i messaggi residui
    MsgItem* m = (MsgItem*) mq->messages.first;
    while(m) {
        MsgItem* next = (MsgItem*) m->list.next;
        List_detach(&mq->messages, (ListItem*) m);
        free(m);
        m = next;
    }
    // rimuove la MQ dalla lista globale
    List_detach(&mq_list, (ListItem*) mq);
    free(mq);
}

MsgQueue* MsgQueue_by_id(int id){
    ListItem* it = mq_list.first;
    while (it) {
        MsgQueue* mq = (MsgQueue*) it;
        if(mq->id == id)
            return mq;
        it = it->next;
    }
    return NULL;
}

/***********************
 *  Internal syscalls
 ***********************/
//Create
void internal_mq_create(){
    int max_msgs = running->syscall_args[0];
    MsgQueue* mq = MsgQueue_alloc(max_msgs);

    if(!mq){
        running->syscall_retvalue = DSOS_EMQ_NOMEM;
        return;
    }
    running->syscall_retvalue = mq->id;
}

//Destroy
void internal_mq_destroy(){
    int mq_id = running->syscall_args[0];

    MsgQueue* mq = MsgQueue_by_id(mq_id);
    if(!mq){
        running->syscall_retvalue = DSOS_EMQ_INVALID;
        return;
    }

    //Se ci sono processi in attesa -> errore
    if(mq->waiting_senders.first || mq->waiting_receivers.first){
        running->syscall_retvalue = DSOS_EMQ_INUSE;
        return;
    }

    MsgQueue_free(mq);
    running->syscall_retvalue = 0;
}