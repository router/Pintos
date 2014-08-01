#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "frame.h"
#include "swap.h"

#define STACK_BOTTOM ((void *) (PHYS_BASE - (8 * 1024 * 1024)))
/* Virtual page. */
struct page 
  {
     void *addr;                 /* User virtual address. */
     struct thread* owner;		// owning thread
     bool isWritable;
  };

struct fileChunk
{
	struct file* inputFile;
	off_t offset;
	size_t readBytesCount;
	size_t zeroBytesCount;
};
struct supplementalTablePageEntry
{
	void *uVAddr;						  // The user virtual address
	int type;							  // Location of the contents of that page
	bool isWritable;						 // Determines if the page is writable
	struct frame *frame;					 // Frame containing the page contents , NULL otherwise
	struct swaptable_entry *swp;			 // Swap slot containing the page contents ,NULL otherwise.
	struct fileChunk *fc;					 // Details of the file if contents are on disk
	struct list_elem listInput;
	bool ismmap;				 // Determines if the virtual address is a memory mapping to  a file.

};



void page_exit (void);

struct page *page_allocate (void *, bool read_only);
void page_deallocate (void *vaddr);

bool page_in (void *fault_addr);
bool page_out (struct page *);
bool page_accessed_recently (struct page *);

bool page_lock (const void *, bool will_write);
void page_unlock (const void *);



// supplemental table methods
void createSupplementalPageTable(struct list *table);
struct list_elem* searchInSuppTable(struct list *table,void *uVAddr);
void newSuppTableEntry(struct list *table,void *addr,int type,struct file* fileinput,off_t offset,size_t rbCount,size_t zbCount,bool writable,bool mmap);
bool is_stack_access(void *, void *);

#endif /* vm/page.h */
