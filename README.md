# Simple Unix File System implementation in C

This is an implementation of a File System based on Linux FS.

## How to use

Use the following command to open a shell.

    make && ./shell

## Commands

We implemented the following commands

``mkfs``: formats the disk

`open <filename> <flag>` : opens a file given its name and flags. The flag argument is an integer that corresponds to 

* 1: Read Only;
* 2: Write Only;
* 3: Read and Write.

If the filename does not exist and flag is 2 or 3, then the file is created.

Relative path are not implemented, so you must be on the path where the desired file is (or will be, if you create it).

`read <fd> <size>`: reads \<size> bytes from the file associated with file descriptor \<fd>. This action moves the file pointer of the \<fd> forward.  

`write <fd> <string>`: writes \<string> on file associated with file descriptor \<fd>. This action moves the file pointer of the \<fd> forward.  

`lseek <fd> <offset>`: moves the file pointer associated with file descriptor \<fd> to \<offset> from the beggining of the file. 

`close <fd>`: closes file associated with \<fd>. 

`mkdir <dirname>`: creates a subdirectory named \<dirname> in the current path.

`rmdir <dirname>`: erases the subdirectory named \<dirname> in the current path if it is empty.

`cd <dirname>`: moves current path to \<dirname>.

`link <src_name> <dest_name>`: creates a link between \<src_name> and \<dest_name>. They must be in the same directory.

`unlink <filename>`: removes a link to file \<filename>.

`ls`: lists the current directory contents.

`cat <filename>`: shows the content the given file.

`create <filename> <size>`: creates a file in the current directory named \<filename> and sized \<size> bytes.

`fsck`: prints disk information, such as the magic number, the number of inodes allocated and its bitmap and the number of blocks allocated and its bitmap.

## Implementation details

Blocks have 512 bytes.

We have 256 blocks with 8 inodes each, resulting in a total of 2048 inodes available. 

Each inode has 10 direct blocks, 1 single indirect block, 1 double indirect block and 1 triple indirect block.
