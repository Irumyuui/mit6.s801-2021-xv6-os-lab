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

#define BUF_HASH_TABLE_N 13
#define BUF_HASH(BLOCK_NO) (BLOCK_NO % BUF_HASH_TABLE_N)

uint get_current_tick() {
  uint result;
  acquire(&tickslock);
  result = ticks;
  release(&tickslock);
  return result;
}

struct buf_hash_table {
  struct spinlock lock;
  struct buf head;
};

struct {
  // struct spinlock lock;
  struct buf buf[NBUF];
  struct buf_hash_table table[BUF_HASH_TABLE_N];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  for (int i = 0; i < BUF_HASH_TABLE_N; i ++) {
    initlock(&bcache.table[i].lock, "bcache");
    bcache.table[i].head.prev = &bcache.table[i].head;
    bcache.table[i].head.next = &bcache.table[i].head;
  }

  for (int i = 0; i < NBUF; i ++) {
    struct buf *b = &bcache.buf[i];

    b->next = bcache.table[0].head.next;    
    b->prev = &bcache.table[0].head;
    b->last_access_tick = 0;
    initsleeplock(&b->lock, "buffer");
    bcache.table[0].head.next->prev = b;
    bcache.table[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int table_id = BUF_HASH(blockno);
  
  acquire(&bcache.table[table_id].lock);
  
  // Is the block already cached?
  for (struct buf *b = bcache.table[table_id].head.next; b != &bcache.table[table_id].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt ++;
      release(&bcache.table[table_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Is the table has a free cache?
  for (struct buf *b = bcache.table[table_id].head.next; b != &bcache.table[table_id].head; b = b->next) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      // b->last_access_tick = get_current_tick();
      release(&bcache.table[table_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not had free cache.
  // Try move free cache from other table.
  struct buf *free_cache = 0;
  int src_id = -1;
  for (int i = 0; i < BUF_HASH_TABLE_N; i ++) {
    if (i == table_id) 
      continue;
    
    acquire(&bcache.table[i].lock);
    for (struct buf *b = bcache.table[i].head.next; b != &bcache.table[i].head; b = b->next) {
      if (b->refcnt != 0) 
        continue;
      if (free_cache == 0 || free_cache->last_access_tick > b->last_access_tick) {
        free_cache = b;
        src_id = i;
      }
    }
    release(&bcache.table[i].lock);
  }

  if (free_cache) {
    // init
    free_cache->dev = dev;
    free_cache->blockno = blockno;
    free_cache->valid = 0;
    free_cache->refcnt = 1;
    // free_cache->last_access_tick = get_current_tick();

    // take
    acquire(&bcache.table[src_id].lock);
    free_cache->prev->next = free_cache->next;    
    free_cache->next->prev = free_cache->prev;
    release(&bcache.table[src_id].lock);

    // insert
    free_cache->next = bcache.table[table_id].head.next;
    free_cache->prev = &bcache.table[table_id].head;
    bcache.table[table_id].head.next->prev = free_cache;
    bcache.table[table_id].head.next = free_cache;

    release(&bcache.table[table_id].lock);
    acquiresleep(&free_cache->lock);
    return free_cache;
  }

  // Not found.
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

  b->last_access_tick = get_current_tick();

  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
  b->last_access_tick = get_current_tick();
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int table_id = BUF_HASH(b->blockno);
  acquire(&bcache.table[table_id].lock);
  b->refcnt --;
  release(&bcache.table[table_id].lock);
}

void
bpin(struct buf *b) {
  int table_id = BUF_HASH(b->blockno);

  acquire(&bcache.table[table_id].lock);
  b->refcnt ++;
  release(&bcache.table[table_id].lock);
}

void
bunpin(struct buf *b) {
  int table_id = BUF_HASH(b->blockno);

  acquire(&bcache.table[table_id].lock);
  b->refcnt --;
  release(&bcache.table[table_id].lock);
}


