#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct lock fs_lock;

static void syscall_handler (struct intr_frame *);

// Our Implementation
static int32_t get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
void sys_exit (int);
int sys_open(const char* file);
void sys_seek(int fd, unsigned pos);
unsigned sys_tell(int fd);
pid_t sys_exec (const char *argline);
int sys_write(int fd, const void *buffer, unsigned size);
struct file_descriptor* find_file_descriptor(struct thread *, int fd);

void
syscall_init (void)
{
  lock_init (&fs_lock);
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
    {
      shutdown_power_off();
      break;
    }
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
    {
      void* argline;
      if (is_valid_address(f->esp + 4, 4))
        argline = *((int*)f->esp+1);
      else
        sys_exit (-1);

      int return_code = sys_exec((const char*) argline);
      f->eax = (uint32_t) return_code;
      break;
    }

  case SYS_WAIT:                   /* Wait for a child process to die. */
    {
      pid_t pid;
      if (is_valid_address(f->esp + 4, 4))
        pid = *((int*)f->esp+1);
      else
        sys_exit (-1); 

      int ret = process_wait(pid);
      f->eax = (uint32_t) ret;
      break;
    }
  case SYS_CREATE:                 /* Create a file. */
    {
      const char* filename;
      unsigned initial_size;

      if (is_valid_address(f->esp + 4, sizeof(filename)))
       filename = *((int*)f->esp+1);
      else
       sys_exit (-1);

      if (is_valid_address(f->esp + 8, sizeof(initial_size)))
        initial_size = *((int*)f->esp+2);
      else
        sys_exit (-1);

      if (get_user((const uint8_t*) filename) == -1) {
        sys_exit(-1);
      }
      
      lock_acquire (&fs_lock);
      f->eax = filesys_create(filename, initial_size);
      lock_release (&fs_lock);
      break;
    }

  case SYS_REMOVE:                 /* Delete a file. */
    {
      const char* filename;

      if (is_valid_address(f->esp + 4, sizeof(filename)))
        filename = *((int*)f->esp+1);
      else
        sys_exit (-1);

      if (get_user((const uint8_t*) filename) == -1) {
        sys_exit(-1);
      }

      lock_acquire (&fs_lock);
      f->eax = filesys_remove(filename);
      lock_release (&fs_lock);
      break;
    }
  case SYS_OPEN:                   /* Open a file. */
    {
      const char* filename;

      if(is_valid_address(f->esp + 4, sizeof(filename))) 
        filename = *((int*)f->esp+1);
      else
        sys_exit(-1); 

      f->eax = sys_open(filename);
      break;
    }
  case SYS_FILESIZE:               /* Obtain a file's size. */
    {
      int fd;
      if(is_valid_address(f->esp + 4, 4)) 
        fd = *((int*)f->esp+1);
      else
        sys_exit(-1);

      struct file_descriptor* file_des;

      file_des = find_file_descriptor(thread_current(), fd);

      f->eax = file_length(file_des->file);
      break;
    }
  case SYS_READ:                   /* Read from a file. */
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
        sys_exit(-1);
      }

      return_code = sys_read(file, buffer, size);
      f->eax = (uint32_t) return_code;
      break;
    }
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
      sys_exit(-1);
    }

    return_code = sys_write(file, buffer, size);

    f->eax = (uint32_t) return_code;
    break;
  }
  case SYS_SEEK:                   /* Change position in a file. */
  {
      int fd;
      unsigned pos;

      if(is_valid_address(f->esp + 4, sizeof(fd))) 
        fd = *((int*)f->esp+1);
      else
        sys_exit(-1);

      if(is_valid_address(f->esp + 8, sizeof(pos))) 
        pos = *((int*)f->esp+2);
      else
        sys_exit(-1);

      struct file_descriptor* file_des = find_file_descriptor(thread_current(), fd);

      if(file_des && file_des->file) {
        file_seek(file_des->file, pos);
      }

      break;
  }
  case SYS_TELL:                   /* Report current position in a file. */
  {
      int fd;
      unsigned return_code;

      if(is_valid_address(f->esp + 4, sizeof(fd))) 
        fd = *((int*)f->esp+1);
      else
        sys_exit(-1);

      struct file_descriptor* file_des = find_file_descriptor(thread_current(), fd);

      if(file_des && file_des->file) {
        f->eax = (uint32_t) file_tell(file_des->file);
      }
      else
        f->eax = (uint32_t) -1; 

      break;
  }
  case SYS_CLOSE:                  /* Close a file. */
  {
      int fd;
      if(is_valid_address(f->esp + 4, sizeof(fd))) 
        fd = *((int*)f->esp+1);
      else
        sys_exit(-1);

      sys_close(fd);
      break;
  }

  default:
    printf("[ERROR] system call %d is unimplemented!\n", syscall_num);
    sys_exit(-1);
    break;
  }

}

