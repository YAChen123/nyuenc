# 2023 Spring Operating Systems --- Lab 3: Encoder

## Introduction
**Data compression** is the process of encoding information using fewer bits than the original representation. 
**Run-length encoding (RLE)** is a simple yet effective compression algorithm: repeated data are stored as a single data and the count. 
In this lab, you will build a parallel run-length encoder called **Not Your Usual ENCoder**, or `nyuenc` for short.

## Objectives
Through this lab, you will:

* Familiarize yourself with multithreaded programming using POSIX threads.
* Learn how to implement a thread pool using mutexes and condition variables.
* Learn how to use a thread pool to parallelize a program.
* Get a better understanding of key IPC concepts and issues.
* Be a better C programmer and be better prepared for your future technical job interviews. In particular, the data encoding technique that you will practice in this lab frequently appears in interview questions.

## Overview
Run-length encoding (RLE) is quite simple. When you encounter n characters of the same type in a row, the encoder (`nyuenc`) will turn that into a single instance of the character followed by the count n.

For example, a file with the following content:

```
aaaaaabbbbbbbbba
```
would be encoded (logically, as the numbers would be in binary format instead of ASCII) as:

```
a6b9a1
```

Note that the exact format of the encoded file is important. You will store the character in ASCII and the count as a **1-byte unsigned integer in binary format**. See the example below for the actual content in the output file.

In this example, the original file is 16 bytes, and the encoded file is 6 bytes.

## Milestone 1: sequential RLE

You will first implement `nyuenc` as a single-threaded program. The encoder reads from one or more files specified as command-line arguments and writes to `STDOUT`. Thus, the typical usage of `nyuenc` would use shell redirection to write the encoded output to a file.

Note that you should use `xxd` to inspect a binary file since not all characters are printable. `xxd` dumps the file content in hexadecimal.

For example, let’s encode the aforementioned file.


```
$ echo -n "aaaaaabbbbbbbbba" > file.txt
$ xxd file.txt
0000000: 6161 6161 6161 6262 6262 6262 6262 6261  aaaaaabbbbbbbbba
$ ./nyuenc file.txt > file.enc
$ xxd file.enc
0000000: 6106 6209 6101                           a.b.a.
```

If multiple files are passed to `nyuenc`, they will be **concatenated** and encoded into a single compressed output. For example:

```
$ echo -n "aaaaaabbbbbbbbba" > file.txt
$ xxd file.txt
0000000: 6161 6161 6161 6262 6262 6262 6262 6261  aaaaaabbbbbbbbba
$ ./nyuenc file.txt file.txt > file2.enc
$ xxd file2.enc
0000000: 6106 6209 6107 6209 6101                 a.b.a.b.a.
```
Note that the last `a` in the first file and the leading `a`’s in the second file are merged.

## Milestone 2: parallel RLE

Next, you will parallelize the encoding using POSIX threads. In particular, you will implement a thread pool for executing encoding tasks.

You should use mutexes, condition variables, or semaphores to realize proper synchronization among threads. **Your code must be free of race conditions. You must not perform busy waiting, and you must not use** `sleep()`, `usleep()`, or `nanosleep()`.

Your `nyuenc` will take an optional command-line option `-j jobs`, which specifies the number of worker threads. (If no such option is provided, it runs sequentially.)

For example:
```
$ time ./nyuenc file.txt > /dev/null
real    0m0.527s
user    0m0.475s
sys     0m0.233s
$ time ./nyuenc -j 3 file.txt > /dev/null
real    0m0.191s
user    0m0.443s
sys     0m0.179s
```
You can see the difference in running time between the sequential version and the parallel version. (Note: redirecting to `/dev/null` discards all output, so the time won’t be affected by I/O.)

## How to parallelize the encoding

Think about what can be done in parallel and what must be done serially by a single thread.

Also, think about how to divide the encoding task into smaller pieces. Note that the input files may vary greatly in size. Will all the worker threads be fully utilized?

Here is an illustration of a thread pool from Wikipedia:

![Wikipedia](https://upload.wikimedia.org/wikipedia/commons/0/0c/Thread_pool.svg)

At the beginning of your program, you should create a pool of worker threads (the green boxes in the figure above). The number of threads is specified by the command-line argument `-j jobs`.

The main thread should divide the input data logically into fixed-size 4KB (i.e., 4,096-byte) chunks and submit the tasks (the blue circles in the figure above) to the task queue, where each task would encode a chunk. Whenever a worker thread becomes available, it would execute the next task in the task queue. (Note: it’s okay if the last chunk of a file is smaller than 4KB.)

For simplicity, you can assume that the task queue is unbounded. In other words, you can submit all tasks at once without being blocked.

After submitting all tasks, the main thread should collect the results (the yellow circles in the figure above) and write them to `STDOUT`. Note that you may need to stitch the chunk boundaries. For example, if the previous chunk ends with `aaaaa`, and the next chunk starts with `aaa`, instead of writing `a5a3`, you should write `a8`.

It is important that you synchronize the threads properly so that there are no deadlocks or race conditions. In particular, there are two things that you need to consider carefully:

* The worker thread should wait until there is a task to do.
* The main thread should wait until a task has been completed so that it can collect the result. Keep in mind that the tasks might not complete in the same order as they were submitted.


## Setup
1. Clone this repository in your computer
2. Inside the `nyuenc` folder terminal, run `make` to compile and create an executable file
3. run `./nyuenc file.txt > file.enc `

Lab Website: https://cs.nyu.edu/courses/spring23/CSCI-GA.2250-002/nyuenc
