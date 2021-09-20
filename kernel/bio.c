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

#define MAP_SIZE 23
#define MAP_NBUF (NBUF / MAP_SIZE)


struct blockmap {
  struct spinlock locks[MAP_SIZE];
  struct buf *tab[MAP_SIZE];  
  struct buf cache[MAP_SIZE][NBUF];
} bmap; 

// hash function
uint hash(uint m, uint n) {
  return m % MAP_SIZE;
}

struct buf* bmap_get_buf(int idx) {
  for (int i = 0; i < NBUF; i++) {
    if (bmap.cache[idx][i].refcnt == 0)
      return &bmap.cache[idx][i];
  }
  panic("bmap_get_buf(): no buffers");
}

void bmap_destory_buf(struct buf *b) {
  b->blockno = 0;
  b->dev = 0;
  b->disk = 0;
  b->next = 0;
  // b->prev = 0;
  b->refcnt = 0;
  b->valid = 0;
}

/**
 * remove a block from the bmap
 * - should do kfree() with the buf/
 */
void bmap_remove(struct buf *b) {
  struct buf   *entry, *cur;
  uint                idx;
  
  idx = hash(b->blockno, b->dev);
  entry = bmap.tab[idx];
  if (!entry) {
    panic("bmap_remove(): nothing to remove");
  } else {
    cur = entry;
    if (cur == b) {
      if (cur->refcnt > 1) {
        return;
      }
      bmap.tab[idx] = cur->next;
      // free cur
      bmap_destory_buf(cur);

      return;
    }
    
    // is it in the map?
    // if in the map, just return
    while (cur->next) {
      if (cur->next == b) {
        struct buf *tmp = cur->next;
        if (tmp->refcnt > 1) 
          return;
        cur->next = cur->next->next;
        // free cur 
        bmap_destory_buf(tmp);

        // just return
        return;
      }
      cur = cur->next;
    }

    // nothing to remove
    panic("bmap_remove(): nothing to remove");
  }
}

/**
 * put a block to the bmap
 */
void bmap_put(struct buf *b) {
  struct buf          *entry, *cur;
  uint                idx;

  idx = hash(b->blockno, b->dev);
  entry = bmap.tab[idx];
  if (!entry) {
    
    // if the slot is empty
    // just alloc a free mem of
    // and put it to the slot
    cur = b;
    cur->next = 0;
    bmap.tab[idx] = cur;
  } else {
    cur = entry;
    
    // is it in the map?
    // if in the map, just return
    while (cur) {
      if (cur == b) {
        // just return
        return;
      }
      cur = cur->next;
    }

    // put it in the slot
    b->next = bmap.tab[idx];
    bmap.tab[idx] = b;
  }
}

/* 
 * if contains, return the buf's address
 * if not, return 0
 */
struct buf *bmap_get(uint dev, uint blockno) {
  struct buf   *entry, *cur;
  uint                idx;
  
  idx = hash(blockno, dev);

  entry = bmap.tab[idx];
  if (!entry) {
    return 0;
  }

  // find in the list
  cur = entry;
  while (cur) {
    if (cur->dev == dev && cur->blockno == blockno) {

      // now add 1
      // *can avoid the race condition*
      cur->refcnt += 1;
      return cur;
    }
    cur = cur->next;
  }
  return 0;
}

struct bcache {
  struct spinlock lock;
} bcache;

void
binit(void)
{
  // block map init 
  memset(&bmap, 0, sizeof(struct blockmap));

  // init the lock 
  for (int i = 0; i < MAP_SIZE; i++) {
    
    // char *name = "";
    // snprintf(name, 30, "bcache.bmap_slot[%d]", i);
    initlock(&bmap.locks[i], "bcache.bmap_slot");
    for (int j = 0; j < NBUF; j++) 
      initsleeplock(&bmap.cache[i][j].lock, "bcache.buffer");
  }

  // set the mem just 0
  memset(&bcache, 0, sizeof(struct bcache));
  initlock(&bcache.lock, "bcache");

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *newbuf;
  int idx = hash(blockno, dev);
  acquire(&bmap.locks[idx]);
  // Is the block already cached?
  // if in the chache, just return.
  newbuf = bmap_get(dev, blockno);
  if (newbuf) {
    release(&bmap.locks[idx]);
    acquiresleep(&newbuf->lock);
    return newbuf;
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.  
  // make a new buffer
  newbuf = bmap_get_buf(idx);
  newbuf->dev = dev;
  newbuf->blockno = blockno;
  newbuf->valid = 0;
  newbuf->refcnt = 1;

  // newbuf should be add to the bmap
  bmap_put(newbuf);

  release(&bmap.locks[idx]);
  acquiresleep(&newbuf->lock);
  return newbuf;
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
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int idx = hash(b->blockno, b->dev);
  acquire(&bmap.locks[idx]);
  releasesleep(&b->lock);
  // remove in the bmap
  bmap_remove(b);
  release(&bmap.locks[idx]);
}

void
bpin(struct buf *b) {
  int idx = hash(b->blockno, b->dev);
  acquire(&bmap.locks[idx]);
  b->refcnt++;
  release(&bmap.locks[idx]);
}

void
bunpin(struct buf *b) {
  int idx = hash(b->blockno, b->dev);
  acquire(&bmap.locks[idx]);
  b->refcnt--;
  release(&bmap.locks[idx]);
}


