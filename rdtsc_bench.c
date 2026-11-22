// rdtsc_bench.c —— 一对 rte_rdtsc_precise 夹空内核的开销
#include <stdio.h>
#include <stdint.h>
#include <rte_eal.h>
#include <rte_cycles.h>

#define NITER 20000000

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (rte_eal_init(argc, argv) < 0) return -1;

    uint64_t hz = rte_get_tsc_hz();

    for (int round = 0; round < 4; ++round) {
        uint64_t sum = 0;
        for (int i = 0; i < NITER; i++) {
            uint64_t t0 = rte_rdtsc_precise();
            __asm__ volatile("" ::: "memory");
            uint64_t t1 = rte_rdtsc_precise();
            sum += (t1 - t0);
        }
        double cyc = (double)sum / (double)NITER;
        printf("空内核 第%d轮: %.2f cycles / %.3f ns\n",
               round + 1, cyc, cyc * 1e9 / (double)hz);
    }
    return 0;
}

