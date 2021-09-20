#ifndef FSUTIL_INCLUDED
#define FSUTIL_INCLUDED

#include "common.h"

/* 
    Function to get and set block index number from inode.
*/
int get_indirect_block(uint32_t, int, int);
int get_iblock(inode_t, int);
int set_indirect_iblock(uint32_t, int, int, int);
int set_iblock(inode_t*, int, int);

/*
    Functions to manipulate the map of bits.
*/
int get_single_available_inode();
int get_single_available_iblock();
void free_iblock(int);
void free_inode(int);
void save_map();

/*
    Operations over directories
*/
bool_t is_dir_block_empty(int iblock);
void remove_file_from_dir(inode_t *, int);
int find_file_in_dir(inode_t, char*, int*);
int insert_file_in_dir(inode_t*, char*, int);
dir_t create_directory(int);
bool_t is_directory_empty(inode_t);

/*
    Operations over inodes
*/
void save_inode(int, inode_t);
inode_t get_inode_per_inum(int);

/*
    Operation on Table of Open Files
*/
int get_single_available_fd();

/*
    Operations on Files
*/
void free_all_data_blocks_indirect(int, int);
void free_all_data_blocks(inode_t);
bool_t is_pointers_block_empty(int);

/*
    General Purpose
*/
int max_blocks_of_file();
int blocks_used();
int inodes_used();

#endif