// rx_bench.c —— 只测 rte_eth_rx_burst 取包成本
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>

#define POOL_N 16384
#define RING   1024
#define BURST  32
#define MAX_S  2000000

static struct rte_mempool *g_mp;
static uint64_t g_samp[MAX_S];

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static int port_init(uint16_t port)
{
    struct rte_eth_conf cfg;
    memset(&cfg, 0, sizeof(cfg));

    int ret = rte_eth_dev_configure(port, 1, 1, &cfg);
    if (ret < 0) { printf("configure fail %d\n", ret); return ret; }

    ret = rte_eth_rx_queue_setup(port, 0, RING,
                                 rte_eth_dev_socket_id(port), NULL, g_mp);
    if (ret < 0) { printf("rxq fail %d\n", ret); return ret; }

    ret = rte_eth_tx_queue_setup(port, 0, RING,
                                 rte_eth_dev_socket_id(port), NULL);
    if (ret < 0) { printf("txq fail %d\n", ret); return ret; }

    ret = rte_eth_dev_start(port);
    if (ret < 0) { printf("start fail %d\n", ret); return ret; }
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (rte_eal_init(argc, argv) < 0) return -1;

    g_mp = rte_pktmbuf_pool_create("p", POOL_N, 256, 0,
                                   RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!g_mp) { printf("mempool fail\n"); return -1; }
    if (port_init(0) != 0) return -1;

    rte_eth_dev_set_link_up(0);
    rte_delay_ms(3000);
    printf("等待流量（最多 20 秒）...\n");

    uint64_t hz = rte_get_tsc_hz();
    double t_start = (double)rte_rdtsc_precise() / hz; double t_end = t_start + 120.0;
    int n = 0;
    uint64_t polls = 0, hits = 0;
    struct rte_eth_stats st0, st1;
    rte_eth_stats_get(0, &st0);

    while (n < MAX_S) {
        struct rte_mbuf *mb[BURST];
        uint64_t t0 = rte_rdtsc_precise();
        uint16_t nb = rte_eth_rx_burst(0, 0, mb, BURST);
        uint64_t t1 = rte_rdtsc_precise();
        polls++;
        if (nb == 0) {
            if ((polls & 0x3FF) == 0 && (double)rte_rdtsc_precise() / hz > t_end) break;
            continue;
        }
        hits++;
        g_samp[n++] = (t1 - t0) / nb;
        for (int i = 0; i < nb; ++i) rte_pktmbuf_free(mb[i]);
    }

    rte_eth_stats_get(0, &st1);
    printf("样本 %d，取包命中 %llu 次 / 轮询 %llu 次，ipackets %llu\n",
           n, (unsigned long long)hits, (unsigned long long)polls,
           (unsigned long long)(st1.ipackets - st0.ipackets));

    if (n == 0) { printf("没有收到包\n"); return 0; }

    qsort(g_samp, n, sizeof(uint64_t), cmp_u64);
    double k = 1e9 / (double)hz;
#define Q(p) (g_samp[((long)n * (p) / 100 >= n) ? n - 1 : (int)((long)n * (p) / 100)] * k)
    printf("p50=%.2f p90=%.2f p99=%.2f p999=%.2f max=%.2f  (ns/包)\n",
           Q(50), Q(90), Q(99), Q(999), g_samp[n - 1] * k);
#undef Q
    return 0;
}

