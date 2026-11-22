// fuse_bench.c —— 解析 + TX 软件路径：分开测 vs 融合
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <rte_eal.h>
#include <rte_cycles.h>

static volatile uint64_t sink;

#define POOL_N 8192
#define ITERS  2000000
#define ROUNDS 7
#define P_OFF  128
#define V_OFF  448

static uint8_t wire[512] __attribute__((aligned(64)));

struct Node { struct Node *next; };
static struct Node g_nodes[POOL_N] __attribute__((aligned(64)));
static struct Node *volatile g_free;

struct tx_desc16 { uint64_t buf_iova; uint32_t cmd_type_len; uint32_t olinfo_status; };
static volatile struct tx_desc16 g_ring[1024] __attribute__((aligned(64)));

static double tsc_to_ns(double c) { return c * 1e9 / rte_get_tsc_hz(); }

/* 解析 4 字段（大端→本机序） */
#define PARSE_4()                                                   \
    uint64_t b0, b5, b10; uint32_t v0;                              \
    memcpy(&b0,  wire + P_OFF + 0 * 8, 8);  b0  = __builtin_bswap64(b0);  \
    memcpy(&b5,  wire + P_OFF + 5 * 8, 8);  b5  = __builtin_bswap64(b5);  \
    memcpy(&b10, wire + P_OFF + 10 * 8, 8); b10 = __builtin_bswap64(b10); \
    memcpy(&v0,  wire + V_OFF, 4);          v0  = __builtin_bswap32(v0)

/* 1) 只解析 */
static double f_parse(void)
{
    uint64_t acc = 0;
    uint64_t t0 = rte_rdtsc_precise();
    for (int i = 0; i < ITERS; ++i) {
        __asm__ volatile("" ::: "memory");   /* 防提到循环外 */
        PARSE_4();
        acc += b0 + b5 + b10 + v0;
    }
    uint64_t t1 = rte_rdtsc_precise();
    sink += acc;
    return tsc_to_ns((double)(t1 - t0)) / ITERS;
}

/* 2) 只 TX 软件路径（取节点 + 填描述符 + 归还） */
static double f_txsw(void)
{
    uint32_t tail = 0;
    uint64_t acc = 0;
    uint64_t t0 = rte_rdtsc_precise();
    for (int i = 0; i < ITERS; ++i) {
        struct Node *n = g_free; g_free = n->next;

        volatile struct tx_desc16 *d = &g_ring[tail];
        d->buf_iova      = (uint64_t)(uintptr_t)n;
        d->cmd_type_len  = (uint32_t)(16u << 16) | 1u;
        d->olinfo_status = (uint32_t)(16u << 24);
        tail = (tail + 1) & 1023;

        n->next = g_free; g_free = n;
        acc += tail;
    }
    uint64_t t1 = rte_rdtsc_precise();
    sink += acc;
    return tsc_to_ns((double)(t1 - t0)) / ITERS;
}

/* 3) 融合：一条循环里全做 */
static double f_fused(void)
{
    uint32_t tail = 0;
    uint64_t acc = 0;
    uint64_t t0 = rte_rdtsc_precise();
    for (int i = 0; i < ITERS; ++i) {
        __asm__ volatile("" ::: "memory");
        PARSE_4();

        struct Node *n = g_free; g_free = n->next;

        volatile struct tx_desc16 *d = &g_ring[tail];
        d->buf_iova      = (uint64_t)(uintptr_t)n;
        d->cmd_type_len  = (uint32_t)(16u << 16) | 1u;
        d->olinfo_status = (uint32_t)(16u << 24);
        tail = (tail + 1) & 1023;

        n->next = g_free; g_free = n;

        acc += b0 + b5 + b10 + v0 + tail;
    }
    uint64_t t1 = rte_rdtsc_precise();
    sink += acc;
    return tsc_to_ns((double)(t1 - t0)) / ITERS;
}

static double best_of(double (*fn)(void))
{
    double best = 1e18;
    for (int r = 0; r < ROUNDS; ++r) {
        double v = fn();
        if (v < best) best = v;
    }
    return best;
}

int main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0) return -1;

    for (int i = 0; i < (int)sizeof(wire); ++i) wire[i] = (uint8_t)(i * 37 + 11);
    for (int i = 0; i < POOL_N; ++i) g_nodes[i].next = &g_nodes[i + 1];
    g_nodes[POOL_N - 1].next = NULL;
    g_free = &g_nodes[0];

    printf("tsc = %.2f GHz\n\n", rte_get_tsc_hz() / 1e9);

    double a = best_of(f_parse);
    double b = best_of(f_txsw);
    double c = best_of(f_fused);

    printf("只解析              : %6.2f ns\n", a);
    printf("只 TX 软件路径      : %6.2f ns\n", b);
    printf("两者相加            : %6.2f ns\n", a + b);
    printf("融合实测            : %6.2f ns\n", c);
    printf("\n融合 vs 相加: %+.2f ns\n", c - (a + b));
    return 0;
}

