#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <linux/memfd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <numaif.h>
#include <fcntl.h>

#define GIB (1024ULL * 1024 * 1024)

struct hugepage_info {
    int fd;
    void *vaddr_orig;
    uintptr_t paddr;
};

int compare_pages(const void *a, const void *b) {
    uintptr_t pa = ((struct hugepage_info *)a)->paddr;
    uintptr_t pb = ((struct hugepage_info *)b)->paddr;
    return (pa < pb) ? -1 : (pa > pb) ? 1 : 0;
}

uint64_t get_paddr(uintptr_t vaddr) {
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) return 0;
    uint64_t data = 0;
    if (pread(fd, &data, 8, (off_t)((vaddr / 4096) * 8)) != 8) {
        close(fd); return 0;
    }
    close(fd);
    return (data & ((1ULL << 54) - 1)) * 4096;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <node> <pages>\n", argv[0]);
        return 2;
    }
    int node = atoi(argv[1]);
    int pages = atoi(argv[2]);
    unsigned long nodemask = (1UL << node);

    struct hugepage_info *hp = malloc(sizeof(struct hugepage_info) * pages);

    printf("Step 1: Allocating %d independent 1GB pages on Node %d...\n", pages, node);
    for (int i = 0; i < pages; i++) {
        hp[i].fd = (int)syscall(SYS_memfd_create, "phys_page", MFD_CLOEXEC | MFD_HUGETLB | (30 << MFD_HUGE_SHIFT));
        if (hp[i].fd == -1) { perror("memfd_create"); return 1; }
        if (ftruncate(hp[i].fd, (off_t)GIB) == -1) { perror("ftruncate"); return 1; }
        hp[i].vaddr_orig = mmap(NULL, GIB, PROT_READ | PROT_WRITE, MAP_SHARED, hp[i].fd, 0);
        mbind(hp[i].vaddr_orig, GIB, MPOL_BIND, &nodemask, sizeof(nodemask)*8, MPOL_MF_STRICT);
        *(volatile char *)hp[i].vaddr_orig = 0;
        hp[i].paddr = get_paddr((uintptr_t)hp[i].vaddr_orig);
    }

    // 物理アドレス順にソート（DPDKスタイル）
    qsort(hp, pages, sizeof(struct hugepage_info), compare_pages);

    int contiguous = 1;
    for (int i = 1; i < pages; i++) {
        if (hp[i-1].paddr + GIB != hp[i].paddr) contiguous = 0;
    }

    if (contiguous) {
        printf("\n--- [phys-gacha] SSR Success! (Physical Contiguity Secured) ---\n");
        
        // 1GBアライメントされた仮想アドレス空間を確保
        void *raw = mmap(NULL, (size_t)(pages + 1) * GIB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        void *final_vaddr = (void *)(((uintptr_t)raw + GIB - 1) & ~(GIB - 1));
        
        printf("%-3s | %-35s | %-25s | %-s\n", "Idx", "Virtual Address Range", "Physical Address Range", "FD");
        printf("--------------------------------------------------------------------------------------------\n");

        for (int i = 0; i < pages; i++) {
            void *v_start = (char *)final_vaddr + (i * GIB);
            void *v_end   = (char *)v_start + GIB - 1;
            uint64_t p_start = hp[i].paddr;
            uint64_t p_end   = p_start + GIB - 1;

            mmap(v_start, GIB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, hp[i].fd, 0);
            munmap(hp[i].vaddr_orig, GIB);

            printf("%3d | %p-%p | 0x%012llx-0x%012llx | %2d\n", 
                   i, v_start, v_end, (unsigned long long)p_start, (unsigned long long)p_end, hp[i].fd);
        }

        printf("--------------------------------------------------------------------------------------------\n");
        printf("PID: %d | Total Size: %d GB\n", getpid(), pages);
        
        // コマンドHINTの生成
        char fd_list[256] = "";
        char *ptr = fd_list;
        for(int i=0; i<pages; i++) ptr += sprintf(ptr, "%d ", hp[i].fd);

        printf("\n\033[1;33m[COMMAND HINTS]\033[0m\n");
        printf("1. Benchmarking:\n");
        printf("   \033[1;32msudo ./phys_bench %d %d %s\033[0m\n", getpid(), pages, fd_list);
        printf("2. DMA Simulation:\n");
        printf("   \033[1;32msudo ./phys_dma_sim %d %s\033[0m\n", getpid(), fd_list);
        printf("3. Peek Physical Memory:\n");
        printf("   \033[1;32msudo ./phys_peek %d %d 0x%llx <target_phys_addr>\033[0m\n", 
               getpid(), hp[0].fd, (unsigned long long)hp[0].paddr);
        
        printf("\n\033[1;31m!!! Do not press Enter until you finish using other tools !!!\033[0m\n");
        printf("Press Enter to release memory and exit...");
        fflush(stdout);
        
        char buf;
        if (read(0, &buf, 1) < 0) {} 
        free(hp);
        return 0;
    }

    printf("\n[FAILED] Fragmented. Gacha again!\n");
    for(int i=0; i<pages; i++) { close(hp[i].fd); munmap(hp[i].vaddr_orig, GIB); }
    free(hp);
    return 1;
}

