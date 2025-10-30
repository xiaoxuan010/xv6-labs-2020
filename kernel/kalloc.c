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

// Number of physical pages managed (from KERNBASE to PHYSTOP).
#define NPHYS_PAGES ((PHYSTOP - KERNBASE) / PGSIZE)
// Reference count per physical page. 0 means free (on freelist).
static int page_refcnt[NPHYS_PAGES];

static inline int
pa2idx(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // refcounts are zero-initialized (free pages). freereange will fill freelist.
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
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
  if(r)
    kmem.freelist = r->next;
  // If we got a page, mark its refcount as 1 (owned by caller).
  if (r)
  {
    int idx = pa2idx((uint64)r);
    page_refcnt[idx] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Increase the reference count for the page at pa.
void incref(uint64 pa)
{
  int idx = pa2idx(pa);
  acquire(&kmem.lock);
  page_refcnt[idx]++;
  release(&kmem.lock);
}

// Decrease the reference count for the page at pa and return the new count.
int decref(uint64 pa)
{
  int idx = pa2idx(pa);
  int rc;
  acquire(&kmem.lock);
  rc = --page_refcnt[idx];
  release(&kmem.lock);
  return rc;
}

// Get current reference count for page at pa.
int getref(uint64 pa)
{
  int idx = pa2idx(pa);
  int rc;
  acquire(&kmem.lock);
  rc = page_refcnt[idx];
  release(&kmem.lock);
  return rc;
}
