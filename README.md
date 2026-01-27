# phys-gacha: Hugepage-based Physical Memory Allocator

---

## Components

| Component | Description |
| --- | --- |
| **`phys_try`** | Gacha engine that secures contiguous 1GB hugepages and exports them via file descriptors for cross-process access. |
| **`allocate_4gb.sh`** | A manager script that handles system-level hugepage reservation and memory "massage" (defragmentation) to ensure SSR success. |
| **`phys_bench_single`** | Analyzes single-core performance, measuring DRAM latency and optimization-resistant sequential/random access patterns. |
| **`phys_bench_multi`** | Saturates hardware memory channels using parallel threads and core pinning to determine the system's aggregate bandwidth limits. |
| **`phys_peek`** | A surgical utility to read or write specific physical addresses within the allocated sanctuary via PID and FD mapping. |

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

--- [phys-gacha] SSR Success! ---
Idx | Virtual Address Range               | Physical Address Range    | FD
--------------------------------------------------------------------------------------------
  0 | 0x7fa600000000-0x7fa63fffffff | 0x000140000000-0x00017fffffff |  3
  1 | 0x7fa640000000-0x7fa67fffffff | 0x000180000000-0x0001bfffffff |  4
  2 | 0x7fa680000000-0x7fa6bfffffff | 0x0001c0000000-0x0001ffffffff |  5
  3 | 0x7fa6c0000000-0x7fa6ffffffff | 0x000200000000-0x00023fffffff |  6
--------------------------------------------------------------------------------------------

[COMMAND HINTS]
1. Single-Thread Bench (Core 0, Latency focus):
   sudo ./phys_bench_single 3289 4 3 4 5 6
2. Multi-Thread Bench (Bandwidth focus, e.g., 4 threads):
   sudo ./phys_bench_multi 3289 4 4 3 4 5 6
3. DMA Simulation:
   sudo ./phys_dma_sim 3289 3 4 5 6
4. Peek Physical Memory:
   sudo ./phys_peek 3289 3 0x140000000 <target_phys_addr>

```


**`Benchmark (single core)`**

```bash
phys-gacha$ sudo ./phys_bench_single 3235 4 3 4 5 6
--- Single-Thread SSR Benchmark (4-Pattern) ---
  Seq Write   :  13.97 GB/s
  Seq Read    :  24.79 GB/s
  Shuffling indices for random access...
  Rand Write  :   0.96 GB/s (  7.72 ns/op)
  Rand Read   :   1.47 GB/s (  5.05 ns/op)
-----------------------------------------------
CheckSum: 1fffffff0000000

```

**`Benchmark (multi-thread)`**

```bash
phys-gacha$ sudo ./phys_bench_multi 3235 4 4 3 4 5 6
Preparing indices for random access tests...
Running Seq Write  (4 threads)...  15.11 GB/s
Running Seq Read   (4 threads)...  43.41 GB/s
Running Rand Write (4 threads)...   1.15 GB/s
Running Rand Read  (4 threads)...   2.37 GB/s

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

