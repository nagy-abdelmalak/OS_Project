#pragma once
#include "linked_list.h"
#include "disastrOS.h"

typedef struct MsgItem {
    ListItem list;
    int payload;
} MsgItem;

typedef struct PendingSend {
    ListItem list;
    PCBPtr* sender;
    MsgItem* msg;
} PendingSend;

typedef struct MsgQueue {
    ListItem list;              // per lista globale delle queue
    ListHead messages;          // messaggi presenti nella coda
    ListHead waiting_senders;   // processi bloccati su send()
    ListHead waiting_receivers; // processi bloccati su receive()
    ListHead pending_sends;     // messaggi in attesa di essere inviati
    int max_msgs;
    int curr_msgs;
    int id;                     // id della coda di riferimento
} MsgQueue;

//Print
void MsgQueueList_print();
void messageQueuePrint(MsgQueue* mq);

// Wrapper
int disastrOS_mq_create(int max_msgs);
int disastrOS_mq_destroy(int id);
int disastrOS_mq_send(int id, int msg);
int disastrOS_mq_receive(int id, int* msg);