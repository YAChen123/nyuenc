#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "rle.h"

#define BUF_SIZE 1024

// paese command-line options using getopt() to get number of jobs
int get_num_thread(int argc, char **argv){
    int opt;
    int jobs = 1;
    while((opt = getopt(argc, argv, "j:")) != -1){
        switch(opt){
            case 'j':
                jobs = atoi(optarg);
                break;
            default:
                jobs = 1;
                break;
        }
    }
    return jobs;
}

// addr is the pointer to concat multiple input files 
// using the RLE simple algorithm to You will store the character 
// in ASCII and the count as a 1-byte unsigned integer in binary format.
int sequential(char *addr, size_t size){
    unsigned char count = 1; 
    char prev_char = addr[0];

    // iterate over addr
    for(size_t i = 1; i<size; i++){
        char curr_char = addr[i];

        // we can assume that no character will appear more than 255 times in a row. 
        if(curr_char == prev_char){
            count++;
        }else{
            unsigned char data[2] = {prev_char, count};
            fwrite(data, sizeof(unsigned char), 2, stdout);
            count = 1;
            prev_char = curr_char;
        }
    }
    unsigned char data[2] = {prev_char, count};
    fwrite(data, sizeof(unsigned char), 2, stdout);

    return 0;
}

int parallel(){
    return 0;
}

int rle(int argc, char **argv){
    int jobs = get_num_thread(argc, argv);

    int fd_in, fd_new_in;
    struct stat sb;
    char buf[BUF_SIZE];
    ssize_t nread;

    fd_new_in = open("new_input.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd_new_in == -1){
        perror("open");
        exit(0);
    }

    for(int i = 1; i < argc; i++){
        fd_in = open(argv[i], O_RDONLY);
        if(fd_in == -1){
            perror("open");
            exit(0);
        }

        while ((nread = read(fd_in, buf, BUF_SIZE)) > 0) {
            if (write(fd_new_in, buf, nread) != nread) {
                perror("write");
                exit(0);
            }
        }

        if (nread == -1) {
            perror("read");
            exit(0);
        }

        if (close(fd_in) == -1) {
            perror("close");
            exit(0);
        }
    }

    if (close(fd_new_in) == -1) {
        perror("close");
        exit(0);
    }

    fd_new_in = open("new_input.txt", O_RDONLY);
    if (fd_new_in == -1) {
        perror("open");
        exit(0);
    }

    if (fstat(fd_new_in, &sb) == -1) {
        perror("fstat");
        exit(0);
    }

    char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd_new_in, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        exit(0);
    }

    if(jobs == 1){
        sequential(addr, sb.st_size);
    }else{
        parallel();
    }

    if (munmap(addr, sb.st_size) == -1) {
        perror("munmap");
        exit(0);
    }

    if (close(fd_new_in) == -1) {
        perror("close");
        exit(0);
    }

    return 0;
}

