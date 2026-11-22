// parse20_v6.c —— v4 + checksum SIMD 归约 + px 向量直落结构（无 vpextrd）
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <immintrin.h>

#define NFIELDS   20
#define STRIDE    96
#define NOFF      16
#define ITERS     2000000
#define ROUNDS    7

/* 新布局（同 v3）
   0 symbol 1 date 2 time 3 volume
   4..12 九个定点价(万分之一) 13 amount
   14..17 bid1v bid2v ask1v ask2v 18 num_trades 19 checksum */

typedef struct {                 /* 正好 64 字节，无 padding */
    int32_t  px[9];              /*  0..36  定点价 */
    uint32_t time;               /* 36..40  HHMMSSmmm */
    uint32_t volume;             /* 40..44  */
    uint32_t amount;             /* 44..48  */
    uint16_t symbol;             /* 48..50  */
    uint16_t date;               /* 50..52  年-2000(7) 月(4) 日(5) */
    uint16_t b1v, b2v, a1v, a2v; /* 52..60  */
    uint16_t num_trades;         /* 60..62  */
    uint8_t  pad[2];             /* 62..64  */
} Tick64;

static uint8_t buf[NOFF * STRIDE] __attribute__((aligned(64)));
static Tick64  rec __attribute__((aligned(64)));
static volatile uint64_t sink;

static inline __m256i bswap32(__m256i v)
{
    const __m256i m = _mm256_setr_epi8(
        3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12,
        3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12);
    return _mm256_shuffle_epi8(v, m);
}
static inline __m128i bswap128(__m128i v)
{
    const __m128i m = _mm_setr_epi8(3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12);
    return _mm_shuffle_epi8(v, m);
}

__attribute__((noinline))
static uint32_t kern_full(const uint8_t *restrict p, Tick64 *restrict r)
{
    uint32_t w[NFIELDS] __attribute__((aligned(32)));

    __m256i b0 = bswap32(_mm256_load_si256((const __m256i *)p));
    __m256i b1 = bswap32(_mm256_load_si256((const __m256i *)(p + 32)));
    __m128i b2 = bswap128(_mm_load_si128((const __m128i *)(p + 64)));

    /* checksum：向量归约替代 19 次串行 add（32 位 wrap 加法可交换，值等价） */
    __m256i s = _mm256_add_epi32(b0, b1);
    __m128i h = _mm_add_epi32(_mm256_castsi256_si128(s),
                              _mm256_extracti128_si256(s, 1));
    h = _mm_add_epi32(h, _mm_shuffle_epi32(h, _MM_SHUFFLE(1,0,3,2)));
    h = _mm_add_epi32(h, _mm_shuffle_epi32(h, _MM_SHUFFLE(2,3,0,1)));
    __m128i b2m = _mm_and_si128(b2, _mm_setr_epi32(-1, -1, -1, 0));
    b2m = _mm_add_epi32(b2m, _mm_shuffle_epi32(b2m, _MM_SHUFFLE(1,0,3,2)));
    b2m = _mm_add_epi32(b2m, _mm_shuffle_epi32(b2m, _MM_SHUFFLE(2,3,0,1)));
    uint32_t sum = (uint32_t)_mm_cvtsi128_si32(h)
                 + (uint32_t)_mm_cvtsi128_si32(b2m);

    /* 标量字段仍需 w[]，只落一次 */
    _mm256_store_si256((__m256i *)w,       b0);
    _mm256_store_si256((__m256i *)(w + 8), b1);
    _mm_store_si128((__m128i *)(w + 16),   b2);

    /* 9 个定点价：向量直落结构，省 3 次回读，不用 vpextrd */
    _mm_storeu_si128((__m128i *)r->px,
                     _mm256_extracti128_si256(b0, 1));      /* 字段 4..7  */
    _mm_storeu_si128((__m128i *)(r->px + 4),
                     _mm256_castsi256_si128(b1));           /* 字段 8..11 */
    _mm_storeu_si32((void *)(r->px + 8),
                    _mm256_extracti128_si256(b1, 1));       /* 字段 12    */

    int32_t dd = (int32_t)w[1];
    r->date = (uint16_t)(((dd / 10000 - 2000) << 9) |
                         (((dd / 100) % 100) << 5) | (dd % 100));
    r->time       = w[2];
    r->volume     = w[3];
    r->amount     = w[13];
    r->symbol     = (uint16_t)((w[0] ^ (w[0] >> 13)) & 0xFFFF);
    r->b1v        = (uint16_t)w[14];
    r->b2v        = (uint16_t)w[15];
    r->a1v        = (uint16_t)w[16];
    r->a2v        = (uint16_t)w[17];
    r->num_trades = (uint16_t)w[18];
    return sum;
}

