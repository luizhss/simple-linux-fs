#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"
#include "fsUtil.h"
#include <assert.h>

#ifdef FAKE
#include <stdio.h>
#include <stdlib.h>
#endif

superblock_t super;
bmap_t map;
dir_t current_dir;

char current_path[MAX_PATH_NAME];

FileDescriptor table[MAX_OPEN_FILES];

void fs_init(void){
    block_init();
    
    Block block;
    block_read(0, (char *) &block); 

    // check if disk is formatted
    if(block.sb.magic_number == MAGIC_NUMBER){
        super = block.sb; // set superblock

        // load map
        block_read(super.beg_map, (char *) &block);
        map = block.map; // set bits map

        block_read(super.beg_data, (char *) &block);
        current_dir = block.data_block.dir; // set root
        
        // initialize open-files table
        for(int i = 0; i < MAX_OPEN_FILES; i++){
            table[i].fd = -1;
            bzero(table[i].name, MAX_PATH_NAME);
        }
    }else{
        fs_mkfs(); // format disk
    }
}

int fs_mkfs(void){

    char null_block[BLOCK_SIZE];
    bzero(null_block, BLOCK_SIZE);
    for(int i = 0; i < FS_SIZE; i++){
        block_write(i, null_block);
    }

    // define superblock
    super = (superblock_t) {.magic_number = MAGIC_NUMBER,
                            .size_disk = FS_SIZE,
                            .block_size = BLOCK_SIZE,
                            .num_inodes = INODES_PER_BLOCK * INODES_BLOCKS,
                            .num_data_blocks = FS_SIZE - 2 - INODES_BLOCKS,
                            .num_blocks_inodes = INODES_BLOCKS,
                            .beg_inodes = 1, 
                            .beg_map = 1 + INODES_BLOCKS, 
                            .beg_data = 2 + INODES_BLOCKS,
                            .pointers_per_block = BLOCK_SIZE/4,
                            .pointers_per_dcb = POINTERS_PER_DCB,
                            .inodes_per_block = INODES_PER_BLOCK,
                            .direct_pointers = DIRECT_POINTERS
                           };

    // mark inodes and block data as free
    for(int i = 0; i < IMAP_BYTES; i++) map.imap[i] = 0;
    for(int i = 0; i < DMAP_BYTES; i++) map.dmap[i] = 0;

    // create root dir
    for(int i = 0; i < super.pointers_per_dcb; i++)
        current_dir.files_inum[i] = -1;
    bcopy((uint8_t *)  ".",(uint8_t *) current_dir.files_name[0], 2);
    bcopy((uint8_t *) "..",(uint8_t *) current_dir.files_name[1], 3);
    current_dir.files_inum[0] = current_dir.files_inum[1] = 0;

    // create inode to root dir
    inode_t iroot = (inode_t) {.type = DIRECTORY,
                               .link_counter = 1,
                               .size = 2,
                               .indirect1 = -1,
                               .indirect2 = -1,
                               .indirect3 = -1};
    for(int i = 0; i < super.direct_pointers; i++)
            iroot.direct[i] = -1;
    iroot.direct[0] = 0;
    
    // set inum and iblock of root as used
    map.imap[0] |= (1<<7);
    map.dmap[0] |= (1<<7);

    // writing to disk
    Block block;
    block.sb = super;
    block_write(0, (char *) &block); // writing superblock

    block.inodes[0] = iroot;
    block_write(1, (char *) &block); // writing first inode

    save_map(); // writing bits map

    block.data_block.dir = current_dir;
    block_write(super.beg_data, (char *) &block); // writing root directory

    // initialize open-files table
    for(int i = 0; i < MAX_OPEN_FILES; i++){
        table[i].fd = -1;
        bzero(table[i].name, MAX_PATH_NAME);
    }

    // initialize current path
    bcopy((uint8_t*) "/", (uint8_t*) current_path, 2);

    return 0;
}

