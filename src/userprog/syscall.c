#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
 
 
static int sys_halt (void);
static int sys_exit (int status);
static int sys_exec (const char *ufile);
static int sys_wait (tid_t);
static int sys_create (const char *ufile, unsigned initial_size);
static int sys_remove (const char *ufile);
static int sys_open (const char *ufile);
static int sys_filesize (int handle);
static int sys_read (int handle, void *udst_, unsigned size);
static int sys_write (int handle, void *usrc_, unsigned size);
static int sys_seek (int handle, unsigned position);
static int sys_tell (int handle);
static int sys_close (int handle);

//Extra system calls added by subhro for memory mapping implementation
static int sys_mmap (int handle, void *addr);
static int sys_munmap (int mapping);
 
static void syscall_handler (struct intr_frame *);
static void copy_in (void *, const void *, size_t);
 
/* Serializes file system operations. */
//static struct lock fs_lock;
 
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  //lock_init (&fs_lock);
}
 
/* System call handler. */
static void
syscall_handler (struct intr_frame *f) 
{
  typedef int syscall_function (int, int, int);

  /* A system call. */
  struct syscall 
    {
      size_t arg_cnt;           /* Number of arguments. */
      syscall_function *func;   /* Implementation. */
    };

  /* Table of system calls. */
  static const struct syscall syscall_table[] =
    {
      {0, (syscall_function *) sys_halt},
      {1, (syscall_function *) sys_exit},
      {1, (syscall_function *) sys_exec},
      {1, (syscall_function *) sys_wait},
      {2, (syscall_function *) sys_create},
      {1, (syscall_function *) sys_remove},
      {1, (syscall_function *) sys_open},
      {1, (syscall_function *) sys_filesize},
      {3, (syscall_function *) sys_read},
      {3, (syscall_function *) sys_write},
      {2, (syscall_function *) sys_seek},
      {1, (syscall_function *) sys_tell},
      {1, (syscall_function *) sys_close},
     // The remainder of the system calls have been added for memory mapping 
      {2, (syscall_function *) sys_mmap},
      {1, (syscall_function *) sys_munmap},
    };

  const struct syscall *sc;
  unsigned call_nr;
  int args[3];

  /* Get the system call. */
  copy_in (&call_nr, f->esp, sizeof call_nr);
  if (call_nr >= sizeof syscall_table / sizeof *syscall_table)
    thread_exit ();
  sc = syscall_table + call_nr;

  /* Get the system call arguments. */
  ASSERT (sc->arg_cnt <= sizeof args / sizeof *args);
  memset (args, 0, sizeof args);
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * sc->arg_cnt);

  /* Execute the system call,
     and set the return value. */
  f->eax = sc->func (args[0], args[1], args[2]);
}
 
/* Returns true if UADDR is a valid, mapped user address,
   false otherwise. */
static bool
verify_user (const void *uaddr) 
{
  return (uaddr < PHYS_BASE
          && pagedir_get_page (thread_current ()->pagedir, uaddr) != NULL);
}
 
/* Copies a byte from user address USRC to kernel address DST.
   USRC must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static inline bool
get_user (uint8_t *dst, const uint8_t *usrc)
{
  int eax;
  asm ("movl $1f, %%eax; movb %2, %%al; movb %%al, %0; 1:"
       : "=m" (*dst), "=&a" (eax) : "m" (*usrc));
  return eax != 0;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static inline bool
put_user (uint8_t *udst, uint8_t byte)
{
  int eax;
  asm ("movl $1f, %%eax; movb %b2, %0; 1:"
       : "=m" (*udst), "=&a" (eax) : "q" (byte));
  return eax != 0;
}
 
/* Copies SIZE bytes from user address USRC to kernel address
   DST.
   Call thread_exit() if any of the user accesses are invalid. */
static void
copy_in (void *dst_, const void *usrc_, size_t size) 
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;
 
  for (; size > 0; size--, dst++, usrc++) 
    if (usrc >= (uint8_t *) PHYS_BASE || !get_user (dst, usrc)) 
      thread_exit ();
}
 
/* Creates a copy of user string US in kernel memory
   and returns it as a page that must be freed with
   palloc_free_page().
   Truncates the string at PGSIZE bytes in size.
   Call thread_exit() if any of the user accesses are invalid. */
static char *
copy_in_string (const char *us) 
{
  char *ks;
  size_t length;
 
  ks = palloc_get_page (0);
  if (ks == NULL) 
    thread_exit ();
 
  for (length = 0; length < PGSIZE; length++)
    {
      if (us >= (char *) PHYS_BASE || !get_user (ks + length, us++)) 
        {
          palloc_free_page (ks);
          thread_exit (); 
        }
       
      if (ks[length] == '\0')
        return ks;
    }
  ks[PGSIZE - 1] = '\0';
  return ks;
}
 
/* Halt system call. */
static int
sys_halt (void)
{
  shutdown_power_off ();
}
 
