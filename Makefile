CFLAGS=-DLINUX

waddle: waddle.o sound.o

clean:
	rm -f waddle waddle.o sound.o
waddle.o: waddle.c waddle.h
