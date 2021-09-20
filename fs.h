#ifndef FS_INCLUDED
#define FS_INCLUDED

#include "common.h"
#include "block.h"

#define FS_SIZE 2048
#define MAX_FILE_NAME 28
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#define MAX_OPEN_FILES 256
#define MAGIC_NUMBER 0x42 // Life, The Universe and Everything

#define INODES_BLOCKS 256
#define INODES_PER_BLOCK 8 // Must be less than or equal to 8
#define DIRECT_POINTERS 10 
#define POINTERS_PER_DCB 16

// the following defines are just to make the code cleaner
#define INODES_NUMBER INODES_BLOCKS * INODES_PER_BLOCK
#define DATA_BLOCKS FS_SIZE - 2 - INODES_BLOCKS
#define IMAP_BYTES (INODES_NUMBER+7)/8
#define DMAP_BYTES (DATA_BLOCKS+7)/8
#define MAP_BLOCK 1 +INODES_BLOCKS

// superblock
typedef struct{

	// metadata of the disk
	uint32_t size_disk; // 4 bytes
	uint32_t block_size; // 4 bytes
	uint32_t num_inodes; // 4 bytes
	uint32_t num_data_blocks; // 4 bytes
	uint32_t num_blocks_inodes; // 4 bytes

	// pointer to beginning of important sectors
	uint32_t beg_inodes; // 4 bytes
	uint32_t beg_map; // 4 bytes
	uint32_t beg_data; // 4 bytes

	// Important 
	uint32_t pointers_per_block; // 4 bytes
	uint32_t pointers_per_dcb; // 4 bytes
	uint32_t inodes_per_block; // 4 bytes
	uint32_t direct_pointers; // 4 bytes
	
	uint32_t magic_number; // 4 byte
} superblock_t; // Total size = 52 bytes

// inode
typedef struct{
	int type; // 4 bytes
	int link_counter; // 4 bytes
	int size; // 4 bytes
	int direct[DIRECT_POINTERS]; // 4 * 10  = 40 bytes
	int indirect1; // 4 byte
	int indirect2; // 4 byte
	int indirect3; // 4 byte
} inode_t; // Total size = 64 bytes

// bits map
typedef struct{
	char imap[IMAP_BYTES]; // 20 bytes
	char dmap[DMAP_BYTES]; // 32 bytes
} bmap_t; // Total size = 52 bytes

// data block
// directory structure
typedef struct{
	uint8_t files_name[POINTERS_PER_DCB][MAX_FILE_NAME]; // 28 * 16 = 448 bytes
	int files_inum[POINTERS_PER_DCB]; // 4 * 16 = 64 bytes
} dir_t; // Total size = 512 bytes

typedef union{
	dir_t dir; // directory type (512 bytes)
	int pointers[BLOCK_SIZE/4]; // pointers block (512 bytes)
	int8_t data[BLOCK_SIZE]; // data (512 bytes)
} DataBlock; // Total size = 512 bytes

// block
typedef union{
	superblock_t sb; // superblock (52 bytes)
	inode_t inodes[INODES_PER_BLOCK]; // inodes (8 * 64 = 512 bytes)
	bmap_t map; // bits map (52 bytes)
	DataBlock data_block; // data block (512 bytes)
} Block; // Total size = 512 bytes

typedef struct{
	uint32_t magic_number;
	int blocks_allocated;
	int inodes_allocated;
	bmap_t map;
} fsCheck;

// Open-files table
typedef struct{
	int fd;
	char name[MAX_PATH_NAME];
	int inode;
	int flag;
	int rw_ptr;
} FileDescriptor; 

void fs_init(void);
int fs_mkfs(void);

int fs_open(char *fileName, int flags);
int fs_close(int fd);
int fs_read(int fd, char *buf, int count);
int fs_write(int fd, char *buf, int count);
int fs_lseek(int fd, int offset);
int fs_mkdir(char *fileName); 
int fs_rmdir(char *fileName); 
int fs_cd(char *dirName);
int fs_link(char *old_fileName, char *new_fileName);
int fs_unlink(char *fileName);
int fs_stat(char *fileName, fileStat *buf);
int fs_fsck(fsCheck *buf);

#endif
