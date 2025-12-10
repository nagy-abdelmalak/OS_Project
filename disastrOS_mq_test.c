#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include "disastrOS.h"
#include "disastrOS_mq.h"
#include "disastrOS_globals.h"

// // #define MQ_CAPACITY 2

// // // ======================================
// // // PRODUCER
// // // ======================================
// // void producer(void* args) {
// //   int mq = (int)(long) args;
// //   int pid = disastrOS_getpid();

// //   printf("[P %d] SEND %d\n", pid, pid*10+1);
// //   disastrOS_mq_send(mq, pid*10+1);

// //   printf("[P %d] SEND %d\n", pid, pid*10+2);
// //   disastrOS_mq_send(mq, pid*10+2);

// //   printf("[P %d] DONE\n", pid);

// //   MsgQueueList_print();
// //   disastrOS_exit(0);
// // }

// // // ======================================
// // // CONSUMER
// // // ======================================
// // void consumer(void* args) {
// //   int mq = (int)(long) args;
// //   int pid = disastrOS_getpid();
// //   int x;

// //   printf("[C %d] WAIT RECV 1\n", pid);
// //   disastrOS_mq_receive(mq, &x);
// //   printf("[C %d] GOT %d\n", pid, x);

// //   printf("[C %d] WAIT RECV 2\n", pid);
// //   disastrOS_mq_receive(mq, &x);
// //   printf("[C %d] GOT %d\n", pid, x);

// //   printf("[C %d] DONE\n", pid);

// //   MsgQueueList_print();
// //   disastrOS_exit(0);
// // }

// // // ======================================
// // // INIT — main orchestrator
// // // ======================================
// // void init(void* args) {
// //   printf("=== FINAL MQ TEST ===\n");

// //   int mq = disastrOS_mq_create(MQ_CAPACITY);
// //   printf("[INIT] MQ CREATED id=%d\n", mq);

// //   // spawn 2 producers
// //   disastrOS_spawn(producer, (void*)(long)mq);
// //   disastrOS_spawn(producer, (void*)(long)mq);

// //   // spawn 3 consumers
// //   disastrOS_spawn(consumer, (void*)(long)mq);
// //   disastrOS_spawn(consumer, (void*)(long)mq);
// //   disastrOS_spawn(consumer, (void*)(long)mq);

// //   // wait all children
// //   int retval;
// //   int pid;
// //   for (int i=0; i<5; i++){
// //     pid = disastrOS_wait(0, &retval);
// //     printf("[INIT] child %d terminated\n", pid);
// //   }

// //   // destroy queue
// //   int r = disastrOS_mq_destroy(mq);
// //   printf("[INIT] MQ DESTROY = %d\n", r);

// //   disastrOS_shutdown();
// // }

// void producer(void* args) {
//   int mq = (int)(long) args;
//   int pid = disastrOS_getpid();

//   printf("[P %d] SEND 11\n", pid);
//   disastrOS_mq_send(mq, 11);
//   printf("[P %d] SENT 11 OK\n", pid);

//   printf("[P %d] SEND 22\n", pid);
//   disastrOS_mq_send(mq, 22);
//   printf("[P %d] SENT 22 OK\n", pid);

//   printf("[P %d] DONE\n", pid);
//   disastrOS_exit(0);
// }

// void consumer(void* args) {
//   int mq = (int)(long) args;
//   int pid = disastrOS_getpid();
//   int x;

//   printf("[C %d] RECV 1...\n", pid);
//   disastrOS_mq_receive(mq, &x);
//   printf("[C %d] GOT %d\n", pid, x);

//   printf("[C %d] RECV 2...\n", pid);
//   disastrOS_mq_receive(mq, &x);
//   printf("[C %d] GOT %d\n", pid, x);

//   printf("[C %d] DONE\n", pid);
//   disastrOS_exit(0);
// }

// void init(void* args) {
//   printf("=== MQ TEST (Producer <-> Consumer, capacity=1) ===\n");

//   int mq = disastrOS_mq_create(1);
//   printf("[INIT] MQ CREATED id=%d\n", mq);

//   disastrOS_spawn(producer, (void*)(long)mq);
//   disastrOS_spawn(consumer, (void*)(long)mq);

//   int pid, retval;
//   pid = disastrOS_wait(0, &retval);
//   printf("[INIT] child %d terminated\n", pid);
//   pid = disastrOS_wait(0, &retval);
//   printf("[INIT] child %d terminated\n", pid);

//   disastrOS_shutdown();
// }

// int main(int argc, char** argv){
//   printf("=== Avvio disastrOS ===\n");
//   disastrOS_start(init, 0, 0);

//   return 0;
// }

#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define MSG_PER_PRODUCER 5

void producer(void* args) {
  int mq = (int)(long) args;
  int pid = disastrOS_getpid();

  for (int i = 0; i < MSG_PER_PRODUCER; i++) {
    int msg = pid * 1000 + i;  // messaggio unico
    printf("[P %d] send %d\n", pid, msg);
    disastrOS_mq_send(mq, msg);
  }

  printf("[P %d] DONE\n", pid);
  disastrOS_exit(0);
}

void consumer(void* args) {
  int mq = (int)(long) args;
  int pid = disastrOS_getpid();
  int x;

  int total = NUM_PRODUCERS * MSG_PER_PRODUCER;

  for (int i = 0; i < total / NUM_CONSUMERS; i++) {
    disastrOS_mq_receive(mq, &x);
    printf("[C %d] recv %d\n", pid, x);
  }

  printf("[C %d] finished\n", pid);
  disastrOS_exit(0);
}

void init(void* args) {
  printf("=== MQ STRESS TEST ===\n");

  int mq = disastrOS_mq_create(2);
  printf("mq id = %d\n", mq);

  // Avvia producer
  for (int i=0; i<NUM_PRODUCERS; i++) {
    disastrOS_spawn(producer, (void*)(long)mq);
  }

  // Avvia consumer
  for (int i=0; i<NUM_CONSUMERS; i++) {
    disastrOS_spawn(consumer, (void*)(long)mq);
  }

  // Attendi tutti
  int alive = NUM_PRODUCERS + NUM_CONSUMERS;
  int pid, retval;

  while (alive > 0) {
    pid = disastrOS_wait(0, &retval);
    printf("INIT: child %d terminated (retval=%d) alive=%d\n",
           pid, retval, alive);
    alive--;
  }

  disastrOS_shutdown();
}

int main(int argc, char** argv){
  printf("=== Avvio disastrOS ===\n");
  disastrOS_start(init, 0, 0);
  return 0;
}
