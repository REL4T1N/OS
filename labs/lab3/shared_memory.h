#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>


/*
размер страницы:
```bash
relat@relatinchik:/mnt/c/C/OS/labs/lab3$ getconf PAGESIZE
4096
```
Это аппаратно-определённое значение для эффективной работы MMU (Memory Management Unit)
*/
#define SHARED_SIZE 4096

typedef struct {
    char data[SHARED_SIZE];
    int data_ready;
    int process_complete;
} shared_memory;

shared_memory *create_shared_memory(const char* name);
shared_memory *open_shared_memory(const char* name);
void close_shared_memory(shared_memory *shm, const char* name, int unlink);

pid_t fork_process();

#endif