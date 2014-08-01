#include <stdio.h>
#include "page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "lib/kernel/list.h"
#include "vm/frame.h"

struct list gFrameTable;
struct lock gFrameTableMutex;


/* Initialize the frame manager. */

// Changed from provided source. We use the function below to initialize out frame table

void frametable_init (void)
{
	list_init(&gFrameTable);
	lock_init(&gFrameTableMutex);
}





// Uses LRU clock algorithm to evict a frame.
void evict()
{

	struct list_elem *minElem,*it;

	for(it=list_begin(&gFrameTable);it&&it!=list_end(&gFrameTable);it=list_next(it))
	//while(it&&it!=list_back(&gFrameTable))
	{
		struct frame *f=list_entry(it,struct frame,listInput);

	      if (!lock_try_acquire (&f->isLocked)) // frame is locked cant evict
	        continue;
	      //printf("lock acquired for %08x\n",f);
	      ASSERT (f != NULL);
 	      ASSERT (lock_held_by_current_thread (&f->isLocked));
	      if (page_accessed_recently (f->page)) //viable candidate for eviction next time
	      {
	          lock_release (&f->isLocked);
	         // printf("lock released for %08x\n",f);
	          continue;
	      }
	      
	      //printf("victim:%08xcontaining page:%08x with v.a.:%08x\n",f,f->page,f->page->addr);
	      //swap_in(f);
	      page_out(f->page);
	      frame_free(f);
	      break;

	}



}

/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, NULL on failure. */

struct frame* frame_alloc_and_lock (struct page *page)
{
	//printf("request for page:%08x",page);
	void* frameDetails=palloc_get_page(PAL_USER); //check
	if(frameDetails==NULL)
	{
		//
		//printf("no more memory left going for eviction\n");
		//PANIC("Kernel . enuf is enuf\n");
		//PANIC("no physical memory left");
		evict();
		//printf("eviction complete\n");
		return frame_alloc_and_lock(page);
		//panic kernel .. no more physical memory
	}
	else
	{
		//printf("new frame\n");
		struct frame *newFrame=(struct frame*)malloc(sizeof(struct frame));
		newFrame->base=frameDetails;
		newFrame->page=page;
		lock_init(&(newFrame->isLocked));
		lock_acquire(&(newFrame->isLocked));
		//printf("lock acquired for %08x\n",newFrame);



		//add to frame table
		lock_acquire(&gFrameTableMutex);
		list_push_back(&gFrameTable,&(newFrame->listInput));
		lock_release(&gFrameTableMutex);
		return newFrame;
	}
}


/* Locks P's frame into memory, if it has one.
   Upon return, p->frame will not change until P is unlocked. */

void frame_lock (struct page *p)
{
	// search for p's frame in list(iterate)
	struct list_elem *it=list_front(&gFrameTable);
	while(it&&it!=list_back(&gFrameTable))
	{
		struct frame *f=list_entry(it,struct frame,listInput);
		if(f->page==p) // found the correct frame
		{
			//f->isLocked=true;
			lock_acquire(&f->isLocked);
			break;
		}
		it=list_next(it);
	}

}


/* Releases frame F for use by another page.
   F must be locked for use by the current process.
   Any data in F is lost. */

void
frame_free (struct frame *f)
{
	ASSERT (lock_held_by_current_thread (&f->isLocked));
	lock_acquire(&gFrameTableMutex);
	//if(f->page->owner==thread_current()&&f->isLocked)
	{
		palloc_free_page(f->base);
		list_remove(&(f->listInput));
		lock_release (&f->isLocked);
		// printf("lock released for %08x\n",f);
		f->page=NULL; // make sure that the page (already inside swap must not be removed)
		free(f);

	}
	lock_release(&gFrameTableMutex);
}


/* Unlocks frame F, allowing it to be evicted.
   F must be locked for use by the current process. */

void
frame_unlock (struct frame *f) 
{
	ASSERT (lock_held_by_current_thread (&f->isLocked));
	lock_release(&f->isLocked);
}

