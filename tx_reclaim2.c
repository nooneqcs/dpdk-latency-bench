// tx_reclaim2.c —— 主动回收优先：tx_done_cleanup → tx_burst
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>

#define POOL_NUM  32768
#define RING_SIZE 1024
#define PKT_LEN   64
#define BATCH     32
#define RUN_SEC   10

static struct rte_mempool *g_mp;

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

int main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0) { printf("EAL fail\n"); return -1; }

    g_mp = rte_pktmbuf_pool_create("mbuf_pool", POOL_NUM, 256, 0,
                                   RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!g_mp) { printf("mempool fail\n"); return -1; }
    if (port_init(0) != 0) return -1;

    rte_eth_dev_set_link_up(0);
    rte_delay_ms(3000);

    struct rte_eth_link link;
    rte_eth_link_get(0, &link);
    printf("link_up=%u speed=%u duplex=%u, tsc=%.2f GHz\n",
           link.link_status, link.link_speed, link.link_duplex,
           rte_get_tsc_hz() / 1e9);
    if (!link.link_status) { printf("链路没起来，中止\n"); return -1; }

    uint64_t hz = rte_get_tsc_hz();
    uint64_t t_end = rte_rdtsc() + hz * RUN_SEC;

    unsigned long long calls = 0, zero = 0, submitted = 0;
    unsigned long long cl = 0, freed = 0;
    int cl_err = 0;

    uint64_t t0 = rte_rdtsc();
    while (rte_rdtsc() < t_end) {
        struct rte_mbuf *mb[BATCH];
        int got = 0;
        for (int k = 0; k < BATCH; ++k) {
            mb[k] = rte_pktmbuf_alloc(g_mp);
            if (!mb[k]) break;
            rte_pktmbuf_append(mb[k], PKT_LEN);
            got++;
        }
        if (got == 0) { rte_delay_us(1); continue; }

        uint16_t off = 0;
        while (off < got) {
            int rf = rte_eth_tx_done_cleanup(0, 0, 0);   /* 先主动回收 */
            ++cl;
            if (rf > 0)      freed += (unsigned)rf;
            else if (rf < 0) cl_err = rf;

            uint16_t n = rte_eth_tx_burst(0, 0, &mb[off], (uint16_t)(got - off));
            ++calls;
            if (n == 0) ++zero;
            off += n;
        }
        submitted += got;
    }
    uint64_t elapsed = rte_rdtsc() - t0;

    struct rte_eth_stats st;
    rte_eth_stats_get(0, &st);

    double secs    = (double)elapsed / (double)hz;
    double per_pkt = (double)elapsed * 1e9 / (double)hz / (double)submitted;

    printf("\n=== 主动回收 + 发送，%d 秒 ===\n", RUN_SEC);
    printf("提交包数=%llu  opackets=%llu  oerrors=%llu\n",
           submitted, (unsigned long long)st.opackets,
           (unsigned long long)st.oerrors);
    printf("tx_done_cleanup 调用=%llu  共回收=%llu 包  返回码=%d(%s)\n",
           cl, freed, cl_err,
           cl_err == -95 ? "ENOTSUP" : (cl_err ? "错误" : "正常"));
    printf("tx_burst 调用=%llu  返回0=%llu  (%.1f%%)\n",
           calls, zero, calls ? 100.0 * (double)zero / (double)calls : 0.0);
    printf("每包实际=%.1f ns  (%.3f Mpps)\n", per_pkt, (double)submitted / secs / 1e6);
    printf("对比：纯重试版 706.5 ns / 1.415 Mpps\n");

    rte_eth_dev_stop(0);
    rte_eth_dev_close(0);
    return 0;
}

