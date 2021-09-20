#include "fs.h"
#include "block.h"
#include "util.h"
#include "fsUtil.h"
#include "common.h"

#include <assert.h>
#include <stdio.h>

// Declaring the global variables
extern superblock_t super;
extern bmap_t map;
extern dir_t current_dir;

extern FileDescriptor table[MAX_OPEN_FILES];

/////////////////////////////////////////////////////////////////////////////////////

/* 
    Function to get and set block index number from inode.
*/

// Returns the pointer to a block given its relative index on
// indirect blocks pointer
int get_indirect_iblock(uint32_t iblock, int height, int index){

    DataBlock block;
    block_read(super.beg_data + iblock, (char *) &block);
    if(height == 1){
        return block.pointers[index];
    }

    int blocks_per_pointer = 1, i;
    for(i = 1; i < height; i++){
        blocks_per_pointer *= super.pointers_per_block;
    }

    for(i = 0; i < super.pointers_per_block; i++){
        if(block.pointers[i] == -1)
            return -1;
        if(index < blocks_per_pointer){
            return get_indirect_iblock(block.pointers[i], 
                                       height-1, index);
        }
        index -= blocks_per_pointer;
    }
    
    return -1;
}

// This function returns the ith block of an inode
int get_iblock(inode_t file, int index){
    if(index >= max_blocks_of_file()){
        return -1;
    }

    if(index < super.direct_pointers){
        return file.direct[index];
    }
    index -= super.direct_pointers;

    // trying the first set of indirect blocks
    if(index < super.pointers_per_block){
        if(file.indirect1 == -1) return -1;
        return get_indirect_iblock(file.indirect1, 1, index);
    }
    index -= super.pointers_per_block;

    // if not found, try the double indirect blocks
    if(index < super.pointers_per_block*super.pointers_per_block){
        if(file.indirect2 == -1) return -1;
        return get_indirect_iblock(file.indirect2, 2, index);
    }
    index -= super.pointers_per_block*super.pointers_per_block;

    if(file.indirect3 == -1) return -1;
    return get_indirect_iblock(file.indirect3, 3, index);
}

// Set the pointer to a block given its relative index on
// indirect blocks pointer, returns 1 if successfuly
int set_indirect_iblock(uint32_t iblock, int height, int index, int new_inum){
    
    DataBlock block;
    block_read(super.beg_data + iblock, (char *) &block);
    if(height == 1){
        if(index >= super.pointers_per_block) return -1;
        block.pointers[index] = new_inum;
        block_write(super.beg_data + iblock, (char *) &block);
        return 0;
    }

    int blocks_per_pointer = 1, i, ret;
    for(i = 1; i < height; i++){
        blocks_per_pointer *= super.pointers_per_block;
    }

    for(i = 0; i < super.pointers_per_block; i++){
        if(index < blocks_per_pointer){
            if(block.pointers[i] == -1){
                // allocate new block of pointers
                int new_iblock = get_single_available_iblock();
                if(new_iblock < 0) return -1;

                DataBlock aux_block;
                for(int i = 0; i < super.pointers_per_block; i++)
                    aux_block.pointers[i] = -1;
                block_write(super.beg_data + new_iblock, (char *) &aux_block);

                block.pointers[i] = new_iblock;
                block_write(super.beg_data + iblock, (char *) &block);
            }

            ret = set_indirect_iblock(block.pointers[i], height-1, index, new_inum);
            if(is_pointers_block_empty(block.pointers[i])){
                free_iblock(block.pointers[i]);
                block.pointers[i] = -1;
            }
            if(ret == 0) return 0;
        }
        index -= blocks_per_pointer;
    }

    return -1;
}
    