int fs_open(char *fileName, int flags){

    Block block;
    int ret;
    inode_t inode_dir = get_inode_per_inum(current_dir.files_inum[0]);
    int existFile = find_file_in_dir(inode_dir, fileName, NULL);

    int fd = get_single_available_fd();
    if(fd < 0){
        return -1;
    }

    // if file doesn't exist and flags is RDWR or WRONLY, we must create
    // the file
    if(existFile == -1) {
        if(flags == FS_O_RDONLY){
            // printf("open: File does not exist.");
           return -1;
        }

        // Allocate inode
        int inum = get_single_available_inode();
        if(inum < 0){
            //printf("open: There is no inode available.\n");
            return -1;
        }

        // set inode entries
        inode_t new_ifile = (inode_t) {.type = FILE_TYPE,
                                       .link_counter = 1,
                                       .size = 0,
                                       .indirect1 = -1,
                                       .indirect2 = -1,
                                       .indirect3 = -1};
        for(int i = 0; i < super.direct_pointers; i++)
            new_ifile.direct[i] = -1;

        // update parent        
        ret = insert_file_in_dir(&inode_dir, fileName, inum);
        if(ret < 0){
            free_inode(inum);
            return -1;
        }
        inode_dir.size++;

        block_read(super.beg_data + inode_dir.direct[0], (char *) &block);
        current_dir = block.data_block.dir;

        // write blocks to disk
        save_inode(inum, new_ifile); // writing new inode
        save_inode(current_dir.files_inum[0], inode_dir); // writing new inode

        block.map = map;
        block_write(super.beg_map, (char *) &block); // writing bits map
    
        existFile = inum;
    }


    inode_t current_inode = get_inode_per_inum(existFile);

    if(current_inode.type == DIRECTORY && flags != FS_O_RDONLY){
        return -1;
    }

    // create a new FileDescriptor instance
    FileDescriptor file;
    file.fd = fd; 
    bcopy((uint8_t*) fileName, (uint8_t*)file.name, strlen(fileName)+1);
    file.inode = existFile;
    file.flag = flags;
    file.rw_ptr = 0;

    // insert into the table
    table[fd] = file;

    return fd;
}

int fs_close(int fd){
    
    if(fd >= MAX_OPEN_FILES || table[fd].fd == -1){
        return -1;
    }

    inode_t current_inode = get_inode_per_inum(table[fd].inode);

    // Erase file whether it's its last link
    if (current_inode.link_counter == 0){

        // check if there isn't any other fd open for this inode
        int found = 0;
        for(found = 0; found < MAX_OPEN_FILES; found++){
            if(found != fd &&table[found].fd != -1 && 
                 table[found].inode == table[fd].inode){
                break;
            }
        }

        if(found == MAX_OPEN_FILES){
            // free all data blocks associate with this file
            free_all_data_blocks(current_inode);
            // free its inode
            free_inode(table[fd].inode);
            // save changes
            save_map();
        }
    }

    // close its fd
    table[fd].fd = -1;

    return 0;
}

int fs_read(int fd, char *buf, int count){
    int rw, index_block, iblock;
    inode_t current_inode;
    DataBlock block;

    if(count == 0){
        return 0;
    }

    if(fd >= MAX_OPEN_FILES || table[fd].fd == -1){
        return -1;
    }

    for(int i = 0; i < count; i++) buf[i] = '\0';

    current_inode = get_inode_per_inum(table[fd].inode);
    
    // check if current inode is a file
    if((current_inode.type == DIRECTORY && table[fd].flag != FS_O_RDONLY) 
          || table[fd].flag == FS_O_WRONLY){
        // printf("read: Target cannot be a directory.\n");
        return -1;
    }

    // get current pointer position
    index_block = table[fd].rw_ptr / super.block_size;
    rw = table[fd].rw_ptr % super.block_size;

    int num_blocks = (current_inode.size + super.block_size-1) / super.block_size;
    if(index_block >= num_blocks){
        return 0;
    }

    iblock = get_iblock(current_inode, index_block);

    block_read(super.beg_data + iblock, (char *) &block);
    for(int i = 0; i < count; i++, rw++){

        // if we already look throughout a block, we must load the next one
        if(rw == super.block_size){
            rw = 0;
            iblock = get_iblock(current_inode, ++index_block);
            if(iblock == -1){
                table[fd].rw_ptr += i;
                return i;
            }
            block_read(super.beg_data + iblock, (char *) &block);  
        }

        // check if we got the maximum size of the file 
        if(current_inode.size <= table[fd].rw_ptr+i){
            table[fd].rw_ptr += i;
            return i;
        }

        // save data to buf
        buf[i] = block.data[rw];
    }

    table[fd].rw_ptr += count;
    return count;
}
    
