CC=gcc
CFLAGS=-pthread -g -pedantic -std=gnu17 -Wall -Werror -Wextra -Wno-unused

.PHONY: all
all: clean nyuenc

nyuenc: nyuenc.o rle.o
	$(CC) $(CFLAGS) nyuenc.o rle.o -o nyuenc

nyuenc.o: nyuenc.c rle.h
	$(CC) $(CFLAGS) -c nyuenc.c

rle.o: rle.c rle.h
	$(CC) $(CFLAGS) -c rle.c

.PHONY: clean
clean:
	rm -f *.o nyuenc new_input.txt *.enc

