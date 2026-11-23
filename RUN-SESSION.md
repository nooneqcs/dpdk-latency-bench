# run_all.sh fast 会话记录（2026-11-22 01:37）

```
Script started on 2026-11-22 01:37:33+08:00 [COMMAND="./run_all.sh fast" TERM="xterm-256color" TTY="/dev/pts/1" COLUMNS="100" LINES="25"]
=== rdtsc ===
EAL: Detected CPU lcores: 4
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
空内核 第1轮: 77.68 cycles / 25.892 ns
空内核 第2轮: 77.69 cycles / 25.896 ns
空内核 第3轮: 77.71 cycles / 25.904 ns
空内核 第4轮: 77.69 cycles / 25.895 ns

=== fuse ===
EAL: Detected CPU lcores: 4
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: Using IOMMU type 1 (Type 1)
tsc = 3.00 GHz

只解析              :   1.42 ns
只 TX 软件路径      :   2.67 ns
两者相加            :   4.09 ns
融合实测            :   2.89 ns

融合 vs 相加: -1.20 ns

=== parse90 ===
verify: OK
scalar :    61.65 ns/tick   0.685 ns/field
avx2   :     9.71 ns/tick   0.108 ns/field
speedup: 6.35x   (sink=43445757440000000)

=== parsetp ===
sizeof(Tick) = 408 字节

verify V1: au2612 SHFE 21:04:13 800.0  V[0]=1000
verify V2: au2612 SHFE 21:04:13 800.0  V[0]=1000
verify V3: au2612 SHFE 21:04:13 (数值已翻转，不做公式对照)

empty  :    1.34 ns
V1 string+逐字段 :  133.80 ns  净 132.46
V2 整块 memcpy   :    4.69 ns  净   3.35
V3 memcpy+大端   :   10.05 ns  净   8.71

=== poll ===
EAL: Detected CPU lcores: 4
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: Using IOMMU type 1 (Type 1)
空轮询 第1轮: 6.382 ns/次（总 382938065 cycles，sink=0）
空轮询 第2轮: 6.379 ns/次（总 382718026 cycles，sink=0）
空轮询 第3轮: 6.368 ns/次（总 382056290 cycles，sink=0）

=== tx ===
EAL: Detected CPU lcores: 4
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: Using IOMMU type 1 (Type 1)
port 0 ready, tsc=3.00 GHz

link_get=0 link_up=1 speed=1000 duplex=1

      (样本 2000 次 x 1 包)  opackets=2000 oerrors=0
      p50=143.33 p90=203.67 p99=424.33 p999=4592.67 max=4592.67  (ns/包)
单包 :   166.34 ns/包
      (样本 2000 次 x 4 包)  opackets=10000 oerrors=0
      p50=51.92 p90=68.92 p99=115.92 p999=166.33 max=166.33  (ns/包)
4 包 :    56.30 ns/包
      (样本 1500 次 x 8 包)  opackets=22000 oerrors=0
      p50=35.54 p90=44.17 p99=78.88 p999=105.29 max=105.29  (ns/包)
8 包 :    37.42 ns/包
      (样本 1200 次 x 16 包)  opackets=41200 oerrors=0
      p50=27.17 p90=34.90 p99=55.23 p999=72.38 max=72.38  (ns/包)
16包 :    28.83 ns/包
      (样本 1000 次 x 32 包)  opackets=73200 oerrors=0
      p50=22.66 p90=26.61 p99=37.97 p999=50.80 max=50.80  (ns/包)
32包 :    23.53 ns/包

=== line ===
EAL: Detected CPU lcores: 4
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: Using IOMMU type 1 (Type 1)
link_up=1 speed=1000 duplex=1
第1轮 5.00s | 提交 7441811 / 未入环 280809005 | opackets 7440679 | 1488136 pps (线速 1488095 的 100.00%)
第2轮 5.00s | 提交 7441814 / 未入环 280987178 | opackets 7440684 | 1488137 pps (线速 1488095 的 100.00%)
第3轮 5.00s | 提交 7387880 / 未入环 277641272 | opackets 7386742 | 1477348 pps (线速 1488095 的 99.28%)


========== 汇总 ==========
--- rdtsc ---
空内核 第1轮: 77.68 cycles / 25.892 ns
空内核 第2轮: 77.69 cycles / 25.896 ns
空内核 第3轮: 77.71 cycles / 25.904 ns
空内核 第4轮: 77.69 cycles / 25.895 ns
--- fuse ---
只 TX 软件路径      :   2.67 ns
两者相加            :   4.09 ns
融合实测            :   2.89 ns
融合 vs 相加: -1.20 ns
--- parse90 ---
scalar :    61.65 ns/tick   0.685 ns/field
avx2   :     9.71 ns/tick   0.108 ns/field
--- parsetp ---
empty  :    1.34 ns
V1 string+逐字段 :  133.80 ns  净 132.46
V2 整块 memcpy   :    4.69 ns  净   3.35
V3 memcpy+大端   :   10.05 ns  净   8.71
--- poll ---
空轮询 第1轮: 6.382 ns/次（总 382938065 cycles，sink=0）
空轮询 第2轮: 6.379 ns/次（总 382718026 cycles，sink=0）
空轮询 第3轮: 6.368 ns/次（总 382056290 cycles，sink=0）
--- tx ---
      p50=27.17 p90=34.90 p99=55.23 p999=72.38 max=72.38  (ns/包)
16包 :    28.83 ns/包
      p50=22.66 p90=26.61 p99=37.97 p999=50.80 max=50.80  (ns/包)
32包 :    23.53 ns/包
--- line ---
第1轮 5.00s | 提交 7441811 / 未入环 280809005 | opackets 7440679 | 1488136 pps (线速 1488095 的 100.00%)
第2轮 5.00s | 提交 7441814 / 未入环 280987178 | opackets 7440684 | 1488137 pps (线速 1488095 的 100.00%)
第3轮 5.00s | 提交 7387880 / 未入环 277641272 | opackets 7386742 | 1477348 pps (线速 1488095 的 99.28%)

完整日志: runall-20261122-013733/

Script done on 2026-11-22 01:38:28+08:00 [COMMAND_EXIT_CODE="0"]
```
