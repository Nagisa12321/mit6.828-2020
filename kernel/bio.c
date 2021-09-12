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

#define MAP_SIZE 13

struct bmap_entry {
  struct bmap_entry *next;
  struct buf *b;
};

struct blockmap {
  struct spinlock locks[MAP_SIZE];
  struct bmap_entry *tab[MAP_SIZE];  
} bmap;

// hash function
uint hash(uint m, uint16 n) {
  return m % MAP_SIZE;
}

/**
 * remove a block from the bmap
 * - should do kfree() with the buf/bmap_entry
 */
void bmap_remove(struct buf *b) {
  struct bmap_entry   *entry, *cur;
  uint                idx;
  
  idx = hash(b->blockno, b->dev);
  acquire(&bmap.locks[idx]);
  entry = bmap.tab[idx];
  if (!entry) {
    panic("bmap_remove(): nothing to remove");
  } else {
    cur = entry;
    if (cur->b == b) {
      bmap.tab[idx] = cur->next;
      // free cur, and block
      kfree(cur->b);
      kfree(cur);

      release(&bmap.locks[idx]);
      return;
    }
    
    // is it in the map?
    // if in the map, just return
    while (cur->next) {
      if (cur->next->b == b) {
        struct bmap_entry *tmp = cur->next;
        cur->next = cur->next->next;
        // free cur, and block
        kfree(tmp->b);
        kfree(tmp);

        // just return
        release(&bmap.locks[idx]);
        return;
      }
      cur = cur->next;
    }

    // nothing to remove
    panic("bmap_remove(): nothing to remove");
  }
  release(&bmap.locks[idx]);
}

/**
 * put a block to the bmap
 */
void bmap_put(struct buf *b) {
  struct bmap_entry   *entry, *cur;
  uint                idx;

  idx = hash(b->blockno, b->dev);
  acquire(&bmap.locks[idx]);
  entry = bmap.tab[idx];
  if (!entry) {
    
    // if the slot is empty
    // just alloc a free mem of bmap_entry
    // and put it to the slot
    cur = (struct bmap_entry *) kalloc();
    cur->next = 0;
    cur->b = b;
    bmap.tab[idx] = cur;
  } else {
    cur = entry;
    
    // is it in the map?
    // if in the map, just return
    while (cur) {
      if (cur->b == b) {
        // just return
        release(&bmap.locks[idx]);
        return;
      }
      cur = cur->next;
    }

    // put it in the slot
    cur = (struct bmap_entry *) kalloc();
    cur->b = b;
    cur->next = bmap.tab[idx];
    bmap.tab[idx] = cur;
  }
  release(&bmap.locks[idx]);
}

/* 
 * if contains, return the buf's address
 * if not, return 0
 */
struct buf *bmap_get(uint dev, uint blockno) {
  struct bmap_entry   *entry, *cur;
  uint                idx;
  
  idx = hash(blockno, dev);

  acquire(&bmap.locks[idx]);
  entry = bmap.tab[idx];
  if (!entry) {
    release(&bmap.locks[idx]);
    return 0;
  }

  // find in the list
  cur = entry;
  while (cur) {
    if (cur->b->dev == dev && cur->b->blockno == blockno) {

      // now add 1
      // *can avoid the race condition*
      cur->b->refcnt += 1;
      release(&bmap.locks[idx]);

      // acquire the sleep lock here
      acquiresleep(&cur->b->lock);
      return cur->b;
    }
    cur = cur->next;
  }
  release(&bmap.locks[idx]);
  return 0;
}

struct bcache {
  struct spinlock lock;
} bcache;

void
binit(void)
{
  // block map init 
  // init the lock 
  for (int i = 0; i < MAP_SIZE; i++) {
    initlock(&bmap.locks[i], "bcache.bmap_slot");
  }

  // set the mem just 0
  memset(&bcache, 0, sizeof(struct bcache));
  initlock(&bcache.lock, "bcache");

}

struct buf* alloc_block() {
  struct buf *b = (struct buf*) kalloc();
  memset(b, 0, sizeof(struct buf));
  if (!b)
    panic("alloc_block(): kalloc");
  return b;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *newbuf;
  // Is the block already cached?
  // if in the chache, just return.
  newbuf = bmap_get(dev, blockno);
  if (newbuf) {
    return newbuf;
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // acquire(&bcache.lock);
  
  // make a new buffer
  newbuf = alloc_block();
  newbuf->dev = dev;
  newbuf->blockno = blockno;
  newbuf->valid = 0;
  newbuf->refcnt = 1;

  // newbuf should be add to the bmap
  bmap_put(newbuf);
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

  releasesleep(&b->lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // remove in the bmap
    bmap_remove(b);
  }
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


