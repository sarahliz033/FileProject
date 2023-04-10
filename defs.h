struct context;
struct file;
struct inode;
struct proc;
struct tfs_stat;
struct superblock;

void OkLoop(void);
void NotOkLoop(void);

// bio.c
void            binit(void);
int             bread(uint, char*);
int             bwrite(uint, char*);

// fs.c
void		readfsinfo();
void		writefsinfo();
void            readsb(struct superblock *sb);
int             dirlink(struct inode*, char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);
struct inode*   ialloc(short);
struct inode*   idup(struct inode*);
void            iinit(void);
void            iput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, char*, uint, uint);
void            stati(struct inode*, struct tfs_stat*);
int             writei(struct inode*, char*, uint, uint);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, char*, int n);
int             filestat(struct file*, struct tfs_stat*);
int             filewrite(struct file*, char*, int n);

// console.c
void            panic(char*);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