int fs_write(int fd, char *buf, int count){
    int rw, index_block, iblock, need;
    inode_t current_inode;
    DataBlock block;

    if(count == 0){
        return 0;
    }

    if(fd >= MAX_OPEN_FILES || table[fd].fd == -1){
        return -1;
    }

    current_inode = get_inode_per_inum(table[fd].inode);
    
    // check if file is a directory and R/W pointer is valid
    if(current_inode.type == DIRECTORY || table[fd].flag == FS_O_RDONLY){
        // printf("read: Target cannot be a directory.\n");
        return -1;
    }

    need = table[fd].rw_ptr - current_inode.size;
    
    if(need > 0){
        index_block = current_inode.size / super.block_size;
        rw = current_inode.size % super.block_size;
    }else{
        need = 0;
        index_block = table[fd].rw_ptr / super.block_size;
        rw = table[fd].rw_ptr % super.block_size;
    }

    iblock = get_iblock(current_inode, index_block);
    if(iblock == -1){
        if (index_block >= max_blocks_of_file() || blocks_used() == super.num_data_blocks){
            // printf("write: File has maximum size.\n");
            return -1;
        }
        // try to allocate block
        iblock = get_single_available_iblock();
        if(iblock < 0){
            // printf("write: Couldn't write string to file.\n");
            return -1;
        }

        if(set_iblock(&current_inode, index_block, iblock) < 0){
            free_iblock(iblock);
            return -1;
        }
        
        bzero((char *) &block, super.block_size);
        block_write(super.beg_data + iblock, (char *) &block);
    }

    block_read(super.beg_data + iblock, (char *) &block);
    for(int i = 0; i < count+need; i++, rw++){
        if(rw == super.block_size){
            // save block already written
            block_write(super.beg_data + iblock, (char *) &block);

            rw = 0;
            iblock = get_iblock(current_inode, ++index_block);
            if(iblock == -1){
                // allocate if I can
                if (index_block >= max_blocks_of_file()){
                    // printf("write: File has maximum size.\n");
                    return -1;
                }
                iblock = get_single_available_iblock();
                if(iblock < 0){
                    table[fd].rw_ptr += i;
                    if(table[fd].rw_ptr > current_inode.size){
                        current_inode.size = table[fd].rw_ptr;
                    }
                    save_inode(table[fd].inode, current_inode); // save inode
                    save_map();
                    return i;
                }

                if(set_iblock(&current_inode, index_block, iblock) < 0){
                    table[fd].rw_ptr += i;
                    if(table[fd].rw_ptr > current_inode.size){
                        current_inode.size = table[fd].rw_ptr;
                    }
                    save_inode(table[fd].inode, current_inode); // save inode
                    free_inode(iblock);
                    save_map();
                    return i;
                }

            }
            block_read(super.beg_data + iblock, (char *) &block);
        }

        if(i < need){
            block.data[rw] = 0;
        }else{
            block.data[rw] = buf[i-need];
        }
    }

    if(iblock >= 0){
        block_write(super.beg_data + iblock, (char *) &block);
    }

    table[fd].rw_ptr += count;
    if(table[fd].rw_ptr > current_inode.size){
        current_inode.size = table[fd].rw_ptr;
    }   
    save_inode(table[fd].inode, current_inode); // save inode
    save_map();

    return count;
}

