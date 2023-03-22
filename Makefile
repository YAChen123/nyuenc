CC=gcc
CFLAGS=-pthread -g -pedantic -std=gnu17 -Wall -Werror -Wextra -Wno-unused

.PHONY: all
all: clean nyuenc

nyuenc: nyuenc.o rle.o

nyuenc.o: nyuenc.c rle.h

rle.o: rle.c rle.h

.PHONY: clean
clean:
	rm -f *.o nyuenc new_input.txt *.enc

