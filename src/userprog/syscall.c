#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"


static void syscall_handler (struct intr_frame *);

// Our Implementation
static int32_t get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
void sys_exit (int);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// Check if the address is valid and has no segfault error. Returns true if there isnt else false.
static bool 
is_valid_address(void * esp, uint8_t size){
  for (uint8_t i = 0; i < size; i++)
  {
    if (get_user(((uint8_t *)esp)+i) == -1){
      return false;
    }
  }
  return true;
}


static void
syscall_handler (struct intr_frame *f)
{
  int syscall_num;

  if(is_valid_address(f->esp,4)) {
    syscall_num = *((int*)f->esp);
  }
  else {
    sys_exit (-1);
  }

  switch (syscall_num) {
  case SYS_HALT:                   /* Halt the operating system. */
  case SYS_EXIT:                   /* Terminate this process. */
    {
      int status;
      if (is_valid_address(f->esp + 4, 4))
        status = *((int*)f->esp+1);
      else
        sys_exit (-1);
      sys_exit(status);
      break;
    }
  case SYS_EXEC:                   /* Start another process. */
  case SYS_WAIT:                   /* Wait for a child process to die. */
  case SYS_CREATE:                 /* Create a file. */
  case SYS_REMOVE:                 /* Delete a file. */
  case SYS_OPEN:                   /* Open a file. */
  case SYS_FILESIZE:               /* Obtain a file's size. */
  case SYS_READ:                   /* Read from a file. */
    goto unimplemented;
  case SYS_WRITE:                  /* Write to a file. */
  {
    int file, return_code;
    const void *buffer;
    unsigned size;

    if(is_valid_address(f->esp + 4, 4)) 
      file = *((int*)f->esp+1);
    else
      sys_exit(-1);

    if(is_valid_address(f->esp + 8, 4)) 
      buffer = *((int*)f->esp+2);
    else
      sys_exit(-1);

    if(is_valid_address(f->esp + 12, 4)) 
      size = *((int*)f->esp+3);
    else
      sys_exit(-1);

    if (get_user((const uint8_t*) buffer) == -1) {
      thread_exit();
    }

    // To write to console, we write all the buffer to putbuf in one go.
    putbuf(buffer, size);
    return_code = size;

    f->eax = (uint32_t) return_code;
    break;
  }
  case SYS_SEEK:                   /* Change position in a file. */
  case SYS_TELL:                   /* Report current position in a file. */
  case SYS_CLOSE:                  /* Close a file. */

unimplemented:
  default:
    printf("[ERROR] system call %d is unimplemented!\n", syscall_num);
    sys_exit(-1);
    break;
  }

}

void sys_exit(int status) {
  struct thread *t = thread_current();
  struct pcb *pcb = t->pcb;
  printf("%s: exit(%d)\n", t->name, status);
  // Update the PCB
  if(pcb != NULL){
    pcb->process_done = true;
    pcb->exitcode = status;
    sema_up (&pcb->sema_wait);
  }
  thread_exit();
}

/* Reads a byte at user virtual address UADDR.
Returns the byte value if successful, -1 if a segfault occurred. */
static int
get_user (const uint8_t *uaddr)
{
  //UADDR must be below PHYS_BASE.
  if (uaddr >= PHYS_BASE)
    return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
  : "=&a" (result) : "m" (*uaddr));
  return result; 
}

/* Writes BYTE to user address UDST. 
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte) 
{
  // UDST must be below PHYS_BASE.
  if (udst >= PHYS_BASE)
    return -1;
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
  : "=&a" (error_code), "=m" (*udst) : "q" (byte)); 
  return error_code != -1;
}