int fs_lseek(int fd, int offset){

    if(fd >= MAX_OPEN_FILES || table[fd].fd == -1){
        return -1;
    }

    if(offset >= 0){
        table[fd].rw_ptr = offset;
        return offset;
    }

    return -1;
}

int fs_mkdir(char *fileName){
    //check if dir with that name already exists
    inode_t parent_inode = get_inode_per_inum(current_dir.files_inum[0]);
    if(find_file_in_dir(parent_inode, fileName, NULL) >= 0){
        // printf("mkdir: Directory already exists.\n");
        return -1;
    }
    
    // Allocate inode
    int inum = get_single_available_inode();
    if(inum < 0){
        // printf("mkdir: There is no inode available.\n");
        return -1;
    }

    // allocate data blocks
    int iblock = get_single_available_iblock();
    if(iblock < 0){
        free_inode(inum);
        // printf("mkdir: There is no data block available.\n");
        return -1;
    }

    //set dcb of the new directory
    dir_t new_dir = create_directory(inum);

    // set inode entries
    inode_t new_inode = (inode_t) {.type = DIRECTORY,
                                   .link_counter = 1,
                                   .size = 2,
                                   .indirect1 = -1,
                                   .indirect2 = -1,
                                   .indirect3 = -1};
    for(int i = 0; i < super.direct_pointers; i++)
            new_inode.direct[i] = -1;
    new_inode.direct[0] = iblock;

    // update parent
    Block block;

    insert_file_in_dir(&parent_inode, fileName, inum);
    block_read(super.beg_data + parent_inode.direct[0], (char *) &block);
    current_dir = block.data_block.dir;

    parent_inode.size++;

    // write blocks to disk
    save_inode(inum, new_inode); // writing new inode
    save_inode(current_dir.files_inum[0], parent_inode); // writing new inode

    block.data_block.dir = new_dir;
    block_write(super.beg_data + iblock, (char *) &block); // writing new data block

    block.map = map;
    block_write(MAP_BLOCK, (char *) &block);// writing bits map

    return 0;
}

int fs_rmdir(char *fileName){

    inode_t parent_inode = get_inode_per_inum(current_dir.files_inum[0]);

    // check if fileName exists
    int relIndex;
    int existFile = find_file_in_dir(parent_inode, fileName, &relIndex);
    if(existFile < 0){
        // printf("fs_rmdir: Directory does not exist.\n");
        return -1;
    }

    // check if directory is empty
    inode_t dir_inode = get_inode_per_inum(existFile);
    if(is_directory_empty(dir_inode) == FALSE){
        // printf("fs_rmdir: Directory is not empty.\n");
        return -1;
    }

    // remove subdirectory
    free_iblock(dir_inode.direct[0]); // free its only data block
    free_inode(existFile); // free its inode number

    // remove link of parent dir to subdirectory
    remove_file_from_dir(&parent_inode, relIndex); 
    parent_inode.size--;

    // load newest current dir
    Block aux;
    block_read(super.beg_data + parent_inode.direct[0], (char*) &aux);
    current_dir = aux.data_block.dir;

    // write to disk
    save_inode(current_dir.files_inum[0], parent_inode); // save current dir inode

    save_map();

    return 0;
}

int fs_cd(char *dirName){

    DataBlock block;
    inode_t parent_inode = get_inode_per_inum(current_dir.files_inum[0]);

    // check if fileName exists
    int existFile = find_file_in_dir(parent_inode, dirName, NULL);
    if(existFile < 0){
        // printf("cd: Directory does not exist.\n");
        return -1;
    }

    // find fileName
    inode_t dir_inode = get_inode_per_inum(existFile);
    if(dir_inode.type == FILE_TYPE){
        // printf("cd: Target is not a directory.\n");
        return -1;
    }

    // update current_dir
    int iblock = get_iblock(dir_inode, 0);
    block_read(super.beg_data + iblock, (char *) &block);

    current_dir = block.dir;

    return 0;
}