/* Exit system call. */
static int
sys_exit (int exit_code) 
{
  thread_current ()->wait_status->exit_code = exit_code;
  thread_exit ();
  NOT_REACHED ();
}
 
/* Exec system call. */
static int
sys_exec (const char *ufile) 
{
  tid_t tid;
  char *kfile = copy_in_string (ufile);

  //lock_acquire (&fs_lock);
  tid = process_execute (kfile);
  //lock_release (&fs_lock);
 
  palloc_free_page (kfile);
 
  return tid;
}
 
/* Wait system call. */
static int
sys_wait (tid_t child) 
{
  return process_wait (child);
}
 
/* Create system call. */
static int
sys_create (const char *ufile, unsigned initial_size) 
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  //lock_acquire (&fs_lock);
  ok = filesys_create (kfile, initial_size);
  //lock_release (&fs_lock);
 
  palloc_free_page (kfile);
 
  return ok;
}
 
/* Remove system call. */
static int
sys_remove (const char *ufile) 
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  //lock_acquire (&fs_lock);
  ok = filesys_remove (kfile);
  //lock_release (&fs_lock);
 
  palloc_free_page (kfile);
 
  return ok;
}
 
/* A file descriptor, for binding a file handle to a file. */
struct file_descriptor
  {
    struct list_elem elem;      /* List element. */
    struct file *file;          /* File. */
    struct dir *dir;
    int handle;                 /* File handle. */
  };
 
/* Open system call. */
static int
sys_open (const char *ufile) 
{
  char *kfile = copy_in_string (ufile);
  struct file_descriptor *fd;
  int handle = -1;
  //printf("opening file:%s\n",ufile);
  fd = malloc (sizeof *fd);
  if (fd != NULL)
    {
      //lock_acquire (&fs_lock);
      fd->file = filesys_open (kfile);
      if (fd->file != NULL)
        {
          struct thread *cur = thread_current ();
          handle = fd->handle = cur->next_handle++;
          list_push_front (&cur->fds, &fd->elem);
        }
      else 
        free (fd);
     // lock_release (&fs_lock);
    }
  
  palloc_free_page (kfile);
  return handle;
}
 
/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with an
   open file. */
static struct file_descriptor *
lookup_fd (int handle) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
   
  for (e = list_begin (&cur->fds); e != list_end (&cur->fds);
       e = list_next (e))
    {
      struct file_descriptor *fd;
      fd = list_entry (e, struct file_descriptor, elem);
      if (fd->handle == handle)
        return fd;
    }
 
  thread_exit ();
}
//Additional functions by subhro

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with an
   open ordinary file. */
static struct file_descriptor *searchFileFd (int handle)
{
//printf("called from searchFileFd\n");
  struct file_descriptor *fd = lookup_fd (handle);
  if (fd->file == NULL)
  {
   //printf("exit at 8 \n");
    thread_exit ();
    }
  return fd;
}
 


/* Filesize system call. */
static int
sys_filesize (int handle) 
{
  struct file_descriptor *fd = lookup_fd (handle);
  int size;

  //lock_acquire (&fs_lock);
  size = file_length (fd->file);
  //lock_release (&fs_lock);
 
  return size;
}
 
/* Read system call. */
static int
sys_read (int handle, void *udst_, unsigned size) 
{
  uint8_t *udst = udst_;
  struct file_descriptor *fd;
  int bytes_read = 0;
 // printf("trying to read %u bytes into location:%08x\n",size,udst);
  /* Handle keyboard reads. */
  if (handle == STDIN_FILENO) 
    {
      for (bytes_read = 0; (size_t) bytes_read < size; bytes_read++)
        if (udst >= (uint8_t *) PHYS_BASE || !put_user (udst++, input_getc ()))
          thread_exit ();
      return bytes_read;
    }

  /* Handle all other reads. */
  fd = lookup_fd (handle);
  //lock_acquire (&fs_lock);
  while (size > 0) 
    {
      /* How much to read into this page? */
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t read_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Check that touching this page is okay. */
      if (!page_lock(udst,true))//!verify_user (udst)) // changed by subhro
        {
         // lock_release (&fs_lock);
    	  //printf("non verified user\n");
          thread_exit ();
        }

      /* Read from file into page. */
      retval = file_read (fd->file, udst, read_amt);
      if (retval < 0)
        {
          if (bytes_read == 0)
            bytes_read = -1; 
          break;
        }
      bytes_read += retval;

      /* If it was a short read we're done. */
      if (retval != (off_t) read_amt)
        break;

      /* Advance. */
      udst += retval;
      size -= retval;
    }
  //lock_release (&fs_lock);
  // printf("bytes read:%u\n",bytes_read);
  return bytes_read;
}
 
