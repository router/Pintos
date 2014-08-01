#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "lib/kernel/list.h"

/* Maximum size of process stack, in bytes. */
#define STACK_MAX (1024 * 1024)




/* Destroys the current process's page table. */
void page_exit (void)
{
	//return;

	struct list_elem *it=list_begin(&(thread_current()->supplementaryPageTable));
	int deleteCount=0;
	while(it!=NULL)
	//for(it=list_begin(&(thread_current()->supplementaryPageTable));it&&it!=list_end(&(thread_current()->supplementaryPageTable));it=list_next(it))
	{
		struct supplementalTablePageEntry *ent= list_entry(it,struct supplementalTablePageEntry,listInput);
		if(ent->type==0)
		{
			frame_free(ent->frame);
		}
		list_remove(it);
		it=list_begin(&(thread_current()->supplementaryPageTable));
		deleteCount++;
	}
	//printf("total %d entries deleted.\n",deleteCount);
}


/* This function takes a rounded down virtual address as input and returns the corresponding supp table entry as output .
 * If there is no such entry then it also works in expanding the stack and returns the corresponding entry.
 */
static struct supplementalTablePageEntry * page_for_addr (struct list *l,const void *address)
{
	struct list_elem *e=searchInSuppTable(l,address);
	if(!e)
	{
		//need to expand stack
		if(address>=PHYS_BASE)
			return NULL;
		//printf("expading stack.\n");
		struct page* p=page_allocate(address,false);

	     struct frame *fr=frame_alloc_and_lock(p);
	     if(!fr)
	     	return NULL;


	     newSuppTableEntry( l,p->addr,0,NULL,0,0,0,true,false);

	     /* Add the page to the process's address space. */

		   if(!(pagedir_get_page (thread_current()->pagedir, p->addr) == NULL&& pagedir_set_page (thread_current()->pagedir, p->addr, fr->base, true)))
	        {
			   //printf("unable to update PT\n");
			   frame_free(fr);
	          return NULL;
	        }
	           //printf("PT updated\n");
		   e=searchInSuppTable(l,p->addr);
		   struct supplementalTablePageEntry *supEntry= list_entry(e,struct supplementalTablePageEntry,listInput);
		   supEntry->frame=fr;
		   //memset(supEntry->frame->base,0,PGSIZE);
		   frame_unlock(supEntry->frame);
		   return supEntry;

	     //return NULL;
	}
	else
	{
		struct supplementalTablePageEntry *supEntry=list_entry(e,struct supplementalTablePageEntry,listInput);
		return supEntry;
	}

}

/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
static bool do_page_in (struct page *p)
{
}

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
bool page_in (void *fault_addr)
{
}

/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */
bool page_out (struct page *p)
{

	struct supplementalTablePageEntry *suppEntry=page_for_addr(&(p->owner->supplementaryPageTable),pg_round_down(p->addr));
	
	ASSERT (suppEntry != NULL);
	ASSERT (suppEntry->frame != NULL);
    ASSERT (lock_held_by_current_thread (&(suppEntry->frame->isLocked)));
	bool isDirty,success;
	pagedir_clear_page(p->owner->pagedir,p->addr);

	  /* Has the frame been modified? */
	isDirty = pagedir_is_dirty(p->owner->pagedir,p->addr);

	 
	//printf("supp entry:%08x---> frame:%08x in supplementary page table:%08x---->dirty page:%d\n",suppEntry,suppEntry->frame,&(p->owner->supplementaryPageTable),isDirty);


	  if(suppEntry->fc!=NULL)
	  {
		  if(isDirty)
		  {
		  	
			  if(!suppEntry->ismmap)
			  {
				 // printf("trying to swap:\n");
				  suppEntry->swp=swap_out(suppEntry->frame);
				  if(suppEntry->swp)
				  {
					  suppEntry->type=1; // changed from 0-->1
					  suppEntry->frame=NULL;
					  return true;
				  }
				  else
				  {
				 	  //printf("swap failed ... writing to disk instead.\n");
				 	  file_seek(suppEntry->fc->inputFile,0);
				 	  off_t bytes_wr=file_write_at(suppEntry->fc->inputFile,suppEntry->frame->base,PGSIZE,suppEntry->fc->offset);
				 	  //printf("nuber of bytes written :%d ---> to write:%d\n",bytes_wr,suppEntry->fc->readBytesCount);
				 	  if(bytes_wr==PGSIZE)
				 	  {
				 		  suppEntry->type=3;
				 		  free(suppEntry->frame->page);
				 		  suppEntry->frame=NULL;
				 		  return true;
				 	  }
				 	  else
				 	  {
				 		  suppEntry->type=3;
				 		  free(suppEntry->frame->page);
				 		  suppEntry->frame=NULL;
				 		 // printf("disk write failed");
				 		  return false;
					  }
				  }
			  }
			  else // if mmap case .. write only read bytes to the file .. no swapping out.
			  {
				  file_seek(suppEntry->fc->inputFile,0);
				  off_t bytes_wr=file_write_at(suppEntry->fc->inputFile,suppEntry->frame->base,suppEntry->fc->readBytesCount,suppEntry->fc->offset);

			  }
			  //}
			 // else
			  //{
			//	  file_write_at(suppEntry->fc->inputFile,suppEntry->frame->base,suppEntry->fc->readBytesCount,suppEntry->fc->offset);
			//	  suppEntry->type=2;
			//	  suppEntry->frame=NULL;
			//	  return true;
			  //}
		  }
		  else
		  {
			//printf("non dirty page\n");
		  	suppEntry->type=3;
		  	free(suppEntry->frame->page);
			suppEntry->frame=NULL;
			return true;
		  }
		  
	  

	}

}