int fs_link(char *old_fileName, char *new_fileName){
    // check if old_fileName and new_fileName exists
    inode_t parent_inode = get_inode_per_inum(current_dir.files_inum[0]);
    
    int old_inode = find_file_in_dir(parent_inode, old_fileName, NULL);
    if(old_inode < 0){
        // printf("link: File does not exist.\n");
        return -1;
    }

    // if new_fileName exists, finish
    int new_inode = find_file_in_dir(parent_inode, new_fileName, NULL);
    if(new_inode >= 0){
        // printf("link: File already exists.\n");
        return -1;
    }

    // check old fileName is a FILE
    inode_t current_inode = get_inode_per_inum(old_inode);
    if(current_inode.type == DIRECTORY){
        // printf("link: Target cannot a directory.\n");
        return -1;
    }

    // insert new_fileName on directory block
    if(insert_file_in_dir(&parent_inode, new_fileName, old_inode) < 0){
        return -1;
    }
    parent_inode.size++;

    Block block;
    block_read(super.beg_data + parent_inode.direct[0], (char *) &block);
    current_dir = block.data_block.dir;

    // update inode of old_fileName on disk and memory 
    // if its open
    current_inode.link_counter++;

    // save to disk
    save_inode(current_dir.files_inum[0], parent_inode); // save changes in parent inode
    save_inode(old_inode, current_inode); // save changes in file inode

    return 0;
}

int fs_unlink(char *fileName){
    // check if fileName exists
    inode_t parent_inode = get_inode_per_inum(current_dir.files_inum[0]);
    int relIndex, fd;
    int file_inum = find_file_in_dir(parent_inode, fileName, &relIndex);
    if(file_inum < 0){
        // printf("unlink: File does not exist.\n");
        return -1;
    }

    // check if it is a directory
    inode_t current_inode = get_inode_per_inum(file_inum);
    if(current_inode.type == DIRECTORY){
        // printf("unlink: Target cannot a directory.\n");
        return -1;
    }

    // remove link of parent dir to subdirectory
    remove_file_from_dir(&parent_inode, relIndex);
    parent_inode.size--;

    // update link counter of inode on disk and memory
    current_inode.link_counter--;

    for(fd = 0; fd < MAX_OPEN_FILES; fd++){
        if(table[fd].fd != -1 && same_string(table[fd].name, fileName))
            break;
    }

    if(current_inode.link_counter == 0 && fd == MAX_OPEN_FILES){
        // erase file

        // free all data blocks associate with this file
        free_all_data_blocks(current_inode);

        // free its inode
        free_inode(file_inum);
    }else{
        save_inode(file_inum, current_inode); 
    }

    // load newest current dir
    Block aux;
    block_read(super.beg_data + parent_inode.direct[0], (char*) &aux);
    current_dir = aux.data_block.dir;

    // write to disk
    save_inode(current_dir.files_inum[0], parent_inode); // save current dir inode

    save_map(); 

    return 0;
}

int fs_stat(char *fileName, fileStat *buf){

    // get inode of parent
    inode_t parent_inode = get_inode_per_inum(current_dir.files_inum[0]);

    // check if file exists
    int file_inum = find_file_in_dir(parent_inode, fileName, NULL);
    if(file_inum < 0){
        // printf("fs_stat: File does not exist.\n");
        return -1;
    }

    // get inode of fileName
    inode_t file_inode = get_inode_per_inum(file_inum);

    // set buf
    int num_blocks;
    if(file_inode.type == DIRECTORY){
        num_blocks = (file_inode.size + super.pointers_per_dcb-1) / super.pointers_per_dcb;
    }else{
        num_blocks = (file_inode.size + super.block_size-1) / super.block_size;  
    } 
    *buf = (fileStat) {.inodeNo = file_inum,
                       .type = file_inode.type,
                       .links = file_inode.link_counter,
                       .size = file_inode.size,
                       .numBlocks = num_blocks};
    return 0;
}

int fs_fsck(fsCheck *buf){
    *buf = (fsCheck) {.magic_number = super.magic_number,
                      .inodes_allocated = inodes_used(),
                      .blocks_allocated = blocks_used(),
                      .map = map};
    return 0;
}
