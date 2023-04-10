struct stat;

// system calls
int tfs_write(int, void*, int);
int tfs_read(int, void*, int);
int tfs_close(int);
int tfs_open(char*, int, int);
int tfs_mknod(char*, short, short);
int tfs_unlink(char*);
int tfs_fstat(int fd, struct stat*);
int tfs_link(char*, char*);
int tfs_mkdir(char*);
int tfs_chdir(char*);
int tfs_dup(int);

