/*
 * File system implementation has five layers:
 *   + Blocks: allocator for raw disk blocks - see bio.c
 *   + Log: crash recovery for multi-step updates - NOT USED in tfs.
 *   + Files: inode allocator, reading, writing, metadata - see fs.c and file.c.
 *   + Directories: inode with special contents (list of other inodes!)
 *   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
 *
 * This file contains the low-level file system manipulation routines.  
 * The (higher-level) system call implementations are in tfsfile.c.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "proc.h"
#include "fs.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);

/* 
 * The Xv6 file system requires locks on various data structures.
 * tinyfs does not require locks
 */

char buf[BSIZE];
struct superblock sb;
uint inodebitmap[BSIZE/4]; // block 2 is reserved for inode bitmap
                           // currently, inode.type == 0 is a free inode
uint databitmap[BSIZE/4];  // block 3 is data block bitmap
struct inode inodes[32];   // 32 inodes on blocks 4 through 7

// Read the super block, bitmaps, and inodes.
void readfsinfo() {
  int s = bread(1, buf);
  memcpy(&sb, buf, sizeof(sb));
  s = bread(2, buf);
  memcpy(inodebitmap, buf, BSIZE);
  s = bread(3, buf);
  memcpy(databitmap, buf, BSIZE);
  //int j = 0;
  for (int i = 0; i < 4; i++) {
    int s = bread(i+4, buf);
    if (s < 0)
      panic("bread fail");
    for (int j = 0; j < 8; j++)
        memcpy(&inodes[j+i*8], buf+(j*sizeof(struct inode)), sizeof(struct inode));
  }
  // copy link to ref - think about this
  for (int i = 0; i < sb.ninodes; i++)
    inodes[i].ref = inodes[i].nlink;
}

// print_inodes can be used for debugging
void print_inodes() {
  for (int k = 0; k < 32; k++)
      printf("inodes[%d].ref, type, size, num, ctime: %x, %d, %d, %d, %x\n", k, inodes[k].ref, inodes[k].type, inodes[k].size, inodes[k].inum, inodes[k].ctime);
}

// Write the super block, bitmaps, and inodes.
void writefsinfo() {
  memset(buf, 0, BSIZE);
  memcpy(buf, &sb, sizeof(sb));
  int s = bwrite(1, buf);
  memset(buf, 0, BSIZE);
  memcpy(buf, inodebitmap, BSIZE);
  s = bwrite(2, buf);
  memset(buf, 0, BSIZE);
  memcpy(buf, databitmap, BSIZE);
  s = bwrite(3, buf);
  //int j = 0;
  for (int i = 0; i < 4; i++) {
    memset(buf, 0, BSIZE);
    for (int j = 0; j < 8; j++) {
      memcpy(buf+(j*sizeof(struct inode)), &inodes[j+i*8], sizeof(struct inode));
    }
    int s = bwrite(i+4, buf);
    if (s < 0)
      panic("bwrite fail");
  }
}

/*
 * Blocks. 
 * Allocate a zeroed disk block.
 * See Xv6 balloc for how to use superblock to search for free blocks.
 * Our simple approach uses Block 3 for data block bitmap
 * First data block is block 8. Note start loop index.
 */
uint balloc() {
  uint m;
  for(int bi = 8; bi < 1024; bi++) { // 1024 blocks for file data
    m = 1 << (bi % 32);
    if((databitmap[bi/32] & m) == 0){  // Is block free?
      databitmap[bi/32] |= m;  // Mark block in use.
      memset(buf, 0, BSIZE);
      bwrite(bi, buf);
      return bi;
    }
  }
  panic("balloc: out of blocks");
  return -1;
}

// Free a disk block.
void bfree(uint bi) {
  uint m = 1 << (bi % 32);
  if((databitmap[bi/32] & m) == 0)
    panic("freeing free block");
  databitmap[bi/32] &= ~m;
}

/*
 * Inodes.
 *
 * An inode describes a single unnamed file.
 * The inode disk structure holds metadata: the file's type,
 * its size, the number of links referring to it, and the
 * list of blocks holding the file's content.
 *
 * The inodes are laid out sequentially on disk immediately after
 * the superblock. Each inode has a number, indicating its
 * position on the disk. We have one block for inodes
 *
 * tinyfs caches all inodes in memory. 
 * The cache is written back to disk on exit.
 * Since we have all inodes in memory, we do not have to free space 
 * and evict an inode to replace it with another.
 */
