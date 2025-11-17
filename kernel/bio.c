// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Hash table with NBUCKET buckets
  struct spinlock bucket_locks[NBUCKET];
  struct buf buckets[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char buf[16];

  initlock(&bcache.lock, "bcache");

  // Initialize bucket locks
  for (int i = 0; i < NBUCKET; i++) {
    snprintf(buf, sizeof(buf), "bcache.bucket");
    initlock(&bcache.bucket_locks[i], buf);
    bcache.buckets[i].next = 0;
  }

  // Initialize all buffers and add to bucket 0
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.buckets[0].next;
    bcache.buckets[0].next = b;
    b->refcnt = 0;
    b->timestamp = 0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket = blockno % NBUCKET;

  acquire(&bcache.bucket_locks[bucket]);

  // Is the block already cached in this bucket?
  for(b = bcache.buckets[bucket].next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->timestamp = ticks;
      release(&bcache.bucket_locks[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached in this bucket.
  // Find the LRU unused buffer in this bucket.
  struct buf *lru = 0;
  for(b = bcache.buckets[bucket].next; b != 0; b = b->next){
    if(b->refcnt == 0) {
      if (lru == 0 || b->timestamp < lru->timestamp) {
        lru = b;
      }
    }
  }

  if (lru) {
    lru->dev = dev;
    lru->blockno = blockno;
    lru->valid = 0;
    lru->refcnt = 1;
    lru->timestamp = ticks;
    release(&bcache.bucket_locks[bucket]);
    acquiresleep(&lru->lock);
    return lru;
  }

  // No unused buffer in this bucket, need to steal from other buckets
  release(&bcache.bucket_locks[bucket]);

  // Try to find an unused buffer in other buckets
  for (int i = 0; i < NBUCKET; i++) {
    if (i == bucket) continue;
    
    acquire(&bcache.bucket_locks[i]);
    lru = 0;
    for(b = bcache.buckets[i].next; b != 0; b = b->next){
      if(b->refcnt == 0) {
        if (lru == 0 || b->timestamp < lru->timestamp) {
          lru = b;
        }
      }
    }

    if (lru) {
      // Remove from current bucket
      struct buf **pp = &bcache.buckets[i].next;
      while (*pp != lru) {
        pp = &(*pp)->next;
      }
      *pp = lru->next;
      release(&bcache.bucket_locks[i]);

      // Add to target bucket
      lru->dev = dev;
      lru->blockno = blockno;
      lru->valid = 0;
      lru->refcnt = 1;
      lru->timestamp = ticks;
      
      acquire(&bcache.bucket_locks[bucket]);
      lru->next = bcache.buckets[bucket].next;
      bcache.buckets[bucket].next = lru;
      release(&bcache.bucket_locks[bucket]);
      
      acquiresleep(&lru->lock);
      return lru;
    }
    release(&bcache.bucket_locks[i]);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.bucket_locks[bucket]);
  b->refcnt--;
  release(&bcache.bucket_locks[bucket]);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.bucket_locks[bucket]);
  b->refcnt++;
  release(&bcache.bucket_locks[bucket]);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.bucket_locks[bucket]);
  b->refcnt--;
  release(&bcache.bucket_locks[bucket]);
}


