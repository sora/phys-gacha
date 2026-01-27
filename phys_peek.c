#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <pid> <start_fd> <base_phys> <target_phys> [hex_val]\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    int start_fd = atoi(argv[2]);
    uint64_t base_p = strtoull(argv[3], NULL, 0);
    uint64_t target_p = strtoull(argv[4], NULL, 0);

    if (target_p < base_p) return 1;
    uint64_t offset = target_p - base_p;
    int page_idx = offset / (1024*1024*1024);
    uint64_t page_offset = offset % (1024*1024*1024);

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, start_fd + page_idx);
    int fd = open(path, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    void *map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)(page_offset & ~0xFFFULL));
    volatile uint64_t *ptr = (uint64_t *)((char *)map + (page_offset & 0xFFFULL));

    if (argc == 6) {
        uint64_t val = strtoull(argv[5], NULL, 0);
        *ptr = val;
        printf("Wrote 0x%llx to 0x%llx\n", (unsigned long long)val, (unsigned long long)target_p);
    } else {
        printf("Value at 0x%llx: 0x%016llx\n", (unsigned long long)target_p, (unsigned long long)*ptr);
    }

    munmap(map, 4096); close(fd);
    return 0;
}