int set_iblock(inode_t *file, int index, int new_inum){
    if(index >= max_blocks_of_file()){
        return -1;
    }

    int ppb = super.pointers_per_block, ret;

    // check if the file is in direct pointers
    if(index < super.direct_pointers){
        file->direct[index] = new_inum;
        return 0;
    }
    index -= super.direct_pointers;

    // check if the file is in simple indirect pointers
    if(index < ppb){
        if(file->indirect1 == -1){
            // allocate new block of pointers
            int iblock = get_single_available_iblock();
            if(iblock < 0) return -1;

            DataBlock block;
            for(int i = 0; i < ppb; i++)
                block.pointers[i] = -1;
            block_write(super.beg_data + iblock, (char *) &block);

            file->indirect1 = iblock;
        }
        ret = set_indirect_iblock(file->indirect1, 1, index, new_inum);
        if(is_pointers_block_empty(file->indirect1)){
            free_iblock(file->indirect1);
            file->indirect1 = -1;
        }
        return ret;
    }
    index -= ppb;

    // check if the file is in double indirect pointers
    if(index < ppb*ppb){
        if(file->indirect2 == -1){
            // allocate new block of pointers
            int iblock = get_single_available_iblock();
            if(iblock < 0) return -1;

            DataBlock block;
            for(int i = 0; i < ppb; i++)
                block.pointers[i] = -1;
            block_write(super.beg_data + iblock, (char *) &block);

            file->indirect2 = iblock;
        }
        ret = set_indirect_iblock(file->indirect2, 2, index, new_inum);
        if(is_pointers_block_empty(file->indirect2)){
            free_iblock(file->indirect2);
            file->indirect2 = -1;
        }
        return ret;
    }
    index -= ppb*ppb;

    if(file->indirect3 == -1){
        // allocate new block of pointers
        int iblock = get_single_available_iblock();
        if(iblock < 0) return -1;

        DataBlock block;
        for(int i = 0; i < ppb; i++)
            block.pointers[i] = -1;
        block_write(super.beg_data + iblock, (char *) &block);

        file->indirect3 = iblock;
    }
    ret = set_indirect_iblock(file->indirect3, 3, index, new_inum);
    if(is_pointers_block_empty(file->indirect3)){
        free_iblock(file->indirect3);
        file->indirect3 = -1;
    }
    return ret;
}

/////////////////////////////////////////////////////////////////////////////////////
/*
    Functions to manipulate the map of bits.
*/

// Return the first available inode and set it as used
int32_t get_single_available_inode(){
    for(int i = 0; i < super.num_inodes; i++){
        if((map.imap[i/8]&(1<<(7-i%8))) == 0){
            map.imap[i/8] |= (1<<(7-i%8));
            save_map();
            return i;
        }
    }
    return -1;
}

// Return the first available block and set it as used
int32_t get_single_available_iblock(){
    for(int i = 0; i < super.num_data_blocks; i++){
        if((map.dmap[i/8]&(1<<(7-i%8))) == 0){
            map.dmap[i/8] |= (1<<(7-i%8));
            save_map();
            return i;
        }
    }
    return -1;
}

// Mark given iblock as free
void free_iblock(int32_t inum){
    map.dmap[inum/8] &= ~(1<<(7-inum%8));
    save_map();
}

// Mark given inode as free
void free_inode(int32_t inum){
    map.imap[inum/8] &= ~(1<<(7-inum%8));
    save_map();
}

// Save map of bits to disk
void save_map(){
    Block aux;
    aux.map = map;
    block_write(MAP_BLOCK, (char *) &aux);
}

/////////////////////////////////////////////////////////////////////////////////////

/*
    Operations over Directory Control Block (DCB)
*/


// Returns TRUE if directory block is empty, else returns FALSE
bool_t is_dir_block_empty(int iblock){
    DataBlock block;
    block_read(super.beg_data + iblock, (char *) &block);

    return (block.dir.files_inum[0] == -1);
} 