/* Returns true if page P's data has been accessed recently,
   false otherwise.
   P must have a frame locked into memory.

   Called from frame itself so the assert conditions are not needed. */
bool page_accessed_recently (struct page *p)  //change
{
	  //bool was_accessed;

	  //ASSERT (p->frame != NULL);
	  //ASSERT (lock_held_by_current_thread (&p->frame->lock));

	  bool pageWasAccessed = pagedir_is_accessed(p->owner->pagedir,p->addr);
	  if (pageWasAccessed)
	    pagedir_set_accessed (p->owner->pagedir,p->addr,false);
	  return pageWasAccessed;
}

// This function loads the contents of the page with user address uvaddr into physical memory
bool loadPageInMemory(void* uVAddr)
{

	struct supplementalTablePageEntry *ent=page_for_addr(&(thread_current()->supplementaryPageTable),pg_round_down(uVAddr));//searchInSuppTable(&(currThread->supplementaryPageTable),p->addr);
	if(ent==NULL)
	{
		//printf("no entry found\n");
		return false;
	}
	if(ent->type==0) // it has already been handled.
	{
		return true;
	}

	if(ent->type==1) // from swap
	{
		//printf("readin from swap\n");
		ent->frame=frame_alloc_and_lock(ent->swp->upage);
		swap_in(ent->swp->upage,ent->frame->base);
		ent->type=0;
		ent->swp=NULL;
		ASSERT (lock_held_by_current_thread (&(ent->frame->isLocked)));
	      /* Add the page to the process's address space. */
		   if(!(pagedir_get_page (thread_current()->pagedir, ent->frame->page->addr) == NULL&& pagedir_set_page (thread_current()->pagedir, ent->frame->page->addr, ent->frame->base, ent->isWritable)))
	       {
	    	 	  frame_free(ent->frame);
	        	  return false;
	       }
	       frame_unlock(ent->frame);
		   return true;
	}
	else if(ent->type==2) // from disk
	{


		// FIRST TIME READ .. so create page
		struct page* p=page_allocate(uVAddr,false);
		ent->frame=frame_alloc_and_lock(p);
		//printf("iswriteable:%d for address:%08x----> supp entry:%08x into frame :%08x with page:%08x\n",ent->isWritable,ent->uVAddr,ent,ent->frame,ent->frame->page);


		/* Load this page. */

			  file_seek(ent->fc->inputFile,0);
		      if (file_read_at(ent->fc->inputFile, ent->frame->base, ent->fc->readBytesCount,ent->fc->offset) != ent->fc->readBytesCount)
		      {
		    	 // printf("count mismatch\n");
		    	  frame_free(ent->frame);
		          return false;
		       }
		      //printf("read  done\n");
		      memset (ent->frame->base + ent->fc->readBytesCount, 0, ent->fc->zeroBytesCount);


		      ent->type=0;
              ASSERT (lock_held_by_current_thread (&(ent->frame->isLocked)));
		      /* Add the page to the process's address space.  Copying the code from install_page*/

		      if(!(pagedir_get_page (thread_current()->pagedir, p->addr) == NULL&& pagedir_set_page (thread_current()->pagedir, p->addr, ent->frame->base, ent->isWritable)))
		      {
		    	 frame_free(ent->frame);
		         return false;
		      }
		      frame_unlock(ent->frame);
		      return true;
		}
	else if(ent->type==3) // reading a dirty page written to disk
	{
		//printf("reading from disk\n");
		// FIRST TIME READ .. so create page
		struct page* p=page_allocate(uVAddr,false);
		ent->frame=frame_alloc_and_lock(p);
		//printf("iswriteable:%d for address:%08x----> supp entry:%08x into frame :%08x with page:%08x\n",ent->isWritable,ent->uVAddr,ent,ent->frame,ent->frame->page);


		/* Load this page. */

			  file_seek(ent->fc->inputFile,0);
		      if (file_read_at(ent->fc->inputFile, ent->frame->base, PGSIZE,ent->fc->offset) != PGSIZE)
		      {
		    	 // printf("count mismatch\n");
		    	  frame_free(ent->frame);
		          return false;
		       }
		      //printf("read  done\n");
		      //memset (ent->frame->base + ent->fc->readBytesCount, 0, ent->fc->zeroBytesCount);


		      ent->type=0;
              ASSERT (lock_held_by_current_thread (&(ent->frame->isLocked)));
		      /* Add the page to the process's address space.  Copying the code from install_page*/

		      if(!(pagedir_get_page (thread_current()->pagedir, p->addr) == NULL&& pagedir_set_page (thread_current()->pagedir, p->addr, ent->frame->base, ent->isWritable)))
		      {
		    	 frame_free(ent->frame);
		         return false;
		      }
		      frame_unlock(ent->frame);
		      return true;
		}

}
/* Adds a mapping for user virtual address VADDR to the page hash
   table.  Fails if VADDR is already mapped or if memory
   allocation fails. */


