CC = gcc
LD = ld

CFLAGS = -fno-builtin-strlen -fno-builtin-bcopy -fno-builtin-bzero

CCOPTS = -Wall -O1 -c

FAKESHELL_OBJS = shellFake.o shellutilFake.o utilFake.o fsFake.o blockFake.o fsUtil.o

# Makefile targets
all: lnxsh

lnxsh: $(FAKESHELL_OBJS)
	$(CC) -o shell $(FAKESHELL_OBJS)

shellFake.o : shell.c
	$(CC) -Wall $(CFLAGS) -c -DFAKE -o shellFake.o shell.c

fsUtil.o : fsUtil.c
	$(CC) -Wall $(CFLAGS) -c -DFAKE -o fsUtil.o fsUtil.c

shellutilFake.o : shellutilFake.c
	$(CC) -Wall $(CFLAGS) -c -DFAKE -o shellutilFake.o shellutilFake.c

blockFake.o : blockFake.c 
	$(CC) -Wall $(CFLAGS) -c -DFAKE -o blockFake.o blockFake.c

utilFake.o : util.c
	$(CC) -Wall $(CFLAGS) -c -DFAKE -o utilFake.o util.c

fsFake.o : fs.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o fsFake.o fs.c

clean:
	rm -f *.o
	rm -f lnxsh
	rm -f .depend