// Remove a file from a given directory inode
// You must pass the relative index of the block in the inode pointers
void remove_file_from_dir(inode_t * dir_inode, int ptr_to_remove){

    int block_index, current_iblock, next_iblock;
    int i;

    // get index of block that we must start delete process
    block_index = ptr_to_remove/super.pointers_per_dcb; 
    ptr_to_remove %= super.pointers_per_dcb;

    // get the beginning block inum
    next_iblock = get_iblock(*dir_inode, block_index);

    do{
        current_iblock = next_iblock; // set current iblock to a new location

        DataBlock block, next_block;

        // load block to memory
        block_read(super.beg_data + current_iblock, (char *) &block);

        // removes value in ptr_to_remove index
        for(i = ptr_to_remove; i < super.pointers_per_dcb-1; i++){
            bcopy(block.dir.files_name[i+1], block.dir.files_name[i], MAX_FILE_NAME);
            block.dir.files_inum[i] = block.dir.files_inum[i+1];
        }

        // get next block to set the last pointer, if there is a next block
        next_iblock = get_iblock(*dir_inode, ++block_index);
        if(next_iblock != -1){

            // if it exists, we set the last pointer equals the first of the
            // block loaded to memory
            block_read(super.beg_data + next_iblock, (char *) &next_block);

            bcopy(next_block.dir.files_name[0], block.dir.files_name[i], MAX_FILE_NAME);
            block.dir.files_inum[i] = next_block.dir.files_inum[0];

        }else{

            // if it does not exist, we set a null value
            bzero((char*)block.dir.files_name[i], MAX_FILE_NAME);
            block.dir.files_inum[i] = -1;
        }

        // write current block to disk
        block_write(super.beg_data + current_iblock, (char *) &block);
        ptr_to_remove = 0;

    }while(next_iblock != -1);

    // delete empty block
    if(is_dir_block_empty(current_iblock) == TRUE){
        set_iblock(dir_inode, --block_index, -1); // free last block from dir
        free_iblock(current_iblock); // free last block from bits map
    }
}

/*
    Find a file in a inode of type directory given its file name

    Parameters: 
        file (inode_t)   - Inode of type directory which we want to find a file
        fileName (char*) - Name of the file we want to find
        relIndex (int*)  - Set this variable as the relative index of the file entry
                           on the inode, if found. You may pass it as NULL. 
    Returns:
        (int) - If the file is found, returns its inode pointer
                else, returns -1 
*/
int find_file_in_dir(inode_t file, char * fileName, int* relIndex){
    DataBlock block;
    int iblock, num_blocks;

    num_blocks = (file.size + super.pointers_per_dcb - 1) / super.pointers_per_dcb;
    for(int i = 0; i < num_blocks; i++){
        iblock = get_iblock(file, i);
        block_read(super.beg_data + iblock, (char *) &block);

        for(int j = 0; j < super.pointers_per_dcb; j++){
            if(same_string((char*) fileName, (char*) block.dir.files_name[j]) ){
                if(relIndex != NULL){
                    *relIndex = i * super.pointers_per_dcb + j;
                }
                return block.dir.files_inum[j];
            }
        }
    }

    return -1;
}


// Insert a new entry in a directory
int insert_file_in_dir(inode_t * dir, char * fileName, int32_t inum){
    
    int num_blocks = (dir->size + super.pointers_per_dcb - 1) / super.pointers_per_dcb;
    // read last block
    int32_t last_block_inum = get_iblock(*dir, num_blocks-1);
    if(last_block_inum < 0){
        return -1;
    }

    DataBlock block;
    block_read(super.beg_data + last_block_inum, (char *) &block);

    // try to insert 
    for(int i = 0; i < super.pointers_per_dcb; i++){
        if(block.dir.files_inum[i] == -1){
            bcopy((uint8_t *)fileName, (uint8_t *)block.dir.files_name[i], strlen(fileName)+1);
            block.dir.files_inum[i] = inum;

            block_write(super.beg_data + last_block_inum, (char *) &block);
            return 0;
        }
    }


    // if function gets here, it means we must add another block
    
    // allocate a new block if possible
    int new_iblock = get_single_available_iblock();
    if(new_iblock < 0){
        return -1;
    }


    // Nullify entries on the new allocated data block
    DataBlock new_block;
    for(int i = 0; i < super.pointers_per_dcb; i++){
        bzero((char *) new_block.dir.files_name[i], MAX_FILE_NAME);
        new_block.dir.files_inum[i] = -1;
    }

    // Insert entry
    bcopy((uint8_t *)  fileName, (uint8_t *) new_block.dir.files_name[0], strlen(fileName)+1);
    new_block.dir.files_inum[0] = inum;
    
    num_blocks++;
    if(set_iblock(dir, num_blocks-1, new_iblock) < 0){
        free_iblock(new_iblock);
        return -1;
    }

    // write blocks to disk
    block_write(super.beg_data + new_iblock, (char *) &new_block);
    
    // save map of bits
    save_map();

    save_inode(current_dir.files_inum[0], *dir);

    return 0;
}


