#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>

#include "threads/synch.h"
#include "threads/vaddr.h"

/* The swap device. */
static struct block *swap_device;

/* Used swap sectors. */
static struct bitmap *swap_bitmap;

/* Protects swap_bitmap. */
static struct lock swap_lock;

/* Number of sectors per page. */
#define PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

struct list swapTable;

/* Sets up swap. */
void swap_init (void)
{

	swap_device=block_get_role(BLOCK_SWAP);//,swap_device);
	//setting up the swap to be of size 4MB
	//swap_device->size=PAGE_SECTORS*1024;
	lock_init(&swap_lock);
	if(swap_device!=NULL)
		swap_bitmap=bitmap_create(block_size (swap_device)/ PAGE_SECTORS); // each bit represents a swap page
	else 
	{
		//printf("no swap for us*********************************\n");
		swap_bitmap = bitmap_create (0);
	}
	list_init(&swapTable);

}


/* Swaps out page P, which must have a locked frame. */
// copies the contents of frame fr in the vacant swap slot
struct swaptable_entry* swap_out (struct frame  *fr)
{
	//return;
	// add to the swap table list
	lock_acquire(&swap_lock);
	block_sector_t firstFreeSector=getFreeSwapSlot();
	if(firstFreeSector==-1)
	{
	//printf("firstFreeSector%u\n",firstFreeSector);
		lock_release(&swap_lock);
		return NULL;
	}
	int i;
	// startSectorForWrite=firstFreePageSlot*PAGE_SECTORS;
	for(i=0;i<PAGE_SECTORS;i++) // sector wise writing
	{
		block_write(swap_device,firstFreeSector+i,fr->base+i*BLOCK_SECTOR_SIZE);
	}

	struct swaptable_entry *newPageSlot=(struct swaptable_entry*)malloc(sizeof(struct swaptable_entry));
	newPageSlot->baseLocation=firstFreeSector;
	newPageSlot->upage=fr->page;
	list_push_back(&swapTable,&newPageSlot->listInput);

	lock_release(&swap_lock);
	return newPageSlot;

}





/* Swaps in page P, which must have a locked frame
   (and be swapped out).*/
// swaps in contents of page p to the frame whose base address is destAddr
void swap_in (struct page *p, void *destAddr)
{
	//return;
	//iterate swap table list
	int i;
	lock_acquire(&swap_lock);
	struct list_elem *it=list_front(&swapTable);
	while(it&&it!=list_end(&swapTable))
	{
		struct swaptable_entry *st=list_entry(it,struct swaptable_entry,listInput);
		if(st->upage==p)
		{
			block_sector_t startSectorForRead=st->baseLocation;
			for(i=0;i<PAGE_SECTORS;i++)
			{
				block_read(swap_device,startSectorForRead+i,destAddr+i*BLOCK_SECTOR_SIZE);
			}
			//bitmap_set_multiple(swap_bitmap,startSectorForRead,PAGE_SECTORS,false);
			bitmap_reset(swap_bitmap,startSectorForRead/PAGE_SECTORS);

			list_remove(&(st->listInput));
			st->upage=NULL; // to make sure tht the page does not get released.
			free(st);
			lock_release(&swap_lock);
			//return true;
		}
		it=list_next(it);
	}
	lock_release(&swap_lock);
}

// Obtains a vacant swap slot
block_sector_t getFreeSwapSlot()
{
	size_t firstFreeSlot= bitmap_scan_and_flip(swap_bitmap,0,1,false);
	if(firstFreeSlot==BITMAP_ERROR)
		return -1;//PANIC("No swap sectors free");
	//printf("firstFreeSlot%u------>%d\n",firstFreeSlot,PAGE_SECTORS);
	//if(firstFreeSlot)
		return firstFreeSlot*PAGE_SECTORS;
	//else
	//	PANIC("No swap sectors free");
}
