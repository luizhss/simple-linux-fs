#include "util.h"
#include "common.h"
#include "shellutil.h"

#ifdef FAKE
#define START main
#include "fs.h"
#include "fsUtil.h"
#include <stdlib.h>
#include <stdio.h>

extern superblock_t super;
extern dir_t current_dir;
#endif

char line[SIZEX+1];
char *argv[SIZEX];
int argc;


static void readLine(void);
static void parseLine(void);
static void usage(char *s);

static void shell_exit(void);
static void shell_fire(void);
static void shell_clearscreen(void);
static void shell_mkfs(void);
static void shell_open(void);
static void shell_read(void);
static void shell_write(void);
static void shell_lseek(void);
static void shell_close(void);
static void shell_mkdir(void);
static void shell_rmdir(void);
static void shell_cd(void);
static void shell_link(void);
static void shell_unlink(void);
static void shell_stat(void);
static void shell_fsck(void);

static void shell_ls(void);
static void shell_create(void);
static void shell_cat(void);

#define EXEC_COMMAND(c, l, m, s, code) { \
    if (same_string(c, argv[0])) { \
        if ((argc >= l) && (argc <= m)) code; else usage(s); \
        continue; \
    } \
}

int START(void) {
	writeStr("ShellShock Version 0.000003");
	writeChar(RETURN);
	writeChar(RETURN);
	
	shell_init();
	
	while(1) {
		writeStr("# ");
		readLine();
		parseLine();
		
		if (argc == 0)  continue;
		
		EXEC_COMMAND("exit",   1,  1, "", shell_exit());
		EXEC_COMMAND("fire",   1,  1, "", shell_fire());
		EXEC_COMMAND("clear",  1,  1, "", shell_clearscreen());
		EXEC_COMMAND("mkfs",   1,  1, "", shell_mkfs());
		EXEC_COMMAND("open",   3,  3, "", shell_open());
		EXEC_COMMAND("read",   3,  3, "", shell_read());
		EXEC_COMMAND("write",  3,  3, "", shell_write());
		EXEC_COMMAND("lseek",  3,  3, "", shell_lseek());
		EXEC_COMMAND("mkdir",  2,  2, "", shell_mkdir());
		EXEC_COMMAND("rmdir",  2,  2, "", shell_rmdir());
		EXEC_COMMAND("cd",     2,  2, "", shell_cd());
		EXEC_COMMAND("close",  2,  2, "", shell_close());
		EXEC_COMMAND("link",   3,  3, "", shell_link());
		EXEC_COMMAND("unlink", 2,  2, "", shell_unlink());
		EXEC_COMMAND("stat",   2,  2, "", shell_stat());
		EXEC_COMMAND("fsck",   1,  1, "", shell_fsck());
		EXEC_COMMAND("ls",     1,  2, "", shell_ls());
		EXEC_COMMAND("create", 3,  3, "", shell_create());
		EXEC_COMMAND("cat",    2,  2, "", shell_cat());
		writeStr(argv[0]);
		writeStr(" : Command not found.");
		writeChar(RETURN);
	}
	return 0;
}

static void usage(char *s) {
	writeStr("Usage : ");
	writeStr(argv[0]);
	writeStr(s);
	writeChar(RETURN);
}

static void parseLine(void) {
	char *s = line;

	argc = 0;
	while(1) {
		while(*s == ' ') {
			*s = 0;
			s++;
		}
		if (*s == 0)
			return;
		else {
			argv[argc++] = s;
			while ((*s != ' ') && (*s != 0))
				s++;
		}
	}
}

static void readLine(void) {
	int i = 0, c;
	
	do {
		readChar(&c);
		if ((i < MAX_LINE) && (c != RETURN)) {
			line[i++] = c;
		}
	} while (c != RETURN);
	line[i] = 0;
}

static void shell_exit(void) {
	exit(0);
}

static void shell_fire(void) {
	fire();
}

static void shell_clearscreen(void) {
	clearShellScreen();
}

static void shell_mkfs(void) {
	if (fs_mkfs() != 0)
		writeStr("mkfs failed\n");
}

static void shell_create(void) {
	int fd, i, ret;
	char letter[1];

	if ((fd = fs_open(argv[1], FS_O_RDWR)) == -1)
		writeStr("Error creating file\n");
	for(i=0; i < atoi(argv[2]); i++) {
		letter[0] = 'A' + (i % 37);
		if ((i+1) % 40 == 0) {
			letter[0] = RETURN;
			ret = fs_write(fd, letter, 1);
			if(ret <= 0)
				break;
		}else{
			ret = fs_write(fd, letter, 1);
			if(ret <= 0)
				break;
		}
	}
	// We added a message to inform it couldn't write in the file
	if(ret == -1){
		writeStr("Error writing file\n");
	}
	fs_close(fd);
}

static void shell_open(void) {
	int i;
	char s[10];
	
	if ((i = fs_open(argv[1], atoi(argv[2]))) == -1)
		writeStr("Error while opening file\n");
	else {
		itoa(i, s);
		writeStr("File handle is ");
		writeStr(s);
		writeChar(RETURN);
	}
}

static void shell_read(void) {
	char data[SIZEX];
	int i, n, count;
	
	n = atoi(argv[2]);
	if (n > SIZEX) {
		writeStr("Requested size too big\n");
		return;
	}
	if ((count = fs_read(atoi(argv[1]), data, n)) == -1)
		writeStr("Read failed\n");
	else {
		writeStr("Data read in : ");
		for (i = 0; i < count; i++)
			writeChar(data[i]);
		writeChar(RETURN);
	}
}

