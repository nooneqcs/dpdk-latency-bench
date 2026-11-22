// tx_bench.c —— 纯 TX 提交成本：单包 vs 批量 + 分位
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>

#define POOL_NUM  32768
#define RING_SIZE 1024
#define PKT_LEN   64
#define MAX_S     4000

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

    ret = rte_eth_rx_queue_setup(port, 0, 128,
                                 rte_eth_dev_socket_id(port), NULL, g_mp);
    if (ret < 0) { printf("rxq fail %d\n", ret); return ret; }

    ret = rte_eth_tx_queue_setup(port, 0, RING_SIZE,
                                 rte_eth_dev_socket_id(port), NULL);
    if (ret < 0) { printf("txq fail %d\n", ret); return ret; }

    ret = rte_eth_dev_start(port);
    if (ret < 0) { printf("start fail %d\n", ret); return ret; }
    return 0;
}

static void drain_ring(void)
{
    for (int i = 0; i < 500; ++i) {
        int n = rte_eth_tx_done_cleanup(0, 0, 0);
        if (n > 0) continue;
        rte_delay_ms(1);
        if (rte_eth_tx_done_cleanup(0, 0, 0) == 0) break;
    }
}

static double bench_burst(int batch, int iters)
{
    rte_delay_ms(20);

    uint64_t hz = rte_get_tsc_hz();
    int done = 0;
    uint64_t cyc = 0;

    for (int i = 0; i < iters; ++i) {
        drain_ring();

        struct rte_mbuf *mb[64];
        uint64_t a = rte_rdtsc_precise();

        int got = 0;
        for (int k = 0; k < batch; ++k) {
            mb[k] = rte_pktmbuf_alloc(g_mp);
            if (!mb[k]) break;
            rte_pktmbuf_append(mb[k], PKT_LEN);
            got++;
        }

        uint16_t sent = 0;
        if (got == batch) {
            sent = rte_eth_tx_burst(0, 0, mb, (uint16_t)got);
            for (int k = sent; k < got; ++k) rte_pktmbuf_free(mb[k]);
        } else {
            for (int k = 0; k < got; ++k) rte_pktmbuf_free(mb[k]);
        }

        uint64_t b = rte_rdtsc_precise();

        if (got < batch || sent < got) break;
        if (done < MAX_S) g_samp[done] = b - a;
        cyc += (b - a);
        done++;
    }

    struct rte_eth_stats st;
    rte_eth_stats_get(0, &st);
    printf("      (样本 %d 次 x %d 包)  opackets=%llu oerrors=%llu\n",
           done, batch,
           (unsigned long long)st.opackets, (unsigned long long)st.oerrors);

    if (done == 0) return -1.0;

    int n = done < MAX_S ? done : MAX_S;
    qsort(g_samp, n, sizeof(uint64_t), cmp_u64);
    double k = 1e9 / (double)hz / (double)batch;   /* cycle -> 每包 ns */

#define Q(p) (g_samp[((long)n * (p) / 100 >= n) ? n - 1 : (int)((long)n * (p) / 100)] * k)
    printf("      p50=%.2f p90=%.2f p99=%.2f p999=%.2f max=%.2f  (ns/包)\n",
           Q(50), Q(90), Q(99), Q(999), g_samp[n - 1] * k);
#undef Q

    double ns = (double)cyc * 1e9 / (double)hz;
    return ns / ((double)done * (double)batch);
}

int main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0) { printf("EAL fail\n"); return -1; }

    g_mp = rte_pktmbuf_pool_create("mbuf_pool", POOL_NUM, 256, 0,
                                   RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!g_mp) { printf("mempool fail\n"); return -1; }

    if (port_init(0) != 0) return -1;
    printf("port 0 ready, tsc=%.2f GHz\n\n", rte_get_tsc_hz() / 1e9);

    rte_eth_dev_set_link_up(0);
    rte_delay_ms(3000);
    struct rte_eth_link link;
    int lr = rte_eth_link_get(0, &link);
    printf("link_get=%d link_up=%u speed=%u duplex=%u\n\n",
           lr, link.link_status, link.link_speed, link.link_duplex);

    printf("单包 : %8.2f ns/包\n", bench_burst(1,  2000));
    printf("4 包 : %8.2f ns/包\n", bench_burst(4,  2000));
    printf("8 包 : %8.2f ns/包\n", bench_burst(8,  1500));
    printf("16包 : %8.2f ns/包\n", bench_burst(16, 1200));
    printf("32包 : %8.2f ns/包\n", bench_burst(32, 1000));

    rte_eth_dev_stop(0);
    rte_eth_dev_close(0);
    return 0;
}

