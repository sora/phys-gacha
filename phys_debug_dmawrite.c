#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define GIB (1024ULL * 1024 * 1024)

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <pid> <pages> <fd1> <fd2>...\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]), pages = atoi(argv[2]);
    uint64_t size = (uint64_t)pages * GIB;

    // 1GBアライメントでマッピング
    void *raw = mmap(NULL, size + GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint64_t *ptr = (uint64_t *)(((uintptr_t)raw + GIB - 1) & ~(GIB - 1));
    for (int i = 0; i < pages; i++) {
        char path[64]; sprintf(path, "/proc/%d/fd/%s", pid, argv[3+i]);
        int fd = open(path, O_RDWR);
        mmap((char*)ptr + (i * GIB), GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
        close(fd);
    }

    // 1. バッファの初期化（FPGAが書く前にゴミを消す）
    printf("Cleaning buffer...\n");
    for (uint64_t i = 0; i < size / 8; i++) ptr[i] = 0xDEADBEEFDEADBEEF;

    printf("Ready for FPGA DMA Write. Please trigger FPGA now.\n");
    printf("Press Enter after FPGA finished DMA...");
    getchar();

    // 2. 検算
    printf("Verifying data...\n");
    uint64_t errors = 0;
    for (uint64_t i = 0; i < size / 8; i++) {
        if (ptr[i] != i) {
            if (errors < 10) {
                printf("Error at index %lu: Expected %016lx, Got %016lx\n", i, i, ptr[i]);
            }
            errors++;
        }
    }

    if (errors == 0) printf("Successfully Verified! All data is correct.\n");
    else printf("Verification Failed. Total Errors: %lu\n", errors);

    return 0;
}

