#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#define GIB (1024ULL * 1024 * 1024)

typedef struct {
    int thread_id;
    int num_threads;
    void *start_addr;
    uint64_t total_size;
    pthread_barrier_t *barrier;
    int mode; // 0: Write, 1: Seq Read, 2: Rand Read
    uint64_t *indices; // ランダムアクセス用インデックス
    uint64_t sum;
} thread_arg_t;

int open_proc_fd(int pid, int fd_num) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, fd_num);
    int fd = open(path, O_RDWR);
    if (fd < 0) { perror("open_proc_fd"); exit(1); }
    return fd;
}

// Fisher-Yates シャッフルでインデックスをバラバラにする
void shuffle_indices(uint64_t *indices, uint64_t n) {
    for (uint64_t i = n - 1; i > 0; i--) {
        uint64_t j = rand() % (i + 1);
        uint64_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
}

void *bench_thread(void *arg) {
    thread_arg_t *t = (thread_arg_t *)arg;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(t->thread_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    uint64_t chunk_size = t->total_size / t->num_threads;
    uint64_t *ptr = (uint64_t *)((char *)t->start_addr + (t->thread_id * chunk_size));
    uint64_t n = chunk_size / sizeof(uint64_t);

    pthread_barrier_wait(t->barrier);

    if (t->mode == 0) { // Sequential Write
        for (uint64_t i = 0; i < n; i++) ptr[i] = i;
    } else if (t->mode == 1) { // Sequential Read
        uint64_t local_sum = 0;
        for (uint64_t i = 0; i < n; i++) local_sum += ptr[i];
        t->sum = local_sum;
    } else if (t->mode == 2) { // Random Read (The Nightmare)
        uint64_t local_sum = 0;
        for (uint64_t i = 0; i < n; i++) {
            local_sum += ptr[t->indices[i]];
        }
        t->sum = local_sum;
    }

    pthread_barrier_wait(t->barrier);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <pid> <num_pages> <num_threads> <fd1> <fd2> ...\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    int num_pages = atoi(argv[2]);
    int num_threads = atoi(argv[3]);
    uint64_t total_size = (uint64_t)num_pages * GIB;

    void *raw = mmap(NULL, total_size + GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void *vaddr = (void *)(((uintptr_t)raw + GIB - 1) & ~(GIB - 1));

    for (int i = 0; i < num_pages; i++) {
        int fd = open_proc_fd(pid, atoi(argv[4 + i]));
        mmap((char *)vaddr + (i * GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
        close(fd);
    }

    pthread_t threads[num_threads];
    thread_arg_t args[num_threads];
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads);
    struct timespec s, e;

    // ランダムアクセスのためのインデックス準備 (各スレッドの担当分)
    uint64_t n_per_thread = (total_size / num_threads) / sizeof(uint64_t);
    printf("Preparing random indices... (This might take a moment)\n");
    for (int i = 0; i < num_threads; i++) {
        args[i] = (thread_arg_t){i, num_threads, vaddr, total_size, &barrier, 0, NULL, 0};
        args[i].indices = malloc(n_per_thread * sizeof(uint64_t));
        for (uint64_t j = 0; j < n_per_thread; j++) args[i].indices[j] = j;
        shuffle_indices(args[i].indices, n_per_thread);
    }

    // --- 各テストの実行 ---
    const char *modes[] = {"Sequential Write", "Sequential Read ", "Random Read     "};
    for (int m = 0; m < 3; m++) {
        printf("--- Running %s (%d threads) ---\n", modes[m], num_threads);
        for (int i = 0; i < num_threads; i++) args[i].mode = m;

        clock_gettime(CLOCK_MONOTONIC, &s);
        for (int i = 0; i < num_threads; i++) pthread_create(&threads[i], NULL, bench_thread, &args[i]);
        uint64_t total_sum = 0;
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
            total_sum += args[i].sum;
        }
        clock_gettime(CLOCK_MONOTONIC, &e);

        double sec = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
        printf("  Aggregate Speed: %6.2f GB/s (Time: %.3f s)\n", (double)total_size / GIB / sec, sec);
    }

    for (int i = 0; i < num_threads; i++) free(args[i].indices);
    pthread_barrier_destroy(&barrier);
    return 0;
}

