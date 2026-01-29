#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <immintrin.h> // mfence用

#define GIB (1024ULL * 1024 * 1024)

// プロセスのFDをオープンするヘルパー
int open_proc_fd(int pid, int fd_num) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, fd_num);
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open_proc_fd");
        exit(1);
    }
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <pid> <num_pages> <fd1> <fd2> ...\n", argv[0]);
        fprintf(stderr, "Example: sudo ./phys_dma_source 1234 4 3 4 5 6\n");
        return 1;
    }

    int pid = atoi(argv[1]);
    int num_pages = atoi(argv[2]);
    uint64_t total_size = (uint64_t)num_pages * GIB;

    // 1. 仮想アドレス空間の予約 (1GBアライメント)
    void *raw_addr = mmap(NULL, total_size + GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw_addr == MAP_FAILED) { perror("mmap reserve"); return 1; }
    uint64_t *vaddr = (uint64_t *)(((uintptr_t)raw_addr + GIB - 1) & ~(GIB - 1));

    // 2. phys_toolで確保されたメモリをマッピング
    printf("--- FPGA DMA Source: Mapping %d GB ---\n", num_pages);
    for (int i = 0; i < num_pages; i++) {
        int fd = open_proc_fd(pid, atoi(argv[3 + i]));
        if (mmap((char *)vaddr + (i * GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
            perror("mmap fixed");
            return 1;
        }
        close(fd);
        printf("  Mapped Page %d to %p\n", i, (char *)vaddr + (i * GIB));
    }

    // 3. テストパターンの生成
    // FPGA側で検証しやすいよう、64bitインデックスとそのビット反転を書き込む
    printf("\n[1/3] Preparing test pattern (Bitwise NOT of index)...\n");
    uint64_t num_elements = total_size / sizeof(uint64_t);
    for (uint64_t i = 0; i < num_elements; i++) {
        // 例: index 0 -> 0xffffffffffffffff, index 1 -> 0xfffffffffffffffe
        vaddr[i] = ~i; 
    }

    // 4. キャッシュの一貫性確保
    // CPUのストアバッファを空にし、メモリへの反映を確定させる
    printf("[2/3] Executing memory barrier (mfence)...\n");
    _mm_mfence(); 

    // 5. FPGAのトリガー待ち
    printf("[3/3] DATA READY IN HOST MEMORY.\n");
    printf("----------------------------------------------------------\n");
    printf("  Target Physical Range: (Check phys_tool output)\n");
    printf("  Pattern Description  : Data at offset 'i*8' is '~i'\n");
    printf("----------------------------------------------------------\n");
    printf(">>> Trigger your FPGA DMA Read now.\n");
    printf(">>> After FPGA confirms completion, press Enter to exit.\n");

    getchar();

    printf("Exiting and unmapping memory.\n");
    munmap(raw_addr, total_size + GIB);
    return 0;
}

