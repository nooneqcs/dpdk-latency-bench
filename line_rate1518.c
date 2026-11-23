// line_rate.c —— 64 字节以太网帧线速测试
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>

#define POOL_N  65536
#define RING    1024
#define BATCH   32
#define PKT_LEN 1500          /* 46 + 14(以太头) + 4(FCS) = 64 字节帧 */

static struct rte_mempool *g_mp;

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

    struct rte_eth_link link;
    rte_eth_link_get(0, &link);
    printf("link_up=%u speed=%u duplex=%u\n", link.link_status, link.link_speed, link.link_duplex);
    if (link.link_status != 1) { printf("链路未通，退出\n"); return -1; }

    for (int round = 0; round < 3; ++round) {
        struct rte_eth_stats s0, s1;
        rte_eth_stats_get(0, &s0);

        uint64_t hz = rte_get_tsc_hz();
        uint64_t t0 = rte_rdtsc_precise();
        uint64_t dur = 5ULL * hz;
        uint64_t sub = 0, miss = 0;

        while (rte_rdtsc() - t0 < dur) {
            struct rte_mbuf *mb[BATCH];
            int n = 0;
            for (int k = 0; k < BATCH; k++) {
                mb[k] = rte_pktmbuf_alloc(g_mp);
                if (!mb[k]) break;
                rte_pktmbuf_append(mb[k], PKT_LEN);
                n++;
            }
            if (n == 0) continue;
            uint16_t sent = rte_eth_tx_burst(0, 0, mb, n);
            sub  += sent;
            miss += (n - sent);
            for (int k = sent; k < n; k++) rte_pktmbuf_free(mb[k]);
        }
        uint64_t t1 = rte_rdtsc_precise();
        rte_eth_stats_get(0, &s1);

        double sec = (double)(t1 - t0) / (double)hz;
        uint64_t outp = s1.opackets - s0.opackets;
        printf("第%d轮 %.2fs | 提交 %llu / 未入环 %llu | opackets %llu | %.0f pps (线速 1488095 的 %.2f%%)\n",
               round + 1, sec,
               (unsigned long long)sub, (unsigned long long)miss,
               (unsigned long long)outp,
               (double)outp / sec,
               (double)outp / sec / 1488095.0 * 100.0);
        rte_delay_ms(500);
    }
    return 0;
}

