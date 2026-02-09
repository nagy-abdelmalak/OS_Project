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
static int List_contains(ListHead* head, ListItem* item){
  ListItem* it = head->first;
  while(it){
    if(it == item) return 1;
    it = it->next;
  }
  return 0;
}

MsgQueue* MsgQueue_alloc(int max_msgs) {
    MsgQueue* mq = malloc(sizeof(MsgQueue));
    if(!mq) return NULL;

    mq->max_msgs = max_msgs;
    mq->curr_msgs = 0;
    mq->id = -1; // id will be set when inserted in the global list

    List_init(&mq->messages);
    List_init(&mq->waiting_senders);
    List_init(&mq->waiting_receivers);
    List_init(&mq->pending_sends);

    return mq;
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

    // libera i sender in attesa
    PendingSend* ps = (PendingSend*) mq->pending_sends.first;
    while(ps){
        PendingSend* next = (PendingSend*) ps->list.next;
        List_detach(&mq->pending_sends, (ListItem*) ps);
        if(ps->msg) free(ps->msg);
        free(ps);
        ps = next;
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

void messageQueuePrint(MsgQueue* mq) {
    if (!mq) {
        printf("MsgQueue is NULL\n");
        return;
    }
    printf("Print mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
    printf(" Messages: [");
    ListItem* aux = mq->messages.first;
    while (aux) {
        MsgItem* msg = (MsgItem*) aux;
        printf("%d", msg->payload);
        aux = aux->next;
        if (aux) printf(", ");
    }
    printf("]\n");
    printf(" Waiting Senders: ");
    PCBPtrList_print(&mq->waiting_senders);
    printf(" Waiting Receivers: ");
    PCBPtrList_print(&mq->waiting_receivers);
    printf(" PendingSends: ");
    PendingSend* ps = (PendingSend*) mq->pending_sends.first;
    printf("[");
    while(ps){
        printf("(m=%d, pid=%d), ", ps->msg ? ps->msg->payload : -1, ps->sender->pcb->pid);
        ps = (PendingSend*) ps->list.next;
        if(ps) printf(", ");
    }
    printf("]\n");
}

MsgItem* MsgItem_alloc(int payload) {
    MsgItem* msg = malloc(sizeof(MsgItem));
    if(!msg) return NULL;

    msg->payload = payload;
    msg->list.prev = NULL;
    msg->list.next = NULL;

    return msg;
}

void wait_schedule() {
    // schedule
    if (ready_list.first){
          PCB* next_process=(PCB*) List_detach(&ready_list, ready_list.first);
          running->status= Waiting;
          if(!List_contains(&waiting_list, (ListItem*) running))
              List_insert(&waiting_list, waiting_list.last, (ListItem*) running);
          next_process->status=Running;
          running=next_process;
          disastrOS_debug("Schedule to next process, pid:%d\n", running->pid);
      } else {
          if(List_contains(&waiting_list, (ListItem*) init_pcb))
              List_detach(&waiting_list, (ListItem*) init_pcb);
          init_pcb->status = Running;
          running = init_pcb;
        //   printf("Schedule to IDLE (running_pid %d)\n", running->pid);
      }

    // debug prints (enabled only when _DISASTROS_DEBUG_)
#ifdef _DISASTROS_DEBUG_
    printf("\nWait_schedule\n Ready: ");
    PCBList_print(&ready_list);
    printf("\n Waiting: ");
    PCBList_print(&waiting_list);
#endif
}

PCB* awake_waiter(ListHead* pending_list, ListItem* to_detach){
    disastrOS_debug("Checking to awake waiter...\n"); //debug
    if(pending_list->first){
        PCBPtr* pcb_ptr = (PCBPtr*) List_detach(pending_list, to_detach ? to_detach : pending_list->first);
        disastrOS_debug("Awakening a waiter from pending_list\n"); //debug
        assert(pcb_ptr);
        PCB* pcb = pcb_ptr->pcb;
        if(List_contains(&waiting_list, (ListItem*) pcb))
            List_detach(&waiting_list, (ListItem*) pcb);
        // debug
        disastrOS_debug("Awakening pid:%d\n", pcb->pid);
        pcb->status = Ready;
        if(!List_contains(&ready_list, (ListItem*) pcb))
            List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
        PCBPtr_free(pcb_ptr);

        // debug prints (only when enabled)
#ifdef _DISASTROS_DEBUG_
        printf("\nawake_waiter\n Ready: ");
        PCBList_print(&ready_list);
        printf("\n Waiting: ");
        PCBList_print(&waiting_list);
#endif
        return pcb; // return the awakened PCB
    }
    return NULL;
}

static PendingSend* PendingSend_alloc(PCBPtr* sender, MsgItem* msg){
    PendingSend* ps = (PendingSend*) malloc(sizeof(PendingSend));
    if(!ps) return NULL;
    ps->list.prev = 0;
    ps->list.next = 0;
    ps->sender = sender;
    ps->msg = msg;
    return ps;
}

static PendingSend* pending_send_pop(MsgQueue* mq){
    if(!mq) return NULL;
    if(!mq->pending_sends.first) return NULL;
    PendingSend* ps = (PendingSend*) List_detach(&mq->pending_sends, mq->pending_sends.first);
    return ps;
}

static void pending_send_push(MsgQueue* mq, PendingSend* ps){
    if(!mq || !ps) return;
    List_insert(&mq->pending_sends, mq->pending_sends.last, (ListItem*) ps);
}

/***********************
 *  Internal syscalls
 ***********************/

//Create
void internal_mq_create(){
    int max_msgs = running->syscall_args[0];

    if(max_msgs <= 0){
        disastrOS_debug("Error(create): invalid max_msgs=%d\n", max_msgs); //debug
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
        printf("Error(destroy): MsgQueue not found for id=%d\n", mq_id); //debug
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
            disastrOS_debug("Error(send): MsgQueue not found for id=%d\n", mq_id); //debug
      running->syscall_retvalue = DSOS_EMQ_INVALID;
      return;
    }

    disastrOS_debug("Sending: mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);

    //Crea msg
    MsgItem* m = MsgItem_alloc(msg);

    //Se la coda e' piena -> blocca sender
    if(mq->curr_msgs >= mq->max_msgs){ 
        PCBPtr* self_ptr = PCBPtr_alloc(running);
        assert(self_ptr);  

        PendingSend* ps = PendingSend_alloc(self_ptr, m);
        if(!ps){
            free(m);
            running->syscall_retvalue = DSOS_EMQ_NOMEM;
            return;
        } 
        pending_send_push(mq, ps);
        
        disastrOS_debug("Insert Sender in waiting: mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
        List_insert(&mq->waiting_senders, mq->waiting_senders.last, (ListItem*) self_ptr);     
        wait_schedule();

        awake_waiter(&mq->waiting_receivers, NULL); //Se esistono waiting_receivers -> svegliane uno
        running->syscall_retvalue = 0;
    #ifdef _DISASTROS_DEBUG_
        messageQueuePrint(mq); //debug
        printf("Sender blocked, msg=%d pending, args1:%d\n", msg, (int) running->syscall_args[1]); //debug
    #endif
        return;
    }

    //Se esistono receivers in attesa -> consegna diretta
    // copy message directly into the receiver's out pointer (no memory separation)
    PCB* receiver_pcb = awake_waiter(&mq->waiting_receivers, NULL);
    if (receiver_pcb){
        int* outptr = (int*) receiver_pcb->syscall_args[1];
        if (outptr) *outptr = m->payload;
        receiver_pcb->syscall_retvalue = 0;
        running->syscall_retvalue = 0;
        free(m); /* message consumed by direct delivery */
        disastrOS_debug("Delivered message directly to waiting receiver pid:%d\n", receiver_pcb->pid);
        return;
    }

    // Add message to queue
    List_insert(&mq->messages, mq->messages.last, (ListItem*) m);
    mq->curr_msgs++;

    disastrOS_debug("msg=%d added curr=%d\n", m->payload, mq->curr_msgs);

#ifdef _DISASTROS_DEBUG_
    printf("Sent! mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
    messageQueuePrint(mq);
#endif
    running->syscall_retvalue = 0;
}

//receive
void internal_mq_receive(){
    int mq_id = running->syscall_args[0];
    int* out = (int*) running->syscall_args[1];

    MsgQueue* mq = MsgQueue_by_id(mq_id);
        if(!mq){
            disastrOS_debug("Error(receive): MsgQueue not found for id=%d\n", mq_id); //debug
      running->syscall_retvalue = DSOS_EMQ_INVALID;
      return;
    }
        disastrOS_debug("Receiving: mq(id:%d, curr:%d, max:%d), running-pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
#ifdef _DISASTROS_DEBUG_
        messageQueuePrint(mq);
#endif

    //Se la coda e' vuota -> blocca receiver
    if(mq->curr_msgs == 0){
        disastrOS_debug("No messages available mq(id:%d, curr:%d, max:%d), blocking receiver pid:%d\n", mq->id, mq->curr_msgs, mq->max_msgs, running->pid); //debug
        PCBPtr* self_ptr = PCBPtr_alloc(running);
        assert(self_ptr);
        List_insert(&mq->waiting_receivers, mq->waiting_receivers.last, (ListItem*) self_ptr);
        wait_schedule();

        awake_waiter(&mq->waiting_senders, NULL); // wake one waiting sender if present
#ifdef _DISASTROS_DEBUG_
        printf("Receiver awoken pid:%d\n", running->pid);
        messageQueuePrint(mq);
#endif
        running->syscall_retvalue = 0;
        return;
    }

    // pop a message
#ifdef _DISASTROS_DEBUG_
    messageQueuePrint(mq);
#endif
    MsgItem* m = (MsgItem*) mq->messages.first;
    List_detach(&mq->messages, (ListItem*) m);
    mq->curr_msgs--;
    if(out) *out = m->payload;
    free(m);
    
    PendingSend* ps = pending_send_pop(mq);
    if(ps){
        disastrOS_debug("Processing pending send from pid:%d, msg:%d\n", ps->sender->pcb->pid, ps->msg ? ps->msg->payload : -1);
        if(mq->waiting_senders.first){
            List_insert(&mq->messages, mq->messages.last, (ListItem*) ps->msg);
            mq->curr_msgs++;
#ifdef _DISASTROS_DEBUG_
            messageQueuePrint(mq);
            printf("Waking up sender pid:%d\n", ps->sender->pcb->pid);
            PCBPtrList_print(&mq->waiting_senders);
#endif
            awake_waiter(&mq->waiting_senders, (ListItem*) ps->sender); // wake one waiting sender
        }
        /* free the PendingSend container (the msg was moved into the queue)
           do not free ps->msg here */
        free(ps);
        disastrOS_debug("Pending send processed\n"); //debug
    }

#ifdef _DISASTROS_DEBUG_
    printf("Received msg=%d, mq(id:%d, curr=%d, max=%d), running-pid=%d\n", *out, mq->id, mq->curr_msgs, mq->max_msgs, running->pid);
    messageQueuePrint(mq);
#endif
    running->syscall_retvalue = 0;
}
