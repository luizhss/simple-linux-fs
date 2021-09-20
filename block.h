#ifndef BLOCK_INCLUDED
#define BLOCK_INCLUDED

#define BLOCK_SIZE_BITS 9
#define BLOCK_SIZE (1 << BLOCK_SIZE_BITS)
#define BLOCK_MASK (BLOCK_SIZE-1)

void bzero_block( char *block);
void block_init( void);
void block_read( int block, char *mem);
void block_write( int block, char *mem);

#endif
