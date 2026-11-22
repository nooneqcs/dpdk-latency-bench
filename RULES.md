# rq-bench 工作规则
> 最高约定，任何会话开工前先读本文件。最后更新 2026-11-23

## 1. 事实源
- ~/rq-bench 是唯一事实源；数值以 bench_notes.md、RESULTS-*.md 为准
- 口径变更时在 bench_notes.md 写“作废”，不删原文，注明取代关系

## 2. 落盘纪律
- 每轮结论：写文件 → git commit → git push
- 提交信息：<类型>: <内容>（feat/docs/fix/chore）
- 禁止“只在对话里给结论，不落盘”

## 3. 跟踪范围（16 个 C/C++）
- 今晚定案：vint_fast2.c、vint_mask10.c、vint_mask11.c、parse_ctp_seg.cpp、poll_bench2.c、parse20_v10.c
- 11-22 及之前（不动）：line_rate.c、rx_bench.c、tx_bench.c、fuse_bench.c、end2end.c、rdtsc_bench.c、poll_bench.c、parse20_simd.c、parse90_simd.c、parse_ctp_tick.cpp
- 本地保留不跟踪：所有实验变体（.gitignore 中显式列出）

## 4. 目录与命名
- 文档放仓库根目录：PROGRESS-YYYY-MM-DD.md、ARCHIVE-YYYY-MM-DD.md
- .gitignore 首行 /*docs 会忽略根目录 docs/doc 类目录
- 禁用 git add -A

## 5. 口径纪律
- 只报可复现实测值；无法溯源或合成负载的数字不对外
- 分段相加 ≠ 全链路，并列必须标注
- ticks = TSC 3.0GHz 折算；核心周期约等于 ticks x 1.17
- 性能数必须同时给出测量条件（缓冲、速率、起点终点）
- 禁语：批量比标量快 N 倍；空轮询当取包成本；link-down 的 tx 数当发送延迟

## 6. 未决
- 端到端 250 ns 的起点/终点
- parse20 10.35 出自哪个源文件
- SMDP2.0 项目：域清单进行中

