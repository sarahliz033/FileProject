//
// File descriptors
// Removed locks - ftable lock and locking inodes
// Removed log.c stuff begin_trans, etc.
// See Xv6 source for missing code
//

#include <stdio.h>
#include <string.h>
#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "file.h"

struct {
  struct file file[NFILE];
} ftable;

void fileinit(void) {
  memset(&ftable, 0, sizeof(ftable));
}

// Allocate a file structure.
struct file* filealloc(void) {
  struct file *f;

  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      return f;
    }
  }
  return 0;
}

// Increment ref count for file f.
struct file* filedup(struct file *f) {
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void fileclose(struct file *f) {
  struct file ff;

  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  
  if(ff.type == FD_INODE){
    iput(ff.ip);
  }
}

// Get metadata about file f.
int filestat(struct file *f, struct tfs_stat *st) {
  if(f->type == FD_INODE){
    stati(f->ip, st);
    return 0;
  }
  return -1;
}

// Read from file f.
int fileread(struct file *f, char *addr, int n) {
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_INODE){
//cprintf("inside fileread\n");
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
//cprintf("inside fileread: after readi rv=%x\n", r);
    return r;
  }
  panic("fileread");
  return -1;
}

//PAGEBREAK!
// Write to file f.
int filewrite(struct file *f, char *addr, int n) {
  int r;

  if(f->writable == 0)
    return -1;
//cprintf("inside filewrite\n");
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((LOGSIZE-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
  return -1;
}