struct inode* iget(uint inum);
uint c_time = 0x30313241;
time_t seconds;
// Allocate a new inode with the given type
// A free inode has a type of zero.
// type is T_FILE, T_DIR, T_DEV
struct inode* ialloc(short type) {
  for(int inum = 1; inum < sb.ninodes; inum++) {
    if(inodes[inum].type == 0){  // a free inode
      memset(&inodes[inum], 0, sizeof(struct inode));
      inodes[inum].type = type;
      inodes[inum].inum = inum;
      time(&seconds);
      memcpy(&c_time, &seconds, 4);
      inodes[inum].ctime = c_time;
      return &inodes[inum];
    }
  }
  panic("ialloc: no inodes");
  return 0;
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
// ROOTINO is inodes[1]
// Do not use inodes[0]
// Need to have a ip->ref > 0 on the disk GUSTY-TODO
struct inode* iget(uint inum) {
  struct inode *ip, *empty;

  // Is the inode already cached?
  empty = 0;
  for(int ino = 1; ino < sb.ninodes; ino++) {
    ip = &inodes[ino];
    if(ip->ref > 0 && ip->inum == inum) {
      ip->ref++;
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->inum = inum;
  ip->ref = 1;
  //ip->flags = 0;
  return ip;
}


// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode* idup(struct inode *ip) {
  ip->ref++;
  return ip;
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
void iput(struct inode *ip) {
  if(ip->ref == 1 && /*(ip->flags & I_VALID) &&*/ ip->nlink == 0){
    // inode has no links: truncate and free inode.
    ip->type = 0;
    //ip->flags = 0;
    itrunc(ip);
  }
  ip->ref--;
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->blocks[].  
// We do not implement NINDIRECT. If so, the next NINDIRECT blocks are 
// listed in block ip->blocks[NDIRECT].
// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint bmap(struct inode *ip, uint bn) {
  uint addr;

  if(bn < NDIRECT){
    if((addr = ip->blocks[bn]) == 0)
      ip->blocks[bn] = addr = balloc();
    return addr;
  }

  panic("bmap: out of range");
  return -1;
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void itrunc(struct inode *ip) {
  for(int i = 0; i < NDIRECT; i++){
    if(ip->blocks[i]){
      bfree(ip->blocks[i]);
      ip->blocks[i] = 0;
    }
  }

  ip->size = 0;
  // iupdate(ip); // not needed - update inodes on disk on exit
}

// Copy stat information from inode.
void stati(struct inode *ip, struct tfs_stat *st)
{
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// Read data from inode.
int readi(struct inode *ip, char *dst, uint off, uint n) {
  uint tot, m;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    int s = bread(bmap(ip, off/BSIZE), buf);
    if (s < 0)
      panic("bread fail");
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, buf + off%BSIZE, m);
  }
  return n;
}

// Write data to inode.
int writei(struct inode *ip, char *src, uint off, uint n) {
  uint tot, m;
//cprintf("inside writei: type=%x major=%x, func addr: %x\n", ip->type, ip->major, devsw[ip->major].write);

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    int s = bread(bmap(ip, off/BSIZE), buf);
    if (s < 0)
      panic("bread fail");
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(buf + off%BSIZE, src, m);
    bwrite(bmap(ip, off/BSIZE), buf); // HERE
  }

  if(n > 0 && off > ip->size){
    ip->size = off;
    // iupdate(ip); // not needed - update inodes on disk on exit
  }
  return n;
}

// Directories
int namecmp(const char *s, const char *t) {
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode* dirlookup(struct inode *dp, char *name, uint *poff) {
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");

    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      //print_inodes(); // debug code
      //struct inode *inod = iget(inum);   // debug code
      //printf("inode->inum: %d\n", inod->inum); // debug code   
      return iget(inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int dirlink(struct inode *dp, char *name, uint inum) { int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");
  
  return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char* skipelem(char *path, char *name) {
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
static struct inode* namex(char *path, int nameiparent, char *name) {
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTINO);
  else
    //ip = iget(ROOTINO);
    ip = idup(curr_proc->cwd);

  while((path = skipelem(path, name)) != 0){
    if(ip->type != T_DIR){
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      return 0;
    }
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode* namei(char *path) {
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode* nameiparent(char *path, char *name) {
  return namex(path, 1, name);
}
