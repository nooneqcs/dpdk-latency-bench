#!/bin/bash
# run_all.sh —— perf 全套一键复现
# 用法: ./run_all.sh          (全部，含需要流量的两项)
#       ./run_all.sh fast     (只跑不需要外部流量的)
set -u
cd "$(dirname "$0")"

MODE=${1:-full}
OUT="runall-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"
SUDO="sudo"

clear_lock() { $SUDO rm -rf /var/run/dpdk/rte; }

runit() {
    name=$1; shift
    echo "=== $name ==="
    clear_lock
    "$@" 2>&1 | tee "$OUT/$name.log"
    echo
}

# ---------- 1. 纯 CPU，不需要网卡 ----------
runit rdtsc   $SUDO ./rdtsc_bench --no-huge --no-pci -l 0
runit fuse    $SUDO ./fuse_bench
runit parse90 ./parse90_simd
runit parsetp ./parse_ctp_tick

# ---------- 2. 需要网卡，不需要对端流量 ----------
runit poll    $SUDO ./poll_bench
runit tx      $SUDO ./tx_bench
runit line    $SUDO ./line_rate

# ---------- 3. 需要笔记本发包 ----------
if [ "$MODE" = "full" ]; then
    echo ">>> 请在笔记本启动高速广播发包（Python 那条不带 sleep 的）"
    read -r -p "    发包起来后按回车继续..." _
    runit rx $SUDO ./rx_bench

    echo ">>> 保持笔记本发包，按回车后跑 end2end 30 秒"
    read -r -p "    按回车..." _
    clear_lock
    echo "=== end2end (30s) ==="
    timeout 30 $SUDO ./end2end 2>&1 | tee "$OUT/end2end.log"
fi

# ---------- 汇总关键行 ----------
echo
echo "========== 汇总 =========="
for f in rdtsc fuse parse90 parsetp poll tx line rx; do
    [ -f "$OUT/$f.log" ] && { echo "--- $f ---"; grep -E "ns|p50|pps" "$OUT/$f.log" | tail -4; }
done
if [ -f "$OUT/end2end.log" ]; then
    echo "--- end2end (中位) ---"
    grep "均值" "$OUT/end2end.log" | tail -20 | sort -t'均' -k2 | sed -n '10p'
fi
echo
echo "完整日志: $OUT/"

