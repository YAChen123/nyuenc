#ifndef _RLE_H_
#define _RLE_H_

// some function
int get_num_thread(int argc, char **argv);
int sequential(char *addr, size_t size);
int parallel(int jobs, char *addr, size_t size);
int rle(int argc, char **argv);

#endif