__attribute__((noinline))
static uint32_t kern_empty(const uint8_t *restrict p, Tick64 *restrict r)
{
    r->volume = p[0];
    return 0;
}

static void build_msg(uint8_t *p, int tag)
{
    uint32_t v[NFIELDS];
    v[0]  = 0x36303030u; v[1] = 20261118u; v[2] = 93015123u; v[3] = 3500u;
    v[4]  = 123456u; v[5] = 120000u; v[6] = 125000u; v[7] = 118000u;
    v[8]  = 119000u; v[9] = 123400u; v[10] = 123300u; v[11] = 123500u;
    v[12] = 123600u; v[13] = 42000u;
    v[14] = 800u; v[15] = 600u; v[16] = 700u; v[17] = 900u; v[18] = 158u;
    for (int i = 0; i < NFIELDS - 1; ++i) v[i] += (uint32_t)tag;
    uint32_t s = 0;
    for (int i = 0; i < NFIELDS - 1; ++i) s += v[i];
    v[NFIELDS - 1] = s;
    for (int i = 0; i < NFIELDS; ++i) {
        p[i*4+0] = (uint8_t)(v[i] >> 24);
        p[i*4+1] = (uint8_t)(v[i] >> 16);
        p[i*4+2] = (uint8_t)(v[i] >> 8);
        p[i*4+3] = (uint8_t)v[i];
    }
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

#define TIME_BODY(K, NAME)                                                  \
__attribute__((noinline)) static double time_##NAME(void) {                 \
    uint64_t acc = 0;                                                       \
    double t0 = now_ns();                                                   \
    for (int i = 0; i < ITERS; ++i)                                         \
        acc += K(buf + ((i & (NOFF - 1)) * STRIDE), &rec);                  \
    double t1 = now_ns();                                                   \
    sink += acc;                                                            \
    return t1 - t0;                                                         \
}
TIME_BODY(kern_empty, empty)
TIME_BODY(kern_full,  full)

int main(void)
{
    for (int o = 0; o < NOFF; ++o) build_msg(buf + o * STRIDE, o);

    Tick64 r0;
    kern_full(buf, &r0);
    int y = 2000 + (r0.date >> 9);
    int m = (r0.date >> 5) & 15;
    int d = r0.date & 31;
    int ok = (r0.px[0] == 123456 && r0.px[8] == 123600 && r0.px[3] == 118000 &&
              r0.volume == 3500 && y == 2026 && m == 11 && d == 18 &&
              r0.time == 93015123u && r0.b1v == 800);
    printf("verify: %s | last=%.4f ask2=%.4f vol=%u %04d-%02d-%02d sizeof=%zu\n",
           ok ? "OK" : "FAIL", r0.px[0] / 1e4, r0.px[8] / 1e4, r0.volume,
           y, m, d, sizeof(Tick64));

    double e = 1e18, f = 1e18;
    for (int r = 0; r < ROUNDS; ++r) {
        double a = time_empty(), b = time_full();
        if (a < e) e = a;
        if (b < f) f = b;
    }
    double nf = (f - e) / ITERS;
    printf("empty : %7.2f ns/tick\n", e / ITERS);
    printf("full  : %7.2f ns/tick  净 %6.2f  |  %6.2f ns/field  |  %6.1f Mops\n",
           f / ITERS, nf, nf / NFIELDS, 1e3 / nf);
    return 0;
}

