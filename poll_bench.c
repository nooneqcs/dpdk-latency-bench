// poll_bench.c —— 空轮询（rx_burst 返回 0）的每次调用成本
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
#define NITER  20000000

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

    struct rte_mbuf *mb[BURST];
    uint64_t hz = rte_get_tsc_hz();

    /* 预热 */
    volatile uint16_t sink = 0;
    for (int i = 0; i < 1000000; i++) sink += rte_eth_rx_burst(0, 0, mb, BURST);

    for (int round = 0; round < 3; ++round) {
        uint64_t t0 = rte_rdtsc_precise();
        for (int i = 0; i < NITER; i++) {
            sink += rte_eth_rx_burst(0, 0, mb, BURST);
        }
        uint64_t t1 = rte_rdtsc_precise();
        double ns = (double)(t1 - t0) * 1e9 / (double)hz / (double)NITER;
        printf("空轮询 第%d轮: %.3f ns/次（总 %llu cycles，sink=%u）\n",
               round + 1, ns, (unsigned long long)(t1 - t0), (unsigned)sink);
    }
    return 0;
}