int sys_write(int fd, const void *buffer, unsigned size) {

  // Validating the memory
  if (get_user((const uint8_t*) buffer + size - 1) == -1) {
      sys_exit(-1);
  }

  int ret;

  // To write to the console
  if(fd == 1) { 
    putbuf(buffer, size);
    ret = size;
  }
  else {
    // write the buffer into the file
    struct file_descriptor* file_des = find_file_descriptor(thread_current(), fd);

    if(file_des && file_des->file) {
      ret = file_write(file_des->file, buffer, size);
    }
    else // If the file couldnt be opened.
      ret = -1;
  }

  return ret;
}

pid_t sys_exec(const char *argline) {

  if (get_user((const uint8_t*) argline) == -1) {
    sys_exit(-1);
  }
  
  lock_acquire (&fs_lock); 
  tid_t tid = process_execute(argline);
  lock_release (&fs_lock); 
  return tid;
}

int sys_read(int fd, void *buffer, unsigned size) {

  // Validating the memory
  if (get_user((const uint8_t*) buffer + size - 1) == -1) {
      sys_exit(-1);
  }

  // To write to the console
  if(fd == 0) { 
    unsigned i;
    for(i = 0; i < size; i++) {
      ((uint8_t *)buffer)[i] = input_getc();
    }
    return size;
  }
  else {
    // read from file into the buffer
    struct file_descriptor* file_des = find_file_descriptor(thread_current(), fd);

    if(file_des && file_des->file) {
      return file_read(file_des->file, buffer, size);
    }
    else // If the file couldnt be opened.
      return -1;
  }
}

void sys_exit(int status) {
  struct thread *t = thread_current();
  struct pcb *pcb = t->pcb;
  printf("%s: exit(%d)\n", t->name, status);

  if(pcb != NULL){
    pcb->process_done = true;
    pcb->exitcode = status;
    sema_up (&pcb->sema_wait);
  }
  thread_exit();
}

int sys_open(const char* file) {
  struct file* open_file;
  struct file_descriptor* fd = palloc_get_page(0);

  // Validating the memory
  if (get_user((const uint8_t*) file) == -1) {
    sys_exit(-1);
  }

  lock_acquire (&fs_lock);
  open_file = filesys_open(file);
  lock_release (&fs_lock);

  if (!open_file) {
    return -1;
  }

  fd->file = open_file;

  struct list* fd_list = &thread_current()->file_descriptors;
  if (!list_empty(fd_list)) {
    // Since the index starts from 0, we add 1.
    fd->id = (list_entry(list_back(fd_list), struct file_descriptor, elem)->id) + 1;
  }
  else {
    // 0, 1, 2 are reserved for stdin, stdout, stderr
    fd->id = 3;
  }
  list_push_back(fd_list, &(fd->elem));

  return fd->id;
}

void sys_close(int fd) {
  lock_acquire (&fs_lock);
  struct file_descriptor* file_des = find_file_descriptor(thread_current(), fd);

  if(file_des && file_des->file) {
    file_close(file_des->file);
    list_remove(&(file_des->elem));
    palloc_free_page(file_des);
  }

  lock_release (&fs_lock);


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

// This is a helper function which will get the file descriptor corresponding to the input fd
struct file_descriptor*
find_file_descriptor(struct thread *t, int fd)
{
  ASSERT (t != NULL);

  if (fd < 3) {
    return NULL;
  }

  struct list_elem *i;

  if (!list_empty(&t->file_descriptors)) {
    for(i = list_begin(&t->file_descriptors); i != list_end(&t->file_descriptors); i = list_next(i))
    {
      struct file_descriptor *file_des = list_entry(i, struct file_descriptor, elem);
      if(file_des->id == fd) {
        return file_des;
      }
    }
  }

  return NULL; 
}