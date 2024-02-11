// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  uint64 ref_count;
} cow_page_ref_info[PHYSTOP / PGSIZE];

// int cow_page_ref_count[PHYSTOP / PGSIZE];

uint64 get_cow_page_index(void* pa) {
  return ((uint64)pa - (uint64)end) / PGSIZE;
}

uint64 adjust_cow_page_ref_count(uint64 pa, int dx) {
  int idx = get_cow_page_index((void*)pa);
  
  // printf("adjust_cow_page_ref_count: pa %p dx %d idx %d\n", pa, dx, idx);

  uint64 cnt;
  acquire(&cow_page_ref_info[idx].lock);
  cow_page_ref_info[idx].ref_count += dx;
  cnt = cow_page_ref_info[idx].ref_count;
  release(&cow_page_ref_info[idx].lock);

  return cnt;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    initlock(&cow_page_ref_info[get_cow_page_index(p)].lock, "cow_page_ref_count lock");
    cow_page_ref_info[get_cow_page_index(p)].ref_count = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  uint64 cnt = adjust_cow_page_ref_count((uint64)pa, -1);
  if (cnt > 0) {
    return;
  }

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // if (cnt < 0) {
  //   panic("kfree: cnt < 0\n");
  // }

  acquire(&kmem.lock);
  if (cnt == 0) {
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // cow_page_ref_count[get_cow_page_index(r)] = 1;
  }
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    if (adjust_cow_page_ref_count((uint64)r, 1) != 1) {
      panic("kalloc: ref cnt is not 1\n");
    }
  }
  return (void*)r;
}