//Creates an instance of virtual page with the given virtual address.
struct page * page_allocate (void *vaddr, bool read_only)
{
	struct page* newPage=(struct page*)malloc(sizeof(struct page));
	newPage->addr=pg_round_down(vaddr);
	newPage->isWritable=!read_only;
	newPage->owner=thread_current();
	return newPage;
}

/* Evicts the page containing address VADDR
   and removes it from the supplementary page table. */
void page_deallocate (void *vaddr)
{
	struct supplementalTablePageEntry * ent=page_for_addr(&(thread_current()->supplementaryPageTable),vaddr);
	ASSERT(ent!=NULL)
	if(ent->type==0) // page is in memory
	{
		struct frame *tempfr=ent->frame;
		if(ent->fc!=NULL)
		{
			lock_try_acquire(&(tempfr->isLocked));
			page_out(tempfr->page);
		}
		//free(ent->frame->page);
		frame_free(tempfr);
	}
	list_remove(&(ent->listInput));
	free(ent);
}


/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
bool
page_lock (const void *addr, bool will_write) //change
{
	  struct supplementalTablePageEntry *suppEntry = page_for_addr (&(thread_current()->supplementaryPageTable),addr);
	  if (suppEntry == NULL || (!suppEntry->isWritable && will_write))
	    return false;

	  //frame_lock (p);
	  if (suppEntry->frame == NULL)
	  {
		  // not in physical memory .. need to swap in
		  return loadPageInMemory(addr);

	  }
	    //return (do_page_in (p) && pagedir_set_page (thread_current ()->pagedir, p->addr,
	   //                              p->frame->base, !p->read_only));
	  //else
	  //  return true;
}

/* Unlocks a page locked with page_lock(). */
void
page_unlock (const void *addr) 
{
}


/*Supplemental page table methods */

void createSupplementalPageTable(struct list *table)
{

	list_init(table);

}

struct list_elem* searchInSuppTable(struct list *table,void *uVAddr)
{

	struct list_elem *it=list_begin(table);
	while(it&&it!=list_end(table))
	{
		struct supplementalTablePageEntry *entry= list_entry(it,struct supplementalTablePageEntry,listInput);
		if(entry->uVAddr==pg_round_down(uVAddr))
			return it;

		it=list_next(it);
	}
	//printf("not found\n");
	return NULL;

}
// since entries to this table always come from the exectuable file
void newSuppTableEntry(struct list *table,void *addr,int type,struct file* fileinput,off_t offset,size_t rbCount,size_t zbCount,bool writable,bool mmap)
{
	
	struct fileChunk *f;
	if(fileinput)
	{
		f=(struct fileChunk*)malloc(sizeof(struct fileChunk));
		f->inputFile=fileinput;
		f->offset=offset;
		f->readBytesCount=rbCount;
		f->zeroBytesCount=zbCount;
	}
	else
		f=NULL;
	struct supplementalTablePageEntry *newEntry=(struct supplementalTablePageEntry*)malloc(sizeof(struct supplementalTablePageEntry));
	newEntry->uVAddr=pg_round_down(addr);
	newEntry->type=type;// should always be 2
	newEntry->fc=f;
	newEntry->frame=NULL;
	newEntry->isWritable=writable;
	newEntry->ismmap=mmap;
	
	//printf("adding new supp table entry%08xof type :%d---->%08x with %d bytes and iswritable:%d\n",table,type,newEntry->uVAddr,rbCount,writable);
//	if(list_end(table)!=NULL)
//		printf("no end to the list\n");
	//struct list_elem *end=list_end(table);
//	if(!end->prev)
//		printf("no prev\n");
//	else if(!end->next)
//		printf("no next\n");

	//if(list_empty(table))
	{
	//	printf("adding first entry.\n");
	//	list_push_front(table,&(newEntry->listInput));

	}
	//else

	//printf("adding entry for va:%08x\n",newEntry->uVAddr);
	list_push_back(table,&(newEntry->listInput));
}
bool isStackAccess (void * esp, void * address)
{
	return (address < PHYS_BASE) && (address > STACK_BOTTOM) && (address + 32 >= esp);
}
