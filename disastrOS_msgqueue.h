#pragma once
#include "linked_list.h"

typedef struct MsgItem {
    ListItem list;
    void* payload; // il messaggio
    int size
} MsgItem;

typedef struct MsgQueue {
    ListItem list;              // per lista globale delle queue
    ListHead messages;          // messaggi presenti nella coda
    ListHead waiting_senders;   // processi bloccati su send()
    ListHead waiting_receivers; // processi bloccati su receive()
    int max_msgs;
    int curr_msgs;
    int id;                     // id della coda di riferimento
} MsgQueue;