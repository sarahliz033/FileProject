/*
 * On-disk file system format currently implemented for tinyfs
 * Block 0 is unused.
 * Block 1 is super block.
 * Block 2 inode bitmap - not used, inodes are free if type == 0
 * Block 3 data block bitmap
 * Blocks 4 through 7 hold inodes
 *  sizeof(inode) is 64 bytes
 *  8 inodes per block
 *  4 blocks of inodes yields 32 files on disk.
 * Blocks 8 to sb.nblocks are data blocks
 *
 * The next 4 lines are descriptions from original Xv6 fs.h
 * Blocks 2 through sb.ninodes/IPB hold inodes.
 * Then free bitmap blocks holding sb.size bits.
 * Then sb.nblocks data blocks.
 * Then sb.nlog log blocks.
 */

#define ROOTINO 1        // root i-number
#define BSIZE 512        // block size
#define NBLOCKS 1024     // number of blocks in file system
#define FSNAME "tinyfs"  // File system name

// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks - not used in tinyfs
  char name[12];     // name of file system
};

// tinyfs files are small. They can be 8 blocks (512 bytes per block)
#define NDIRECT 8
// tinyfs does not implement NINDIRECT, indirect blocks allow a file to expand
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

/*
 * inode structure - Xv6 has an ondisk inode and an in-memory inode. tinyfs has one inode structure
 * sizeof(inode) is 64, which allows for 8 inodes per 512 byte disk block
 * tinyfs allocates blocks 4-7 for inodes. tinyfs accomodates 32 files
 * If new members are added to struct inode and sizeof(struct inode) changes, you have to rethink disk layout
 *
 * Xv6 has a cache of in-memory inodes. inodes on the disk are read into the cache.
 * The in-memory inode structure has a ref member that counts the number of files referring to the in-memory inode.
 * tinyfs retains the ref member in order for the Xv6 code to work.
 * The function iget expectes ref to be non-zero to properly find an inode structure for an inode number
 * When tinyfs reads the intial inodes from disk, tinyfs sets ref to be equal to nlink.
 * This allows iget to work properly
 * A unit is 4 bytes. 
 * A struct inode has 7 members that are type uint - 28 bytes
 * A struct inode has a uint blocks[] that has 9 elements - 36 bytes
 * A struct inode is 64 bytes
 */
struct inode {
  uint type;     // File type - dir, file
  uint nlink;    // Number of links to inode in file system
  uint size;     // Size of file (bytes)
  uint ref;      // references to inode HERE
  uint inum;     // inode number
  //uint uid;      // user id
  //uint gid;      // group id
  uint ctime;    // creation time
  uint mtime;    // modified time
  uint blocks[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct inode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)

/*
 * A directory is a file containing a sequence of dirent structures.
 * A dirent structure maps a name to an inode number.
 * The function iget maps the inode number to its inode structure
 * tinyfs filenames are 14 characters, and an inum is a ushort.
 * A struct dirent is 16 bytes - a nice number
 */
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

