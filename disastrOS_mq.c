#include <stdlib.h>
#include <stdio.h>
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

    List_init(&mq->messages);
    List_init(&mq->waiting_senders);
    List_init(&mq->waiting_receivers);

    return mq;
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

void MsgQueueList_print() {
    printf("Message Queues: [");
    ListItem* aux = mq_list.first;
    while (aux) {
        MsgQueue* mq = (MsgQueue*) aux;
        printf("(id=%d, max=%d, num_msgs=%d)",
               mq->id,
               mq->max_msgs,
               mq->curr_msgs);
        aux = aux->next;
        if (aux) printf(", ");
    }
    printf("]\n");
}

/***********************
 *  Internal syscalls
 ***********************/
//Create
void internal_mq_create(){
    int max_msgs = running->syscall_args[0];

    /*if(MsgQueue_by_id(mq_id)){
        running->syscall_retvalue = DSOS_EMQ_EXISTS;
        return;
    }*/

    MsgQueue* mq = MsgQueue_alloc(max_msgs);
    if(!mq){
        running->syscall_retvalue = DSOS_EMQ_NOMEM;
        return;
    }

    mq->id = ++last_mq_id;
    List_insert(&mq_list, mq_list.last, (ListItem*) mq);
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

//send
void internal_mq_send(){
    int mq_id = running->syscall_args[0];
    int msg = running->syscall_args[1];

    MsgQueue* mq = MsgQueue_by_id(mq_id);
    if(!mq){
        running->syscall_retvalue = DSOS_EMQ_INVALID;
        return;
    }

    //Se la coda e' piena -> blocca sender
    if(mq->curr_msgs >= mq->max_msgs){
        running->status = Waiting;
        List_insert(&mq->waiting_senders, mq->waiting_senders.last, (ListItem*) running);
        internal_schedule();
        return;
    }

    //Crea msg
    MsgItem* m = malloc(sizeof(MsgItem));
    m->payload = msg;
    List_insert(&mq->messages, mq->messages.last, (ListItem*) m);
    mq->curr_msgs++;

    //Se esiste un receiver in attesa -> sveglialo
    if(mq->waiting_receivers.first){
        PCB* recevier = (PCB*) List_detach(&mq->waiting_receivers, mq->waiting_receivers.first);
        recevier->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) recevier);
    }

    running->syscall_retvalue = 0;
}

//receive
void internal_mq_receive(){
    int mq_id = running->syscall_args[0];
    int* out = (int*) running->syscall_args[1];

    MsgQueue* mq = MsgQueue_by_id(mq_id);
    if(!mq){
        running->syscall_retvalue = DSOS_EMQ_INVALID;
        return;
    }

    //Se la coda e' vuota -> blocca receiver
    if(mq->curr_msgs == 0){
        running->status = Waiting;
        List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) running);
        internal_schedule();
        return;
    }

    //Prendi un messaggio
    MsgItem* m = (MsgItem*) mq->messages.first;
    List_detach(&mq->messages, (ListItem*) m);
    mq->curr_msgs--;

    *out = m->payload;
    free(m);

    //Se esiste uno bloccato in send -> sveglialo
    if(mq->waiting_senders.first){
        PCB* sender = (PCB*) List_detach(&mq->waiting_senders, mq->waiting_senders.first);
        sender->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) sender);
    }

    running->syscall_retvalue = 0;
}