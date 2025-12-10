#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_mq.h"

#define NUM_PRODUCERS 5
#define NUM_CONSUMERS 5
#define MSG_PER_PRODUCER 50

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