// Returns an empty directory
dir_t create_directory(int inum){
    dir_t new_dir;

    // nullify all entries from dcb
    for(int i = 0; i < super.pointers_per_dcb; i++){
        bzero((char *) new_dir.files_name[i], MAX_FILE_NAME);
        new_dir.files_inum[i] = -1;
    }

    // set '.' and '..' entries to dcb
    bcopy((uint8_t *)  ".",(uint8_t *) new_dir.files_name[0], 2);
    bcopy((uint8_t *) "..",(uint8_t *) new_dir.files_name[1], 3);
    new_dir.files_inum[0] = inum;
    new_dir.files_inum[1] = current_dir.files_inum[0];

    return new_dir;
}


// Returns TRUE, if the directory is empty else returns FALSE
bool_t is_directory_empty(inode_t dir){
    if(dir.size > 2){
        return FALSE;
    }

    DataBlock block;
    block_read(super.beg_data + dir.direct[0], (char *) &block);

    return (block.dir.files_inum[2] == -1);
}


/////////////////////////////////////////////////////////////////////////////////////

/*
    Operations over inodes
*/

// Save to disk given inode in the given index
void save_inode(int index, inode_t inode){
    int iblock = index / super.inodes_per_block;

    Block block;
    block_read(super.beg_inodes + iblock, (char *) &block);

    block.inodes[index%super.inodes_per_block] = inode;

    block_write(super.beg_inodes + iblock, (char *) &block);
}

// Retuns an inode given its index on disk
inode_t get_inode_per_inum(int index){
    int iblock = index / super.inodes_per_block;

    Block block;
    block_read(super.beg_inodes + iblock, (char *) &block);

    return block.inodes[index%super.inodes_per_block];
}


/////////////////////////////////////////////////////////////////////////////////////

/*
    Operation on Table of Open Files
*/

int get_single_available_fd(){
    for(int fd = 0; fd < MAX_OPEN_FILES; fd++){
        if(table[fd].fd == -1)
            return fd;
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////

/*
    Operations on Files
*/

void free_all_data_blocks_indirect(int iblock, int height){
    if(height >= 1){
        DataBlock block;
        block_read(super.beg_data + iblock, (char *) &block);
        for(int i = 0; i < super.pointers_per_block; i++){
            if(block.pointers[i] == -1) break;
            if(height > 1)
                free_all_data_blocks_indirect(block.pointers[i], height-1);
            free_iblock(block.pointers[i]);
        }
    }
}

// Free all data blocks of an inode by setting all its allocated blocks as free
void free_all_data_blocks(inode_t inode){
    int num_blocks = (inode.size + super.block_size -1) / super.block_size;
    for(int i = 0; i < super.direct_pointers && i < num_blocks; i++){
        free_iblock(inode.direct[i]);
    }
    if(inode.indirect1 != -1){
        free_all_data_blocks_indirect(inode.indirect1, 1);
        free_iblock(inode.indirect1);
    }
    if(inode.indirect2 != -1){
        free_all_data_blocks_indirect(inode.indirect2, 2);
        free_iblock(inode.indirect2);
    }
    if(inode.indirect3 != -1){
        free_all_data_blocks_indirect(inode.indirect3, 3);
        free_iblock(inode.indirect3);
    }
}

// Check if a pointers block is empty
bool_t is_pointers_block_empty(int iblock){
    DataBlock block;
    block_read(super.beg_data+iblock, (char*) &block);
    return (block.pointers[0] == -1);
}

/////////////////////////////////////////////////////////////////////////////////////

/*
    General Purpose
*/

int max_blocks_of_file(){
    int aux = super.pointers_per_block;
    return aux * aux * aux + // triple indirect
           aux * aux + // double indirect
           aux + // simple indirect
           super.direct_pointers; // direct pointer
}

int blocks_used(){
    int cnt = 0;
    for(int i = 0; i < super.num_data_blocks; i++){
        cnt += ((map.dmap[i/8] & (1<<(7-i%8))) > 0);
    } 
    return cnt;
}

int inodes_used(){
    int cnt = 0;
    for(int i = 0; i < super.num_inodes; i++){
        cnt += ((map.imap[i/8] & (1<<(7-i%8))) > 0);
    } 
    return cnt;
}
