#include <stdio.h>
#include "util.h"
#include "common.h"
#include "fs.h"
#include "shellutil.h"

int system (const char *string );

void shell_init(void) {
	fs_init();
}

void writeChar(int c) {
	if (c == RETURN) 
		c = '\n';
	putchar(c);
}

void clearShellScreen(void) {
	system("clear");
}

void fire(void) {
	writeStr("   B A N G\n");
}

void writeInt(int i){
	char to_str[9];
	itoa(i, to_str);
	for(int i = 0; i < strlen(to_str); i++){
		writeChar(to_str[i]);
	} 
}

void readChar(int *c) {
	*c = getchar();
	if (*c == '\n')
		*c = RETURN;
}

void writeStr(char *s) {
	while(*s != 0) {
		writeChar(*s);
		s++;
	}
}

