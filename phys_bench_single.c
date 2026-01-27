#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sched.h>

#define GIB (1024ULL * 1024 * 1024)

double get_sec(struct timespec s, struct timespec e) {
    return (double)(e.tv_sec - s.tv_sec) + (double)(e.tv_nsec - s.tv_nsec) / 1e9;
}

void shuffle(uint64_t *arr, uint64_t n) {
    for (uint64_t i = n - 1; i > 0; i--) {
        uint64_t j = rand() % (i + 1);
        uint64_t tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) { printf("Usage: %s <pid> <num_pages> <fd1>...\n", argv[0]); return 1; }
    
    // コア0に固定
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);

    int pid = atoi(argv[1]), num_pages = atoi(argv[2]);
    uint64_t size = (uint64_t)num_pages * GIB;
    void *raw = mmap(NULL, size + GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void *v = (void *)(((uintptr_t)raw + GIB - 1) & ~(GIB - 1));

    for (int i = 0; i < num_pages; i++) {
        char path[64]; sprintf(path, "/proc/%d/fd/%s", pid, argv[3+i]);
        int fd = open(path, O_RDWR);
        mmap((char*)v + (i*GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
        close(fd);
    }

    uint64_t n = size / sizeof(uint64_t);
    volatile uint64_t *ptr = (uint64_t *)v; // 最適化防止
    struct timespec s, e;
    double sec;
    uint64_t sum = 0;

    printf("--- Single-Thread SSR Benchmark (4-Pattern) ---\n");

    // 1. Seq Write
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) ptr[i] = i;
    clock_gettime(CLOCK_MONOTONIC, &e);
    sec = get_sec(s, e);
    printf("  Seq Write   : %6.2f GB/s\n", (double)size/GIB/sec);

    // 2. Seq Read
    sum = 0;
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) {
        sum += ptr[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    // ダミーのアセンブラ壁を置いて、sumの計算を強制する
    __asm__ volatile("" : : "g"(sum) : "memory");
    sec = get_sec(s, e);
    printf("  Seq Read    : %6.2f GB/s\n", (double)size/GIB/sec);

    // インデックス準備
    uint64_t *idx = malloc(n * sizeof(uint64_t));
    for(uint64_t i=0; i<n; i++) idx[i] = i;
    printf("  Shuffling indices for random access...\n");
    shuffle(idx, n);

    // 3. Rand Write
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) {
        ptr[idx[i]] = i;
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    sec = get_sec(s, e);
    printf("  Rand Write  : %6.2f GB/s (%6.2f ns/op)\n", (double)size/GIB/sec, (sec * 1e9)/n);

    // 4. Rand Read
    sum = 0;
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) {
        sum += ptr[idx[i]];
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    __asm__ volatile("" : : "g"(sum) : "memory");
    sec = get_sec(s, e);
    printf("  Rand Read   : %6.2f GB/s (%6.2f ns/op)\n", (double)size/GIB/sec, (sec * 1e9)/n);

    printf("-----------------------------------------------\n");
    printf("CheckSum: %lx\n", sum);
    free(idx);
    return 0;
}

