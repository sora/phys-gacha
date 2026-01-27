# phys-gacha: Hugepage-based Physical Memory Allocator

---

## Components

- **`phys_try`**: The core allocator. Allocates hugepages, binds to NUMA, checks contiguity, and holds memory.
- **`allocate_4gb.sh`**: The orchestrator. Retries up to 10 times with memory compaction.
- **`phys_bench`**: Measures raw memory throughput (GB/s) via `/dev/mem`.
- **`phys_peek`**: Directly reads/writes 64-bit values at physical addresses for debugging.

---

## Requirements

### Prerequisites
- Linux Kernel 5.x+
- Hardware supporting 1GB Hugepages
- `libnuma-dev` installed
- Root privileges (sudo)


---

## 4. Getting Started

### Build

```bash
make

```

### Allocation (Secure 4GB of contiguous memory)

Run the script specifying the target NUMA node (e.g., node `0`).

```bash
sudo ./allocate_4gb.sh 0

```


### Outputs

**`allocate_4gb.sh`**


```text
phys-gacha$ sudo ./allocate_4gb.sh 0
Reserving 4 x 1GB hugepages on Node 0...

========================================
   Phys-Gacha SSR Attempt 1 / 10
========================================
Step 1: Allocating 4 independent 1GB pages on Node 0...

--- [phys-gacha] SSR Success! (Physical Contiguity Secured) ---
Idx | Virtual Address Range               | Physical Address Range    | FD
--------------------------------------------------------------------------------------------
  0 | 0x7f4640000000-0x7f467fffffff | 0x000140000000-0x00017fffffff |  3
  1 | 0x7f4680000000-0x7f46bfffffff | 0x000180000000-0x0001bfffffff |  4
  2 | 0x7f46c0000000-0x7f46ffffffff | 0x0001c0000000-0x0001ffffffff |  5
  3 | 0x7f4700000000-0x7f473fffffff | 0x000200000000-0x00023fffffff |  6
--------------------------------------------------------------------------------------------
PID: 2081 | Total Size: 4 GB

[COMMAND HINTS]
1. Benchmarking:
   sudo ./phys_bench 2081 4 3 4 5 6
2. DMA Simulation:
   sudo ./phys_dma_sim 2081 3 4 5 6
3. Peek Physical Memory:
   sudo ./phys_peek 2081 3 0x140000000 <target_phys_addr>

!!! Do not press Enter until you finish using other tools !!!
Press Enter to release memory and exit...

```


**`Benchmark`**

```bash
phys-gacha$ sudo ./phys_bench 2081 4 3 4 5 6
Mapping 4 pages from PID 2081 to aligned vaddr 0x7fa400000000...
Benchmarking 4.0 GB...
  [RESULT] Write Bandwidth: 12.28 GB/s

```


**`Peering (Read/Write Debugging)`**

```bash
phys-gacha$ sudo ./phys_peek 2081 3 0x140000000 0x140000000
Value at 0x140000000: 0x0000000055aa55aa

```


**`DMA sim`**

```bash
phys-gacha$ sudo ./phys_dma_sim 2081 3 4 5 6
CPU: Prepared data 0x55aa55aa at source
DMA: Transferring...
CPU: DMA Finished. Data: 0x55aa55aa

```

