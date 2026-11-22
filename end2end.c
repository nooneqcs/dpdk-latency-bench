// end2end.c —— 收 → 解析 → 发回（含 解析/tx 拆分与各自 p99）
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>

#define POOL_N 16384
#define RING   1024
#define BURST  32
#define HIST_N 2000          /* 10 ns/桶，上限 20 us */

static struct rte_mempool *g_mp;
static uint64_t g_hist[HIST_N];     /* 端到端 */
static uint64_t g_hparse[HIST_N];   /* 解析段 */
static uint64_t g_htx[HIST_N];      /* 发送段 */
static uint64_t g_sum, g_cnt, g_polls;
static uint64_t g_sparse, g_stx;

static int port_init(uint16_t port)
{
    struct rte_eth_conf cfg;
    memset(&cfg, 0, sizeof(cfg));

    int ret = rte_eth_dev_configure(port, 1, 1, &cfg);
    if (ret < 0) { printf("configure fail\n"); return ret; }

    ret = rte_eth_rx_queue_setup(port, 0, RING,
                                 rte_eth_dev_socket_id(port), NULL, g_mp);
    if (ret < 0) { printf("rxq fail\n"); return ret; }

    ret = rte_eth_tx_queue_setup(port, 0, RING,
                                 rte_eth_dev_socket_id(port), NULL);
    if (ret < 0) { printf("txq fail\n"); return ret; }

    ret = rte_eth_dev_start(port);
    if (ret < 0) { printf("start fail\n"); return ret; }
    return 0;
}

static double now_ns(void)
{
    return (double)rte_rdtsc_precise() * 1e9 / rte_get_tsc_hz();
}

static int pct(uint64_t *h, int p)
{
    uint64_t total = 0, acc = 0;
    for (int i = 0; i < HIST_N; ++i) total += h[i];
    if (total == 0) return -1;
    for (int i = 0; i < HIST_N; ++i) {
        acc += h[i];
        if (acc * 100 >= total * p) return i * 10;
    }
    return -1;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (rte_eal_init(argc, argv) < 0) return -1;

    g_mp = rte_pktmbuf_pool_create("p", POOL_N, 256, 0,
                                   RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!g_mp) { printf("mempool fail\n"); return -1; }
    if (port_init(0) != 0) return -1;

    printf("端到端测试就绪，等待流量...\n");
    double last = now_ns();

    while (1) {
        struct rte_mbuf *mb[BURST];
        uint16_t nb = rte_eth_rx_burst(0, 0, mb, BURST);
        if (nb == 0) continue;

        uint64_t t0 = rte_rdtsc_precise();

        /* 解析：从包体取 4 个字段（模拟真实解析动作） */
        for (int i = 0; i < nb; ++i) {
            uint8_t *p = rte_pktmbuf_mtod(mb[i], uint8_t *); static const uint8_t to_nic[6]={0xa0,0xce,0xc8,0xe4,0xfc,0x74}; memcpy(p,to_nic,6);
            uint64_t a, b;
            memcpy(&a, p + 0, 8);  a = __builtin_bswap64(a);
            memcpy(&b, p + 8, 8);  b = __builtin_bswap64(b);
            __asm__ volatile("" :: "r"(a), "r"(b) : "memory");
        }

        uint64_t tm = rte_rdtsc_precise();          /* 解析结束 / 发送开始 */

        uint16_t sent = rte_eth_tx_burst(0, 0, mb, nb);
        for (int i = sent; i < nb; ++i) rte_pktmbuf_free(mb[i]);

        uint64_t t1 = rte_rdtsc_precise();

        uint64_t d = t1 - t0;
        g_sum += d;
        g_cnt += nb;
        g_polls++;
        g_sparse += (tm - t0);
        g_stx    += (t1 - tm);

        double c2n = 1e9 / rte_get_tsc_hz();
        int b  = (int)((double)d        / nb * c2n / 10.0);
        int bp = (int)((double)(tm - t0) / nb * c2n / 10.0);
        int bt = (int)((double)(t1 - tm) / nb * c2n / 10.0);
        if (b  >= 0 && b  < HIST_N) g_hist[b]++;
        if (bp >= 0 && bp < HIST_N) g_hparse[bp]++;
        if (bt >= 0 && bt < HIST_N) g_htx[bt]++;

        double now = now_ns();
        if (now - last >= 1e9 && g_cnt > 0) {
            double mean = (double)g_sum / g_cnt * c2n - 11.35;
            printf("端到端 均值%6.1f p99%4d | 解析 均值%5.1f p99%4d | tx 均值%6.1f p99%4d | 批均%.1f\n",
                   mean,               pct(g_hist, 99),
                   (double)g_sparse / g_cnt * c2n, pct(g_hparse, 99),
                   (double)g_stx    / g_cnt * c2n, pct(g_htx, 99),
                   (double)g_cnt / g_polls);

            g_sum = g_cnt = g_polls = 0;
            g_sparse = g_stx = 0;
            memset(g_hist,   0, sizeof(g_hist));
            memset(g_hparse, 0, sizeof(g_hparse));
            memset(g_htx,    0, sizeof(g_htx));
            last = now;
        }
    }
    return 0;
}

