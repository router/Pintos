#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include "lib/kernel/list.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"

struct swaptable_entry
{
	struct page* upage;
	block_sector_t baseLocation;
	struct list_elem listInput;
};
void swap_init (void);
struct swaptable_entry* swap_out (struct frame *);
void swap_in (struct page *,void *);
block_sector_t getFreeSwapSlot();

#endif /* vm/swap.h */
