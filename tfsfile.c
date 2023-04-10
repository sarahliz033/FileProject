/*
 * tfsfile.c defines the user API for tinyfs system calls.
 * Since processes manipulate files, tfsfile.c uses curr_proc for file descriptors
 * For the most part, tfsfile.c functions call functions in file.c
 */

#include <string.h>
#include <stdio.h>
#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int fd_to_file(int fd, struct file **pf) {
  struct file *f;

  if(fd < 0 || fd >= NOFILE || (f=curr_proc->ofiles[fd]) == 0)
    return -1;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int fdalloc(struct file *f) {
  int fd;

  for(fd = 0; fd < NOFILE; fd++){
    if(curr_proc->ofiles[fd] == 0){
      curr_proc->ofiles[fd] = f;
      return fd;
    }
  }
  return -1;
}

int tfs_dup(struct file *f) {
  int fd;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int tfs_read(int fd, void *p, int n) {
  struct file *f;
  if (fd_to_file(fd, &f) < 0)
    return -1;
  return fileread(f, p, n);
}

int tfs_write(int fd, void *p, int n) {
  struct file *f;
  if (fd_to_file(fd, &f) < 0)
    return -1;
  return filewrite(f, p, n);
}

int tfs_close(int fd) {
  struct file *f;
  if (fd_to_file(fd, &f) < 0)
    return -1;
  curr_proc->ofiles[fd] = 0;
  fileclose(f);
  return 0;
}

int tfs_fstat(int fd, struct tfs_stat *st) {
  struct file *f;
  if (fd_to_file(fd, &f) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int tfs_link(char *old, char *new) {
  char name[DIRSIZ];
  struct inode *dp, *ip;

  if((ip = namei(old)) == 0)
    return -1;

  if(ip->type == T_DIR)
    return -1;

  ip->nlink++;
  //iupdate(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  if(dirlink(dp, name, ip->inum) < 0)
    goto bad;
  iput(ip);

  return 0;

bad:
  ip->nlink--;
  //iupdate(ip);
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dp) {
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int tfs_unlink(char *path) {
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ];
  uint off;

  if((dp = nameiparent(path, name)) == 0)
    return -1;

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    return -1;

  if((ip = dirlookup(dp, name, &off)) == 0)
    return -1;

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip))
    return -1;

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    //iupdate(dp);
  }

  ip->nlink--;
  //iupdate(ip);
  return 0;
}

static struct inode* create(char *path, short type) {
  uint off;
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  if((ip = dirlookup(dp, name, &off)) != 0){
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    return 0;
  }

  if((ip = ialloc(type)) == 0)
    panic("create: ialloc");

  ip->nlink = 1;
  //iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    // iupdate(dp); // not nteeded - update inodes on disk on exit
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  return ip;
}

int tfs_open(char *path, int flags, int mode) {
  int fd;
  struct file *f;
  struct inode *ip;

  if(flags & TO_CREATE){
    ip = create(path, T_FILE);
    if(ip == 0)
      return -1;
  } else {
    if((ip = namei(path)) == 0)
      return -1;
    if(ip->type == T_DIR && flags != TO_RDONLY){
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    return -1;
  }

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(flags & TO_WRONLY);
  f->writable = (flags & TO_WRONLY) || (flags & TO_RDWR);
  return fd;
}

int tfs_mkdir(char *path) {
  struct inode *ip;
  if ((ip = create(path, T_DIR)) == 0)
    return -1;
  return 0;
}

int tfs_chdir(char *path) {
  struct inode *ip;

  if((ip = namei(path)) == 0)
    return -1;
  if(ip->type != T_DIR){
    return -1;
  }
  iput(curr_proc->cwd);
  curr_proc->cwd = ip;
  return 0;
}
