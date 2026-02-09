#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_mq.h"

#define NUM_PRODUCERS 10
#define NUM_CONSUMERS 10
#define MSG_PER_PRODUCER 20

/* done queue id — children will notify init when they finish */
static int done_mq = 0;

void producer(void* args) {
  int mq = (int)(long) args;
  int pid = disastrOS_getpid();

  for (int i = 0; i < MSG_PER_PRODUCER; i++) {
    int msg = pid * 1000 + i;
    printf("[P %d] send %d\n", pid, msg);
    disastrOS_mq_send(mq, msg);
    disastrOS_sleep(2);  // Sleep 2 ticks between sends
  }

  printf("[P %d] DONE sending %d messages\n", pid, MSG_PER_PRODUCER);
  if (done_mq) disastrOS_mq_send(done_mq, pid);
  disastrOS_exit(0);
}

void consumer(void* args) {
  int mq = (int)(long) args;
  int pid = disastrOS_getpid();
  int x;

  int total = NUM_PRODUCERS * MSG_PER_PRODUCER;
  int msgs_to_consume = total / NUM_CONSUMERS;

  for (int i = 0; i < msgs_to_consume; i++) {
    disastrOS_mq_receive(mq, &x);
    printf("[C %d] recv %d\n", pid, x);
    disastrOS_sleep(2);  // Sleep 2 ticks between receives
  }

  printf("[C %d] DONE consuming %d messages\n", pid, msgs_to_consume);
  if (done_mq) disastrOS_mq_send(done_mq, pid);
  disastrOS_exit(0);
}

/* Evitare che la ready_list diventi vuota con idle*/
void idle(void* args) {
  while (1) {
    disastrOS_preempt();
  }
}

void init(void* args) {
  printf("\n=== MQ STRESS TEST ===\n");
  printf("Producers: %d, Consumers: %d, Messages/Producer: %d\n", 
         NUM_PRODUCERS, NUM_CONSUMERS, MSG_PER_PRODUCER);
  printf("Total messages: %d\n\n", NUM_PRODUCERS * MSG_PER_PRODUCER);

  int mq = disastrOS_mq_create(5);  // Small queue to force blocking
  printf("Message queue id = %d (capacity: 5)\n", mq);

  /* spawn an idle helper so ready_list is never empty */
  disastrOS_spawn(idle, 0);

  // Spawn all producers
  printf("Spawning %d producers...\n", NUM_PRODUCERS);
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    disastrOS_spawn(producer, (void*)(long)mq);
  }

  // Spawn all consumers
  printf("Spawning %d consumers...\n\n", NUM_CONSUMERS);
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    disastrOS_spawn(consumer, (void*)(long)mq);
  }

  /* create a done queue so init can wait without calling disastrOS_wait */
  done_mq = disastrOS_mq_create(NUM_PRODUCERS + NUM_CONSUMERS);
  printf("Done queue id = %d\n\n", done_mq);

  /* Wait for all children to complete */
  int total = NUM_PRODUCERS + NUM_CONSUMERS;
  printf("Waiting for %d processes to complete...\n", total);
  for (int i = 0; i < total; ++i) {
    int who = -1;
    disastrOS_mq_receive(done_mq, &who);
    printf("  [%2d/%2d] Process %d finished\n", i+1, total, who);
  }

  printf("\n=== ALL PROCESSES COMPLETED SUCCESSFULLY ===\n");
  disastrOS_shutdown();
}

int main(int argc, char** argv){
  printf("=== Avvio disastrOS ===\n");
  disastrOS_start(init, 0, 0);
  return 0;
}