static void shell_write(void) {
	if (fs_write(atoi(argv[1]), argv[2], strlen(argv[2])) == -1)
		writeStr("Error while writing file\n");
	else
		writeStr("Done\n");
}

static void shell_lseek(void) {
	if (fs_lseek(atoi(argv[1]), atoi(argv[2])) == -1)
		writeStr("Problem with seeking\n");
	else
		writeStr("OK\n");
}

static void shell_close(void) {
	if (fs_close(atoi(argv[1])) == -1)
		writeStr("Problem with closing file\n");
	else
		writeStr("OK\n");
}

static void shell_mkdir(void) {
	if (fs_mkdir(argv[1]) == -1)
		writeStr("Problem with making directory\n");
	else
		writeStr("OK\n");
}

static void shell_rmdir(void) {
	if (fs_rmdir(argv[1]) == -1)
		writeStr("Problem with removing directory\n");
	else
		writeStr("OK\n");
}

static void shell_cd(void) {
	if (fs_cd(argv[1]) == -1)
		writeStr("Problem with changing directory\n");
	else
		writeStr("OK\n");
}

static void shell_ls(void) {
	int i, j, k, current_iblock, sz_file_name;
	DataBlock block;

	inode_t dir_inode = get_inode_per_inum(current_dir.files_inum[0]);

	int num_blocks = (dir_inode.size + super.pointers_per_dcb - 1) / super.pointers_per_dcb;
	for(i = 0; i < num_blocks; i++){
		current_iblock = get_iblock(dir_inode, i);
		if(current_iblock < 0){
			return;
		}

		block_read(super.beg_data + current_iblock, (char *) &block);
		for(j = 0; j < POINTERS_PER_DCB; j++){
			if(block.dir.files_inum[j] == -1){
				j = POINTERS_PER_DCB;
			}else{
				inode_t file_inode = get_inode_per_inum(block.dir.files_inum[j]);

				sz_file_name = strlen((char*)block.dir.files_name[j]);
				writeStr((char*)block.dir.files_name[j]);
				for(k = 0; k < MAX_FILE_NAME - sz_file_name + 1; k++)
					writeChar(' ');
				writeStr(file_inode.type == DIRECTORY ? "D" : "F");
				for(k = 0; k < 4; k++) writeChar(' '); // print 4 space
				writeInt((int) block.dir.files_inum[j]);
				for(k = 0; k < 5; k++) writeChar(' '); // print 5 space
				writeInt(file_inode.type == DIRECTORY ? file_inode.size-2 : file_inode.size);
				writeStr("\n");
			}
		}

	}
}

static void shell_link(void) {
	if (fs_link(argv[1], argv[2]) == -1)
		writeStr("Problem with link\n");
}

static void shell_unlink(void) {
	if (fs_unlink(argv[1]) == -1)
		writeStr("Problem with unlink\n");
}

static void shell_stat(void) {
	fileStat status;
	int ret;
	char s[10];
	
	ret = fs_stat(argv[1], &status);
	if (ret == 0) {
		itoa(status.inodeNo, s);
		writeStr("    Inode No         : "); writeStr(s); writeChar(RETURN);
		if (status.type == FILE_TYPE)
			writeStr("    Type             : FILE\n");
		else
			writeStr("    Type             : DIRECTORY\n");
		itoa(status.links, s);
		writeStr("    Link Count       : "); writeStr(s); writeChar(RETURN);
		itoa(status.size, s);
		writeStr("    Size             : "); writeStr(s); writeChar(RETURN);
		itoa(status.numBlocks, s);
		writeStr("    Blocks allocated : "); writeStr(s); writeChar(RETURN);
	} else{
		writeStr("Problem with stat\n");
	}
}

static void shell_fsck(void){
	fsCheck status;
	char s[10];

	if(fs_fsck(&status) < 0){
		writeStr("Problem with fsck\n");
	}else{
		itohex(status.magic_number, s);
		writeStr("    Magic Number     : 0x"); writeStr(s); writeChar(RETURN);
		itoa(status.inodes_allocated, s);
		writeStr("    Inodes allocated : "); writeStr(s); writeChar('/');
		itoa(super.num_inodes, s);	writeStr(s); writeChar(RETURN);
		writeStr("    Inodes map       : ");
		for(int i = 0; i < super.num_inodes; i++){
			if(i%64 == 0){ 
				writeChar(RETURN); writeStr("      ");
			}
			if((status.map.imap[i/8]&(1<<(7-i%8))))
				writeChar('1');
			else
				writeChar('0');
		}
		writeChar(RETURN);
		itoa(status.blocks_allocated, s);
		writeStr("    Blocks allocated : "); writeStr(s); writeChar('/');
		itoa(super.num_data_blocks, s);	writeStr(s); writeChar(RETURN);
		writeStr("    Blocks map       : ");
		for(int i = 0; i < super.num_data_blocks; i++){
			if(i%64 == 0){ 
				writeChar(RETURN); writeStr("      ");
			}
			if((status.map.dmap[i/8]&(1<<(7-i%8))))
				writeChar('1');
			else
				writeChar('0');
		}
		writeChar(RETURN);
	}
}

static void shell_cat(void) {
	int fd, n, i;
	char buf[256];
	
	fd = fs_open(argv[1], FS_O_RDONLY);
	if (fd < 0) {
		writeStr("Cat failed\n");
		return;
	}
	
	do {
		n = fs_read(fd, buf, 256);
		for (i = 0; i < n; i++) 
			writeChar(buf[i]);
	} while (n > 0);
	fs_close(fd);
	writeChar(RETURN);
}

