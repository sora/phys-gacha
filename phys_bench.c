#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define GIB (1024ULL * 1024 * 1024)

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <pid> <num_pages> <fd1> <fd2> ...\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    int num_pages = atoi(argv[2]);
    uint64_t total_size = (uint64_t)num_pages * GIB;

    // 1. 1GBアライメントされた仮想空間を確保する
    // total_size + 1GB分を多めに予約し、その中から1GBの倍数地点を見つける
    void *raw_addr = mmap(NULL, total_size + GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw_addr == MAP_FAILED) { perror("mmap reserve"); return 1; }

    // 1GB境界に繰り上げ
    uintptr_t vaddr_aligned = ((uintptr_t)raw_addr + GIB - 1) & ~(GIB - 1);
    void *vaddr = (void *)vaddr_aligned;

    printf("Mapping %d pages from PID %d to aligned vaddr %p...\n", num_pages, pid, vaddr);

    for (int i = 0; i < num_pages; i++) {
        int target_fd = atoi(argv[3 + i]);
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, target_fd);
        
        int fd = open(path, O_RDWR);
        if (fd < 0) { perror("open proc fd"); return 1; }

        // MAP_FIXED で1GB境界にピッタリはめ込む
        void *ret = mmap((char *)vaddr + (i * GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
        if (ret == MAP_FAILED) {
            perror("mmap fixed failed (Alignment issue?)");
            return 1;
        }
        close(fd);
    }

    // 2. ベンチマーク実行
    volatile uint64_t *ptr = (uint64_t *)vaddr;
    uint64_t n = total_size / sizeof(uint64_t);
    struct timespec s, e;

    printf("Benchmarking %.1f GB...\n", (double)total_size / GIB);
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) ptr[i] = i; 
    clock_gettime(CLOCK_MONOTONIC, &e);
    
    double diff = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
    printf("  [RESULT] Write Bandwidth: %.2f GB/s\n", ((double)total_size / 1e9) / diff);

    return 0;
}

