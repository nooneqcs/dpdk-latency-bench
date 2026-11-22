# perf 实测结果（2026-11-22）

环境见 bench_notes.md 环境记录节

## fuse
```
tsc = 3.00 GHz

只解析              :   1.42 ns
只 TX 软件路径      :   2.67 ns
两者相加            :   4.09 ns
融合实测            :   2.89 ns

融合 vs 相加: -1.20 ns
```

## line
```
link_up=1 speed=1000 duplex=1
第1轮 5.00s | 提交 7441810 / 未入环 279590574 | opackets 7440677 | 1488135 pps (线速 1488095 的 100.00%)
第2轮 5.00s | 提交 7441814 / 未入环 280252202 | opackets 7440679 | 1488136 pps (线速 1488095 的 100.00%)
第3轮 5.00s | 提交 7441814 / 未入环 280216842 | opackets 7440678 | 1488135 pps (线速 1488095 的 100.00%)
```

## parse90
```
verify: OK
scalar :    61.64 ns/tick   0.685 ns/field
avx2   :     9.71 ns/tick   0.108 ns/field
speedup: 6.35x   (sink=43445757440000000)
```

## parsetp
```
sizeof(Tick) = 408 字节

verify V1: au2612 SHFE 21:04:13 800.0  V[0]=1000
verify V2: au2612 SHFE 21:04:13 800.0  V[0]=1000
verify V3: au2612 SHFE 21:04:13 (数值已翻转，不做公式对照)

empty  :    1.34 ns
V1 string+逐字段 :  130.52 ns  净 129.18
V2 整块 memcpy   :    4.69 ns  净   3.35
V3 memcpy+大端   :   10.05 ns  净   8.71
```

## poll
```
空轮询 第1轮: 6.366 ns/次（总 381938268 cycles，sink=0）
空轮询 第2轮: 6.366 ns/次（总 381955586 cycles，sink=0）
空轮询 第3轮: 6.371 ns/次（总 382259641 cycles，sink=0）
```

## rdtsc
```
空内核 第1轮: 77.71 cycles / 25.902 ns
空内核 第2轮: 77.68 cycles / 25.895 ns
空内核 第3轮: 77.70 cycles / 25.899 ns
空内核 第4轮: 81.53 cycles / 27.177 ns
```

## tx
```
port 0 ready, tsc=3.00 GHz

link_get=0 link_up=1 speed=1000 duplex=1

      (样本 2000 次 x 1 包)  opackets=2000 oerrors=0
      p50=142.33 p90=202.33 p99=413.33 p999=3844.33 max=3844.33  (ns/包)
单包 :   163.44 ns/包
      (样本 2000 次 x 4 包)  opackets=10000 oerrors=0
      p50=51.75 p90=66.92 p99=111.75 p999=194.25 max=194.25  (ns/包)
4 包 :    55.64 ns/包
      (样本 1500 次 x 8 包)  opackets=22000 oerrors=0
      p50=35.88 p90=45.25 p99=79.92 p999=119.46 max=119.46  (ns/包)
8 包 :    37.87 ns/包
      (样本 1200 次 x 16 包)  opackets=41200 oerrors=0
      p50=27.04 p90=34.44 p99=49.10 p999=81.83 max=81.83  (ns/包)
16包 :    28.60 ns/包
      (样本 1000 次 x 32 包)  opackets=73200 oerrors=0
      p50=22.83 p90=27.26 p99=37.78 p999=105.94 max=105.94  (ns/包)
32包 :    23.74 ns/包
```
