/*	common.h

	Common definitions and types.
*/

#ifndef COMMON_H
#define COMMON_H

#ifndef NULL
#define NULL ((void*) 0)
#endif

//	Size of a sector on the floppy 
#define SECTOR_SIZE 512

//	Physical address of the text mode display 
#define SCREEN_ADDR ((short *) 0xb8000)

/*	If the expression p fails, print the source file and 
	line number along with the text s. Then hang the os. 

	Processes should use this macro.
*/
#define ASSERT2(p, s) \
	do { \
	if (!(p)) { \
		print_str(0, 0, "Assertion failure: "); \
		print_str(0, 19, s); \
		print_str(1, 0, "file: "); \
		print_str(1, 6, __FILE__); \
		print_str(2, 0, "line: "); \
		print_int(2, 6, __LINE__); \
		asm volatile ("cli"); \
		while (1); \
	} \
	} while(0)

/*	The #p tells the compiler to copy the expression p 
	into a text string and use this as an argument. 
	=> If the expression fails: print the expression as a message. 
*/
#define ASSERT(p) ASSERT2(p, #p)

//	Gives us the source file and line number where we decide to hang the os. 
#define HALT(s) ASSERT2(FALSE, s)

//	Typedefs of commonly used types
typedef enum {
	FALSE, TRUE
} bool_t;

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
#ifndef FAKE
typedef long long int int64_t;
#endif
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef unsigned char uchar_t;
typedef unsigned int uint_t;
typedef unsigned long ulong_t;

#define FREE_INODE 0
#define DIRECTORY 1
#define FILE_TYPE 2

#define FS_O_RDONLY 1
#define FS_O_WRONLY 2
#define FS_O_RDWR 3

typedef struct {
    // Fill in your stat here, this is just an example
    int inodeNo;        /* the file i-node number */
    short type;         /* the file i-node type, DIRECTORY, FILE_TYPE (there's another value FREE_INODE which never appears here */
    char links;         /* number of links to the i-node */
    int size;           /* file size in bytes */
    int numBlocks;      /* number of blocks used by the file */
} fileStat;

struct directory_t {
	int location;	//	Sector number
	int size;		//	Size in number of sectors
};

#endif
