#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define GIB (1024ULL * 1024 * 1024)

// 時間計測用ユーティリティ
double get_sec(struct timespec s, struct timespec e) {
    return (double)(e.tv_sec - s.tv_sec) + (double)(e.tv_nsec - s.tv_nsec) / 1e9;
}

void print_result(const char *name, uint64_t bytes, double sec) {
    double gb = (double)bytes / GIB;
    printf("  %-20s: %6.2f GB/s (%6.2f GiB in %.3f sec)\n", name, gb / sec, gb, sec);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <pid> <num_pages> <fd1> <fd2> ...\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    int num_pages = atoi(argv[2]);
    uint64_t total_size = (uint64_t)num_pages * GIB;

    // 1. 1GBアライメント予約 (Hugepage対応)
    void *raw_addr = mmap(NULL, total_size + GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw_addr == MAP_FAILED) { perror("mmap reserve"); return 1; }
    void *vaddr = (void *)(((uintptr_t)raw_addr + GIB - 1) & ~(GIB - 1));

    printf("--- Phys-Gacha Benchmark (Total %d GB) ---\n", num_pages);

    // 2. マッピング
    for (int i = 0; i < num_pages; i++) {
        int target_fd = atoi(argv[3 + i]);
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, target_fd);
        int fd = open(path, O_RDWR);
        if (fd < 0) { perror("open proc fd"); return 1; }

        if (mmap((char *)vaddr + (i * GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
            perror("mmap fixed failed"); return 1;
        }
        close(fd);
    }

    struct timespec s, e;
    volatile uint64_t *ptr = (uint64_t *)vaddr;
    uint64_t n = total_size / sizeof(uint64_t);
    uint64_t sum = 0;

    // Test 1: Sequential Write (Simple Fill)
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) ptr[i] = i;
    clock_gettime(CLOCK_MONOTONIC, &e);
    print_result("Sequential Write", total_size, get_sec(s, e));

    // Test 2: Sequential Read (Summation)
    // コンパイラによる最適化削除を防ぐため、結果をsumに加算
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) sum += ptr[i];
    clock_gettime(CLOCK_MONOTONIC, &e);
    print_result("Sequential Read", total_size, get_sec(s, e));

    // Test 3: Read-Modify-Write (RMW)
    // データベースやパケットカウンタの更新を想定
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (uint64_t i = 0; i < n; i++) ptr[i] = ptr[i] + 1;
    clock_gettime(CLOCK_MONOTONIC, &e);
    print_result("Read-Modify-Write", total_size, get_sec(s, e));

    // Test 4: Internal Memory Copy (memcpy)
    // 最初の1GBを次の1GBにコピー。全域を半分ずつ使ってコピー
    if (num_pages >= 2) {
        uint64_t half_size = total_size / 2;
        clock_gettime(CLOCK_MONOTONIC, &s);
        memcpy((void *)((char *)vaddr + half_size), (void *)vaddr, half_size);
        clock_gettime(CLOCK_MONOTONIC, &e);
        // memcpyはReadとWriteの両方が発生するため、処理データ量は2倍として計算
        print_result("Memory Copy", half_size * 2, get_sec(s, e));
    }

    printf("-------------------------------------------\n");
    printf("Benchmark Complete. (Check Sum: %lx)\n", sum);

    return 0;
}

