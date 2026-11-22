# rq-bench 索引

## 结论在哪
| 主题 | 文件 |
|---|---|
| VInt 解码（12 版迭代、最终数） | RESULTS-VInt.md + bench_notes.md |
| DPDK 路径分段（rx/parse/tx/线速） | bench_notes.md + RESULTS.md |
| SMDP2.0 调研（字段映射、材料清单） | PROGRESS-2026-11-23.md |
| 工作规则 | RULES.md |

## 源码对应
| 基准 | 定案源码 | 结果 |
|---|---|---|
| VInt 标量 | vint_fast2.c | 4.48 ticks/VInt |
| VInt 批量 | vint_mask10.c | 1.04 ticks（热 0.98） |
| VInt 缓冲验证 | vint_mask11.c | 32KB/256KB/16MB 三档 |
| CTP 584B 解析 | parse_ctp_seg.cpp | 8.05 ns 净 |
| 空轮询 | poll_bench2.c | 6.18 ns |
| 20 字段解析 | parse20_v10.c | 10.35 ns（待核版本） |
| DPDK 工具 | line_rate.c / rx_bench.c / tx_bench.c / fuse_bench.c / end2end.c / rdtsc_bench.c | 见 RESULTS.md |

## 已判负（本地保留，不在仓库）
vint_fast3..7.c、vint_mask2..9/12.c、vint_spec.c、vint_batch.c、vint_simd.c、v4lut.c、parse20_v6..v9.c、parse_ctp_v4/full/seg2..4/tick

## 未决
- 端到端 250 ns 的定义
- parse20 10.35 的源文件版本
- SMDP2.0 域清单（进行中）
