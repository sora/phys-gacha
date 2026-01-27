#!/bin/bash

# --- 設定 ---
NODE=$1
PAGES=4
MAX_ATTEMPTS=10
TOOL="./phys_try"

# --- チェック ---
if [ -z "$NODE" ]; then
    echo "Usage: sudo $0 <numa_node_id>"
    echo "Example: sudo $0 0"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo)."
    exit 1
fi

# --- 1GB巨大ページの予約 ---
HUGE_SYSFS="/sys/devices/system/node/node${NODE}/hugepages/hugepages-1048576kB/nr_hugepages"
if [ ! -f "$HUGE_SYSFS" ]; then
    echo "Error: 1GB Hugepages not supported on Node $NODE."
    exit 1
fi

echo "Reserving $PAGES x 1GB hugepages on Node $NODE..."
echo $PAGES > "$HUGE_SYSFS"
sleep 1

# --- ガチャ・ループ ---
for i in $(seq 1 $MAX_ATTEMPTS); do
    echo ""
    echo "========================================"
    echo "   Phys-Gacha SSR Attempt $i / $MAX_ATTEMPTS"
    echo "========================================"

    # メモリの揉みほぐし
    sync
    echo 3 > /proc/sys/vm/drop_caches
    echo 1 > /proc/sys/vm/compact_memory
    sleep 1

    # 実行
    $TOOL "$NODE" "$PAGES"
    RESULT=$?

    if [ $RESULT -eq 0 ]; then
        exit 0
    fi

    echo "Attempt $i failed. Retrying..."
done

echo "Error: Could not secure contiguous memory after $MAX_ATTEMPTS attempts."
exit 1

