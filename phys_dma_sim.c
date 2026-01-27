#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define GIB (1024ULL * 1024 * 1024)

typedef struct {
    int fds[4];
    void *aligned_vaddr;
    volatile int status;
} dma_config_t;

void* dma_thread(void* arg) {
    dma_config_t* c = (dma_config_t*)arg;
    
    for(int i=0; i<4; i++) {
        mmap((char*)c->aligned_vaddr + (i*GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, c->fds[i], 0);
    }

    c->status = 1;
    memcpy((char*)c->aligned_vaddr + (2 * GIB), c->aligned_vaddr, GIB);
    c->status = 2;
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: %s <pid> <fd1> <fd2> <fd3> <fd4>\n", argv[0]); return 1; }
    int pid = atoi(argv[1]);
    dma_config_t config = { .status = 0 };

    for(int i=0; i<4; i++) {
        char path[64]; snprintf(path, sizeof(path), "/proc/%d/fd/%s", pid, argv[2+i]);
        config.fds[i] = open(path, O_RDWR);
    }

    // 1GBアライメント予約
    void *raw = mmap(NULL, 5 * GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    config.aligned_vaddr = (void*)(((uintptr_t)raw + GIB - 1) & ~(GIB - 1));

    // ソース準備
    void *src = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED, config.fds[0], 0);
    *((uint32_t*)src) = 0x55AA55AA;
    printf("CPU: Prepared data 0x%x at source\n", *((uint32_t*)src));

    pthread_t tid; pthread_create(&tid, NULL, dma_thread, &config);
    while(config.status != 2) { printf("DMA: Transferring...\n"); usleep(500000); }

    void *dst = mmap(NULL, 4096, PROT_READ, MAP_SHARED, config.fds[2], 0);
    printf("CPU: DMA Finished. Data: 0x%x\n", *((uint32_t*)dst));

    pthread_join(tid, NULL);
    return 0;
}