/* Write system call. */
static int
sys_write (int handle, void *usrc_, unsigned size) 
{
  uint8_t *usrc = usrc_;
  struct file_descriptor *fd = NULL;
  int bytes_written = 0;

  /* Lookup up file descriptor. */
  if (handle != STDOUT_FILENO)
    fd = searchFileFd (handle);

  //lock_acquire (&fs_lock);
  while (size > 0) 
    {
      /* How much bytes to write to this page? */
      size_t page_left = PGSIZE - pg_ofs (usrc);
      size_t write_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Check that we can touch this user page. */
      if (!page_lock (usrc,false))
        {
         // lock_release (&fs_lock);
          thread_exit ();
        }

      /* Do the write. */
      if (handle == STDOUT_FILENO)
        {
          putbuf ((char*)usrc, write_amt);
          retval = write_amt;
        }
      else
        retval = file_write (fd->file, usrc, write_amt);

      page_unlock(usrc);
      if (retval < 0) 
        {
          if (bytes_written == 0)
            bytes_written = -1;
          break;
        }
      bytes_written += retval;

      /* If it was a short write we're done. */
      if (retval != (off_t) write_amt)
        break;

      /* Advance. */
      usrc += retval;
      size -= retval;
    }
  //lock_release (&fs_lock);
 
  return bytes_written;
}
 
/* Seek system call. */
static int
sys_seek (int handle, unsigned position) 
{
  struct file_descriptor *fd = lookup_fd (handle);

  //lock_acquire (&fs_lock);
  if ((off_t) position >= 0)
    file_seek (fd->file, position);
  //lock_release (&fs_lock);
 
  return 0;
}
 
/* Tell system call. */
static int
sys_tell (int handle) 
{
  struct file_descriptor *fd = lookup_fd (handle);
  unsigned position;

  //lock_acquire (&fs_lock);
  position = file_tell (fd->file);
  //lock_release (&fs_lock);
 
  return position;
}
 
/* Close system call. */
static int
sys_close (int handle) 
{
  struct file_descriptor *fd = lookup_fd (handle);
  //lock_acquire (&fs_lock);
  file_close (fd->file);
  //lock_release (&fs_lock);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}
 
/* On thread exit, close all open files. */
void
syscall_exit (void) 
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;
   
  for (e = list_begin (&cur->fds); e != list_end (&cur->fds); e = next)
    {
      struct file_descriptor *fd;
      fd = list_entry (e, struct file_descriptor, elem);
      next = list_next (e);
      //lock_acquire (&fs_lock);
      file_close (fd->file);
      //lock_release (&fs_lock);
      free (fd);
    }
}


/*System call implementations for memory mapping*/
/* Binds a mapping id to a region of memory and a file. */
struct MappingTableEntry
  {
    struct list_elem listInput;
    int handle;                 // Mapping id
    uint8_t *baseAddr;              // Base address for memory mapping
    size_t pageCount;           //Count of pages mapped
    struct file *file;          // file pointer
  };

// Takes a handle as input and returns the file descriptor associated with it.
static struct MappingTableEntry *searchMap (int handle)
{
  struct thread *currThread = thread_current ();
  struct list_elem *it;

  for (it=list_begin(&(currThread->mapTable)); it!=list_end(&(currThread->mapTable));it=list_next(it))
    {
      struct MappingTableEntry *m = list_entry (it, struct MappingTableEntry, listInput);
      if (m->handle == handle)
        return m;
    }
  thread_exit ();
}

/* Remove mapping M from the virtual address space,
   writing back any pages that have changed. */
static void unmap (struct MappingTableEntry *m)
{
  list_remove(&(m->listInput));
  //printf("unmap********************************************\n");
  while((m->pageCount--)> 0)
    {
      page_deallocate(m->baseAddr);
      m->baseAddr += PGSIZE;
    }
  file_close(m->file);
  free(m);
}

// Mmap system call.
static int sys_mmap (int handle, void *addr)
{
//printf("called at 6\n");
  struct file_descriptor *fd = searchFileFd (handle);
  struct MappingTableEntry *m = malloc (sizeof *m);
  size_t offset;
  off_t length;

  if (m == NULL || addr == NULL || pg_ofs (addr) != 0)
    return -1;

  m->handle = thread_current ()->next_handle++;
  m->file = file_reopen (fd->file);
  if (m->file == NULL)
    {
      free (m);
      return -1;
    }
  m->baseAddr = addr;
  m->pageCount = 0;
  list_push_front (&thread_current()->mapTable, &(m->listInput));

  offset = 0;
  length = file_length (m->file);
  while (length > 0)
    {
      off_t readBytes=length >= PGSIZE ? PGSIZE : length;
      off_t zeroBytes= PGSIZE-readBytes;
      newSuppTableEntry(&(thread_current()->supplementaryPageTable),((uint8_t *) addr + offset),2,m->file,offset,readBytes,zeroBytes,true,true);
      offset += readBytes;
      length -= readBytes;
      m->pageCount++;
    }

  return m->handle;
}

/* Munmap system call. */
static int sys_munmap (int m)
{
  unmap(searchMap(m));
  return 0;
}

