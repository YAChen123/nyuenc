#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include "rle.h"

#define BUF_SIZE 1024
#define CHUNK_SIZE 4096

typedef struct{
    char *data;
    size_t size;
    int index;
} Chunk;

typedef struct{
    Chunk **chunks;
    int num_chunks;
    int current_chunk;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TaskQueue;

typedef struct{
    unsigned char *data;
    size_t size;
    int index;
} Result;

typedef struct{
    Result **results;
    int num_results;
    int current_result;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ResultQueue;

typedef struct{
    TaskQueue *taskQueue;
    ResultQueue *resultQueue;
} WorkerArgs;

TaskQueue *create_task_queue(char *data, size_t size){
    TaskQueue *queue = (TaskQueue *)malloc(sizeof(TaskQueue));
    if(queue == NULL){
        perror("malloc");
    }
    if(size <= CHUNK_SIZE){
        queue->num_chunks = 1;
    }else{
        queue->num_chunks = size / CHUNK_SIZE;
        if (size % CHUNK_SIZE != 0){
            queue->num_chunks++;
        }
    }
    
    queue->chunks = (Chunk **)malloc(queue->num_chunks * sizeof(Chunk *));
    if(queue->chunks == NULL){
        perror("malloc");
    }
    queue->current_chunk = 0;
    for (int i = 0; i < queue->num_chunks; i++){
        Chunk *chunk = (Chunk *)malloc(sizeof(Chunk));
        if(chunk == NULL){
            perror("malloc");
        }
        chunk->index = i;
        if (i < queue->num_chunks - 1){
            chunk->size = CHUNK_SIZE;
        }
        else{
            chunk->size = size % CHUNK_SIZE == 0 ? CHUNK_SIZE : size % CHUNK_SIZE;
        }
        chunk->data = (char *)malloc(chunk->size * sizeof(char));
        if(chunk->data == NULL){
            perror("malloc");
        }
        memcpy(chunk->data, data + i * CHUNK_SIZE, chunk->size);
        queue->chunks[i] = chunk;
    }
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

void destroy_task_queue(TaskQueue *queue){
    for (int i = 0; i < queue->num_chunks; i++){
        free(queue->chunks[i]->data);
        free(queue->chunks[i]);
    }
    free(queue->chunks);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}

Chunk *dequeue_task(TaskQueue *queue){
    pthread_mutex_lock(&queue->mutex);
    Chunk *chunk = NULL;
    if(queue->current_chunk < queue->num_chunks){
        chunk = queue->chunks[queue->current_chunk];
        queue->current_chunk++;
    }
    pthread_mutex_unlock(&queue->mutex);
    return chunk;
}

ResultQueue *create_result_queue(int num_results){
    ResultQueue *queue = (ResultQueue *)malloc(sizeof(ResultQueue));
    queue->num_results = num_results;
    queue->results = (Result **)malloc(num_results * sizeof(Result *));
    for (int i = 0; i < num_results; i++){
        queue->results[i] = NULL;
    }
    queue->current_result = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

void destroy_result_queue(ResultQueue *queue){
    for (int i = 0; i < queue->num_results; i++){
        free(queue->results[i]->data);
        free(queue->results[i]);
    }
    free(queue->results);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}

void enqueue_result(ResultQueue *queue, Result *result){
    pthread_mutex_lock(&queue->mutex);
    if(result->index < queue->num_results){
        queue->results[result->index] = result;
    }
    //pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

Result *dequeue_result(ResultQueue *queue, int index){
    pthread_mutex_lock(&queue->mutex);
    Result *result = queue->results[index];
    pthread_mutex_unlock(&queue->mutex);
    return result;
}

void *worker_thread(void *arg){
    WorkerArgs *worker_arg = (WorkerArgs *) arg;
    while (1){
        Chunk *chunk = dequeue_task(worker_arg->taskQueue);
        if (chunk == NULL){
            break;
        }

        // Process the chunk here
        char *addr = chunk->data;

        // Create a Result struct
        Result *result = (Result *)malloc(sizeof(Result));
        result->data = (unsigned char *)malloc(chunk->size*2 * sizeof(unsigned char));
        result->size = 0;
        result->index = chunk->index;

        unsigned char count = 1; 
        char prev_char = addr[0];

        // iterate over addr
        for(size_t i = 1; i<chunk->size; i++){
            char curr_char = addr[i];

            // we can assume that no character will appear more than 255 times in a row. 
            if(curr_char == prev_char){
                count++;
            }else{
                //unsigned char data[2] = {prev_char, count};
                result->data[result->size] = prev_char;
                result->size++;
                result->data[result->size] = count;
                result->size++;

                count = 1;
                prev_char = curr_char;
            }
        }
        //unsigned char data[2] = {prev_char, count};
        result->data[result->size] = prev_char;
        result->size++;
        result->data[result->size] = count;
        result->size++;
        result->data = (unsigned char *) realloc(result->data, result->size);

        // Enqueue the Result
        enqueue_result(worker_arg->resultQueue, result);
        //free(task_queue);
    }
    pthread_exit(NULL);
}

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

int parallel(int num_threads, char *addr, size_t size){
    TaskQueue *task_queue = create_task_queue(addr, size);
    ResultQueue *result_queue = create_result_queue(task_queue->num_chunks);
    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    WorkerArgs *args = (WorkerArgs *)malloc(num_threads * sizeof(WorkerArgs));

    for (int i = 0; i < num_threads; i++){
        args[i].taskQueue = task_queue;
        args[i].resultQueue = result_queue;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }

    for (int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }

    Result *prev_result = dequeue_result(result_queue, 0); 
    for (int i = 1; i < result_queue->num_results; i++){
        Result *curr_result = dequeue_result(result_queue,i);
        char curr_head_char = curr_result->data[0];
        char prev_tail_char = prev_result->data[prev_result->size -2];

        if(curr_head_char == prev_tail_char){
            curr_result->data[1] = prev_result->data[prev_result->size -1] + curr_result->data[1];
            
            //prev_result->data[prev_result->size -1] = prev_result->data[prev_result->size -1] + curr_result->data[1];
            //curr_result->data+=2;
            //curr_result->size-=2;
            //curr_result->data = (unsigned char *) realloc(curr_result->data, curr_result->size);
            for(size_t j = 0; j<prev_result->size-2;j++){
                fwrite(&prev_result->data[j], sizeof(unsigned char), 1, stdout);
            }
            prev_result = curr_result;
        }else{
            fwrite(prev_result->data, sizeof(unsigned char), prev_result->size, stdout);
            prev_result = curr_result;
        }
    }
    fwrite(prev_result->data, sizeof(unsigned char), prev_result->size, stdout);

    destroy_task_queue(task_queue);
    destroy_result_queue(result_queue);
    free(threads);
    free(args);

    return 0;
}


int rle(int argc, char **argv){
    int jobs = get_num_thread(argc, argv);

    int fd_in, fd_new_in;
    struct stat sb;
    char buf[BUF_SIZE];
    ssize_t nread;

    // Concat the multiple input files into new_input.txt
    fd_new_in = open("new_input.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if(fd_new_in == -1){
        perror("open");
        exit(0);
    }

    int i;
    if(jobs == 1){
        i = 1;
    }else{
        i = 3;
    }

    for(i; i < argc; i++){
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
    // Read new_input.txt into pointer char array *addr
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

    // detmerine the number of threads for sequential or parallel RLE
    if(jobs == 1){
        sequential(addr, sb.st_size);
    }else{
        parallel(jobs, addr, sb.st_size);
    }

    // free addr
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

