#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

typedef uint32_t pid_t;

#define INIT_PID ((pid_t) -2)

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* Process Control Block */
struct pcb {
  pid_t pid;                /* The pid of process */
  const char* argline;      /* The command line of this process being executed */
  struct list_elem elem;    /* element in the child list */
  int32_t exitcode;         /* the exit code passed from exit() */
  bool process_waiting;     /* verifies if parent process is waiting */
  bool process_done;        /* indicates if the process is done. */
  struct semaphore sema_begin;   /* the semaphore used between start_process() and process_execute() */
  struct semaphore sema_wait;    /* the semaphore used for wait() : parent blocks until child exits  */
};

#endif /* userprog/process.h */
