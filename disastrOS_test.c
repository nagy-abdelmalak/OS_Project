#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#include "disastrOS.h"

//mq_test
#include "disastrOS_mq.h"
#include "disastrOS_globals.h"
#include "linked_list.h"

// we need this to handle the sleep state
void sleeperFunction(void* args){
  printf("Hello, I am the sleeper, and I sleep %d\n",disastrOS_getpid());
  while(1) {
    getc(stdin);
    disastrOS_printStatus();
  }
}

void childFunction(void* args){
  printf("Hello, I am the child function %d\n",disastrOS_getpid());
  printf("I will iterate a bit, before terminating\n");
  int type=0;
  int mode=0;
  int fd=disastrOS_openResource(disastrOS_getpid(),type,mode);
  printf("fd=%d\n", fd);
  printf("PID: %d, terminating\n", disastrOS_getpid());

  for (int i=0; i<(disastrOS_getpid()+1); ++i){
    printf("PID: %d, iterate %d\n", disastrOS_getpid(), i);
    disastrOS_sleep((20-disastrOS_getpid())*5);
  }
  disastrOS_exit(disastrOS_getpid()+1);
}


void initFunction(void* args) {
  disastrOS_printStatus();
  printf("hello, I am init and I just started\n");
  disastrOS_spawn(sleeperFunction, 0);
  

  printf("I feel like to spawn 10 nice threads\n");
  int alive_children=0;
  for (int i=0; i<10; ++i) {
    int type=0;
    int mode=DSOS_CREATE;
    printf("mode: %d\n", mode);
    printf("opening resource (and creating if necessary)\n");
    int fd=disastrOS_openResource(i,type,mode);
    printf("fd=%d\n", fd);
    disastrOS_spawn(childFunction, 0);
    alive_children++;
  }

  disastrOS_printStatus();
  int retval;
  int pid;
  while(alive_children>0 && (pid=disastrOS_wait(0, &retval))>=0){ 
    disastrOS_printStatus();
    printf("initFunction, child: %d terminated, retval:%d, alive: %d \n",
	   pid, retval, alive_children);
    --alive_children;
  }
  printf("shutdown!");
  disastrOS_shutdown();
}

//mq_test
void testMsgQueue(void* args) {
    /*printf("\n=== TEST Message Queue: CREATE & DESTROY ===\n");

    MsgQueueList_print();

    printf("\n[1] Creo la MQ con id=10, max_msgs=5\n");
    int mq_id = disastrOS_mq_create(5);
    printf("Return mq_id = %d\n", mq_id);
    MsgQueueList_print();

    printf("\n[2] Distruggo la MQ con id=%d\n", mq_id);
    int res = disastrOS_mq_destroy(mq_id);
    printf("Return destroy = %d\n", res);
    MsgQueueList_print();

    printf("\nTest completato! Shutdown...\n");
    disastrOS_shutdown();*/

    int q = disastrOS_mq_create(3);
    printf("Child: created mq id=%d\n", q);

    disastrOS_mq_send(q, 123);
    disastrOS_mq_send(q, 456);
    disastrOS_mq_send(q, 789);

    int x;
    disastrOS_mq_receive(q, &x);
    printf("Child: received %d\n", x);

    disastrOS_mq_destroy(q);
    printf("Child: destroyed mq\n");

    disastrOS_exit(0);
}

void init(void* args) {
    disastrOS_spawn(testMsgQueue, 0);

    int retval;
    disastrOS_wait(0, &retval);

    disastrOS_shutdown();
}

int main(int argc, char** argv){
  /*char* logfilename=0;
  if (argc>1) {
    logfilename=argv[1];
  }
  // we create the init process processes
  // the first is in the running variable
  // the others are in the ready queue
  printf("the function pointer is: %p", childFunction);
  // spawn an init process
  printf("start\n");
  disastrOS_start(initFunction, 0, logfilename);*/

  printf("=== Avvio disastrOS (test MQ) ===\n");
  disastrOS_start(init, 0, 0);

  return 0;
}
