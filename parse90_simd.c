// parse90_simd.c —— 90 字段裸报文 AVX2(pshufb) 解析
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <immintrin.h>

#define MSG_BYTES 384
#define NVEC      (MSG_BYTES / 32)   /* 12 块 32B */
#define ITERS     2000000
#define NFIELDS   90
#define ROUNDS    5

static uint8_t  msg[MSG_BYTES]     __attribute__((aligned(64)));
static uint32_t out[MSG_BYTES / 4] __attribute__((aligned(64)));
static uint32_t ref[MSG_BYTES / 4] __attribute__((aligned(64)));

static inline __m256i bswap32(__m256i v)
{
    const __m256i m = _mm256_setr_epi8(
        3,2,1,0,  7,6,5,4,  11,10,9,8,  15,14,13,12,
        3,2,1,0,  7,6,5,4,  11,10,9,8,  15,14,13,12);
    return _mm256_shuffle_epi8(v, m);
}

__attribute__((noinline))
static void parse_avx2(const uint8_t *restrict p, uint32_t *restrict o)
{
    for (int i = 0; i < NVEC; ++i) {
        __m256i v = _mm256_load_si256((const __m256i *)(p + i * 32));
        _mm256_store_si256((__m256i *)(o + i * 8), bswap32(v));
    }
}

__attribute__((noinline))
static void parse_scalar(const uint8_t *p, uint32_t *o)
{
    for (int i = 0; i < MSG_BYTES / 4; ++i)
        o[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

int main(void)
{
    for (int i = 0; i < MSG_BYTES; ++i) msg[i] = (uint8_t)(i * 37 + 11);

    parse_scalar(msg, ref);
    parse_avx2(msg, out);
    printf("verify: %s\n", memcmp(ref, out, MSG_BYTES) == 0 ? "OK" : "MISMATCH");

    volatile uint64_t sink = 0;
    double best_s = 1e18, best_v = 1e18;

    for (int r = 0; r < ROUNDS; ++r) {
        uint64_t acc = 0;
        double t0 = now_ns();
        for (int i = 0; i < ITERS; ++i) {
            msg[0] ^= 1;                 /* 防整段计算被提出循环 */
            parse_scalar(msg, ref);
            acc += ref[i & 95];
        }
        double t1 = now_ns();
        if (t1 - t0 < best_s) best_s = t1 - t0;
        sink += acc;
    }
    for (int r = 0; r < ROUNDS; ++r) {
        uint64_t acc = 0;
        double t0 = now_ns();
        for (int i = 0; i < ITERS; ++i) {
            msg[0] ^= 1;
            parse_avx2(msg, out);
            acc += out[i & 95];
        }
        double t1 = now_ns();
        if (t1 - t0 < best_v) best_v = t1 - t0;
        sink += acc;
    }

    printf("scalar : %8.2f ns/tick  %6.3f ns/field\n", best_s / ITERS, best_s / ITERS / NFIELDS);
    printf("avx2   : %8.2f ns/tick  %6.3f ns/field\n", best_v / ITERS, best_v / ITERS / NFIELDS);
    printf("speedup: %.2fx   (sink=%llu)\n", best_s / best_v, (unsigned long long)sink);
    return 0;
}
