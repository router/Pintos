#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/synch.h"

/* A physical frame. */
struct frame 
{
    void *base;                 /* Kernel virtual base address or rather starting physical address */
    struct page *page;          /* Mapped process page, if any. */
    struct list_elem listInput;
    struct lock isLocked;

};

void frame_init (void);
struct frame *frame_alloc_and_lock (struct page *);
void frame_lock (struct page *);
void frame_free (struct frame *);
void frame_unlock (struct frame *);
void frametable_init (void);
void evict();

#endif /* vm/frame.h */
