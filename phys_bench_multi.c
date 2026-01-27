#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <string.h>

#define GIB (1024ULL * 1024 * 1024)

typedef struct {
    int id; 
    int threads; 
    void *addr; 
    uint64_t total_size;
    pthread_barrier_t *barrier; 
    int mode; // 0:SeqW, 1:SeqR, 2:RandW, 3:RandR
    uint64_t *idx; 
    uintptr_t sum; // 最適化防止用の集計値
} arg_t;

// ヘルパー: FDオープン
int open_proc_fd(int pid, int fd_num) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, fd_num);
    int fd = open(path, O_RDWR);
    if (fd < 0) { perror("open_proc_fd"); exit(1); }
    return fd;
}

// 共通シャッフル関数
void shuffle(uint64_t *arr, uint64_t n) {
    for (uint64_t i = n - 1; i > 0; i--) {
        uint64_t j = rand() % (i + 1);
        uint64_t tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

// スレッドワーカー
void *worker(void *ptr) {
    arg_t *a = (arg_t *)ptr;
    
    // Core Pinning: スレッドをID順のコアに縛り付ける
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(a->id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    uint64_t chunk = a->total_size / a->threads;
    volatile uint64_t *p = (volatile uint64_t *)((char *)a->addr + (a->id * chunk));
    uint64_t n = chunk / sizeof(uint64_t);
    uint64_t local_sum = 0;

    // 全スレッドが揃うまで待機
    pthread_barrier_wait(a->barrier);

    switch(a->mode) {
        case 0: // Seq Write
            for (uint64_t i = 0; i < n; i++) p[i] = i;
            break;
        case 1: // Seq Read
            for (uint64_t i = 0; i < n; i++) local_sum += p[i];
            break;
        case 2: // Rand Write
            for (uint64_t i = 0; i < n; i++) p[a->idx[i]] = i;
            break;
        case 3: // Rand Read
            for (uint64_t i = 0; i < n; i++) local_sum += p[a->idx[i]];
            break;
    }

    // 書き込みを確実に完了させ、読み出し値を保持させるための壁
    __asm__ volatile("" : : "g"(local_sum) : "memory");
    a->sum = local_sum;

    pthread_barrier_wait(a->barrier);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <pid> <pages> <threads> <fd1> <fd2> ...\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]), pages = atoi(argv[2]), threads = atoi(argv[3]);
    uint64_t size = (uint64_t)pages * GIB;

    // 1. アライメント予約 & マッピング
    void *raw = mmap(NULL, size + GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void *vaddr = (void *)(((uintptr_t)raw + GIB - 1) & ~(GIB - 1));

    for (int i = 0; i < pages; i++) {
        int fd = open_proc_fd(pid, atoi(argv[4 + i]));
        mmap((char *)vaddr + (i * GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
        close(fd);
    }

    // 2. テスト準備
    uint64_t n_per_thread = (size / threads) / sizeof(uint64_t);
    pthread_t t[threads];
    arg_t args[threads];
    pthread_barrier_t b;
    pthread_barrier_init(&b, NULL, threads);

    printf("Preparing indices for random access tests...\n");
    for (int i = 0; i < threads; i++) {
        args[i] = (arg_t){i, threads, vaddr, size, &b, 0, malloc(n_per_thread * sizeof(uint64_t)), 0};
        for (uint64_t j = 0; j < n_per_thread; j++) args[i].idx[j] = j;
        shuffle(args[i].idx, n_per_thread);
    }

    // 3. 4パターンのテスト実行
    const char *names[] = {"Seq Write ", "Seq Read  ", "Rand Write", "Rand Read "};
    for (int m = 0; m < 4; m++) {
        printf("Running %s (%d threads)... ", names[m], threads); fflush(stdout);
        for (int i = 0; i < threads; i++) {
            args[i].mode = m;
            args[i].sum = 0;
        }

        struct timespec s, e;
        clock_gettime(CLOCK_MONOTONIC, &s);
        for (int i = 0; i < threads; i++) pthread_create(&t[i], NULL, worker, &args[i]);
        for (int i = 0; i < threads; i++) pthread_join(t[i], NULL);
        clock_gettime(CLOCK_MONOTONIC, &e);

        double sec = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
        printf("%6.2f GB/s\n", (double)size / GIB / sec);
    }

    // 後片付け
    for (int i = 0; i < threads; i++) free(args[i].idx);
    pthread_barrier_destroy(&b);
    return 0;
}

