// parse20_simd.c —— 20 字段 × 4B 大端（80B 裸报文）
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <immintrin.h>

#define MSG_BYTES 80
#define NFIELDS   20
#define ITERS     5000000
#define ROUNDS    5

static uint8_t  msg[MSG_BYTES] __attribute__((aligned(64)));
static uint32_t out[NFIELDS]   __attribute__((aligned(64)));
static uint32_t ref[NFIELDS]   __attribute__((aligned(64)));

static inline __m256i bswap32(__m256i v)
{
    const __m256i m = _mm256_setr_epi8(
        3,2,1,0,  7,6,5,4,  11,10,9,8,  15,14,13,12,
        3,2,1,0,  7,6,5,4,  11,10,9,8,  15,14,13,12);
    return _mm256_shuffle_epi8(v, m);
}

static inline __m128i bswap32_128(__m128i v)
{
    const __m128i m = _mm_setr_epi8(
        3,2,1,0,  7,6,5,4,  11,10,9,8,  15,14,13,12);
    return _mm_shuffle_epi8(v, m);
}

__attribute__((noinline))
static void parse_avx2(const uint8_t *restrict p, uint32_t *restrict o)
{
    __m256i v0 = _mm256_load_si256((const __m256i *)(p));
    __m256i v1 = _mm256_load_si256((const __m256i *)(p + 32));
    _mm256_store_si256((__m256i *)(o),     bswap32(v0));
    _mm256_store_si256((__m256i *)(o + 8), bswap32(v1));
    __m128i v2 = _mm_load_si128((const __m128i *)(p + 64));
    _mm_store_si128((__m128i *)(o + 16), bswap32_128(v2));
}

__attribute__((noinline))
static void parse_scalar(const uint8_t *p, uint32_t *o)
{
    for (int i = 0; i < NFIELDS; ++i)
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
        for (int i = 0; i < ITERS; ++i) { msg[0] ^= 1; parse_scalar(msg, ref); acc += ref[i & 15]; }
        double t1 = now_ns();
        if (t1 - t0 < best_s) best_s = t1 - t0;
        sink += acc;
    }
    for (int r = 0; r < ROUNDS; ++r) {
        uint64_t acc = 0;
        double t0 = now_ns();
        for (int i = 0; i < ITERS; ++i) { msg[0] ^= 1; parse_avx2(msg, out); acc += out[i & 15]; }
        double t1 = now_ns();
        if (t1 - t0 < best_v) best_v = t1 - t0;
        sink += acc;
    }

    printf("scalar : %7.2f ns/tick  %6.3f ns/field  %6.1f Mops\n",
           best_s / ITERS, best_s / ITERS / NFIELDS, 1e3 / (best_s / ITERS));
    printf("avx2   : %7.2f ns/tick  %6.3f ns/field  %6.1f Mops\n",
           best_v / ITERS, best_v / ITERS / NFIELDS, 1e3 / (best_v / ITERS));
    printf("speedup: %.2fx   (sink=%llu)\n", best_s / best_v, (unsigned long long)sink);
    return 0;
}
