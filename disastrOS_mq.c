#include <stdlib.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_mq.h"
#include "disastrOS_pcb.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_constants.h"

static int last_mq_id = 100;

/****************************************
 *  Helper functions for MsgQueue
 ****************************************/
MsgQueue* MsgQueue_alloc(int max_msgs) {
    MsgQueue* mq = malloc(sizeof(MsgQueue));
    if(!mq) return NULL;

    mq->max_msgs = max_msgs;
    mq->curr_msgs = 0;
    mq->id = -1; // id will be set when inserted in the global list

    List_init(&mq->messages);
    List_init(&mq->waiting_senders);
    List_init(&mq->waiting_receivers);

    return mq;
}

static int List_contains(ListHead* head, ListItem* item){
  ListItem* it = head->first;
  while(it){
    if(it == item) return 1;
    it = it->next;
  }
  return 0;
}

void MsgQueue_free(MsgQueue* mq){
    if(!mq) return;

    // libera i messaggi residui
    MsgItem* m = (MsgItem*) mq->messages.first;
    while(m) {
        MsgItem* next = (MsgItem*) m->list.next;
        List_detach(&mq->messages, (ListItem*) m);
        free(m);
        m = next;
    }
    // rimuove la MQ dalla lista globale
    if(List_contains(&mq_list, (ListItem*) mq))
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
        printf("(id=%d, max=%d, curr=%d)",
               mq->id,
               mq->max_msgs,
               mq->curr_msgs);
        aux = aux->next;
        if (aux) printf(", ");
    }
    printf("]\n");
}

void wait_schedule() {
    // schedule
    if (ready_list.first){
          PCB* next_process=(PCB*) List_detach(&ready_list, ready_list.first);
          running->status= Waiting;
          List_insert(&waiting_list, waiting_list.last, (ListItem*) running);
          next_process->status=Running;
          running=next_process;
          printf("Schedule to next process, pid:%d\n", running->pid); //debug
      } else {
          printf("No other process ready to run, deadlock possible!\n");
      }

    //debug
    printf("\nwait_schedule\n Ready: ");
    PCBList_print(&ready_list);
    printf("\n Waiting: ");
    PCBList_print(&waiting_list);
    //debug
}

MsgItem* awake_waiter(ListHead* waiters_list) {
    if(waiters_list->first){
        PCBPtr* pcb_ptr = (PCBPtr*) List_detach(waiters_list, waiters_list->first);
        assert(pcb_ptr);
        PCB* pcb = (PCB*) List_detach(waiters_list, (ListItem*) pcb_ptr->pcb);

        //debug
        printf("Awaking waiter pid:%d(pending_msg:%d), pending_out:%d\n", pcb->pid, pcb_ptr->pending_msg ? pcb_ptr->pending_msg->payload : -1, pcb_ptr->pending_out ? *(pcb_ptr->pending_out) : -1);

        MsgItem* msg_item = (MsgItem*) pcb_ptr->pending_msg;
        if (pcb_ptr->pending_out) *(pcb_ptr->pending_out) = msg_item->payload;

        pcb->status = Ready;
        List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
        PCBPtr_free(pcb_ptr);

        //debug
        printf("\nawake_waiter\n Ready: ");
        PCBList_print(&ready_list);
        printf("\n Waiting: ");
        PCBList_print(&waiting_list);
        //debug
        return msg_item;
    }
    return NULL;
}

/***********************
 *  Internal syscalls
 ***********************/

//Create
void internal_mq_create(){
    int max_msgs = running->syscall_args[0];

    if(max_msgs <= 0){
        running->syscall_retvalue = DSOS_EMQ_INVALID;
        return;
    }

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
    if(mq->curr_msgs > 0 || mq->waiting_senders.first || mq->waiting_receivers.first){
        running->syscall_retvalue = DSOS_EMQ_INUSE;
        return;
    }

    MsgQueue_free(mq);
    running->syscall_retvalue = 0;
}

//send
void internal_mq_send(){
    int mq_id = running->syscall_args[0];
    int msg = (int) running->syscall_args[1];
  
    MsgQueue* mq = MsgQueue_by_id(mq_id);
    if(!mq){
      running->syscall_retvalue = DSOS_EMQ_INVALID;
      return;
    }

    //debug
    printf("Sending:\n mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);

    //Crea msg
    MsgItem* m = (MsgItem*) malloc(sizeof(MsgItem));
    if(!m){
        running->syscall_retvalue = DSOS_EMQ_NOMEM;
        return;
    }
    m->payload = msg;

    //Se la coda e' piena -> blocca sender
    if(mq->curr_msgs >= mq->max_msgs){  
        PCBPtr* running_ptr = PCBPtr_alloc(running);
        assert(running_ptr);
        running_ptr->pending_msg = m; // assegna il messaggio da inviare al pcb_ptr   
        printf("Insert Sender in waiting:\n mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
        List_insert(&mq->waiting_senders, mq->waiting_senders.last, (ListItem*) running_ptr);     
        wait_schedule();

        awake_waiter(&mq->waiting_receivers); //Se esistono waiting_receivers -> svegliane uno
        return;
    }

    // Allora aggiungi il messaggio
    List_insert(&mq->messages, mq->messages.last, (ListItem*) m);
    mq->curr_msgs++;
    //debug
    printf("msg=%d added curr=%d\n", m->payload, mq->curr_msgs);

    awake_waiter(&mq->waiting_receivers); //Se esistono waiting_receivers -> svegliane uno

    //debug
    printf("Sent!\n mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
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
    
    printf("Receiving:\n mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
    //Se la coda e' vuota -> blocca receiver
    if(!mq->curr_msgs){
        PCBPtr* running_ptr = PCBPtr_alloc(running);
        assert(running_ptr);
        running_ptr->pending_out = out; // assegna il puntatore dove scrivere il messaggio ricevuto
        printf("Insert receiver in waiting:\n mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
        List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) running_ptr);
        wait_schedule();

        awake_waiter(&mq->waiting_senders); //Se esistono waiting_senders -> svegliane uno
        running->syscall_retvalue = 0;
        return;
    }

    //pop un messaggio
    MsgItem* m = (MsgItem*) mq->messages.first;
    List_detach(&mq->messages, (ListItem*) m);
    mq->curr_msgs--;
    if(out) *out = m->payload;
    free(m);
    
    m = awake_waiter(&mq->waiting_senders); //Se esistono waiting_senders -> svegliane uno
    if(m){
        List_insert(&mq->messages, mq->messages.last, (ListItem*) m);
        mq->curr_msgs++;
    }

    //debug
    printf("Received *out=%d curr=%d\n", *out, mq->curr_msgs);
    running->syscall_retvalue = 0;
}