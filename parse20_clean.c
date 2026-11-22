// parse20_clean.c —— 由 parse20_v10.c 整理：修 TIME_BODY 调用，语义不变
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

typedef struct {                 /* 正好 64 字节 */
    int32_t  px[9];
    uint32_t time;
    uint32_t volume;
    uint32_t amount;
    uint16_t symbol;
    uint16_t date;
    uint16_t b1v, b2v, a1v, a2v;
    uint16_t num_trades;
    uint8_t  pad[2];
} Tick64;

static uint8_t  buf[NOFF * STRIDE] __attribute__((aligned(64)));
static Tick64   rec __attribute__((aligned(64)));
static volatile uint64_t sink;
static uint16_t mdlut[10000];

static inline __m256i bswap32(__m256i v){
    const __m256i m = _mm256_setr_epi8(
        3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12,
        3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12);
    return _mm256_shuffle_epi8(v, m);
}
static inline __m128i bswap128(__m128i v){
    const __m128i m = _mm_setr_epi8(3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12);
    return _mm_shuffle_epi8(v, m);
}

__attribute__((noinline)) static uint32_t kern_empty(const uint8_t *restrict p, Tick64 *restrict r){
    r->volume = p[0];
    return 0;
}

__attribute__((noinline)) static uint32_t kern_nofill(const uint8_t *restrict p, Tick64 *restrict r){
    (void)r;
    __m256i b0 = bswap32(_mm256_load_si256((const __m256i *)p));
    __m256i b1 = bswap32(_mm256_load_si256((const __m256i *)(p + 32)));
    __m128i b2 = bswap128(_mm_load_si128((const __m128i *)(p + 64)));
    __m256i s = _mm256_add_epi32(b0, b1);
    __m128i h = _mm_add_epi32(_mm256_castsi256_si128(s), _mm256_extracti128_si256(s, 1));
    h = _mm_add_epi32(h, b2);
    h = _mm_add_epi32(h, _mm_shuffle_epi32(h, _MM_SHUFFLE(1,0,3,2)));
    h = _mm_add_epi32(h, _mm_shuffle_epi32(h, _MM_SHUFFLE(2,3,0,1)));
    return (uint32_t)_mm_cvtsi128_si32(h);
}

__attribute__((noinline)) static uint32_t kern_nofill2(const uint8_t *restrict p, Tick64 *restrict r){
    (void)r;
    __m256i b0 = bswap32(_mm256_load_si256((const __m256i *)p));
    __m256i b1 = bswap32(_mm256_load_si256((const __m256i *)(p + 32)));
    __m128i b2 = bswap128(_mm_load_si128((const __m128i *)(p + 64)));
    uint32_t t[8] __attribute__((aligned(32)));
    _mm256_store_si256((__m256i *)t, _mm256_add_epi32(b0, b1));
    _mm_storeu_si128((__m128i *)(t + 4), b2);
    return t[0] + t[4];
}

__attribute__((noinline)) static uint32_t kern_nofill4(const uint8_t *restrict p){
    uint32_t acc = 0;
#pragma GCC unroll 4
    for (int k = 0; k < 4; ++k){
        const uint8_t *q = p + k * STRIDE;
        __m256i b0 = bswap32(_mm256_load_si256((const __m256i *)q));
        __m256i b1 = bswap32(_mm256_load_si256((const __m256i *)(q + 32)));
        __m128i b2 = bswap128(_mm_load_si128((const __m128i *)(q + 64)));
        uint32_t t[8] __attribute__((aligned(32)));
        _mm256_store_si256((__m256i *)t, _mm256_add_epi32(b0, b1));
        _mm_storeu_si128((__m128i *)(t + 4), b2);
        acc += t[0] + t[4];
    }
    return acc;
}

__attribute__((noinline)) static uint32_t kern_full(const uint8_t *restrict p, Tick64 *restrict r){
    uint32_t w[NFIELDS] __attribute__((aligned(32)));
    __m256i v0 = _mm256_load_si256((const __m256i *)p);
    __m256i v1 = _mm256_load_si256((const __m256i *)(p + 32));
    __m128i v2 = _mm_load_si128((const __m128i *)(p + 64));
    _mm256_store_si256((__m256i *)w,       bswap32(v0));
    _mm256_store_si256((__m256i *)(w + 8), bswap32(v1));
    _mm_store_si128((__m128i *)(w + 16),   bswap128(v2));

    uint32_t sum = 0;
    for (int i = 0; i < NFIELDS - 1; ++i) sum += w[i];

    __m128i q0 = _mm_loadu_si128((const __m128i *)(w + 4));
    __m128i q1 = _mm_loadu_si128((const __m128i *)(w + 8));
    _mm_storeu_si128((__m128i *)(r->px),     q0);
    _mm_storeu_si128((__m128i *)(r->px + 4), q1);
    r->px[8] = (int32_t)w[12];

    int32_t dd = (int32_t)w[1];
    r->date = (uint16_t)(((dd / 10000 - 2000) << 9) | mdlut[dd % 10000]);

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

static void build_msg(uint8_t *p, int tag){
    uint32_t v[NFIELDS];
    v[0]=0x36303030u; v[1]=20261118u; v[2]=93015123u; v[3]=3500u;
    v[4]=123456u; v[5]=120000u; v[6]=125000u; v[7]=118000u;
    v[8]=119000u; v[9]=123400u; v[10]=123300u; v[11]=123500u;
    v[12]=123600u; v[13]=42000u;
    v[14]=800u; v[15]=600u; v[16]=700u; v[17]=900u; v[18]=158u;
    for (int i = 0; i < NFIELDS - 1; ++i) v[i] += (uint32_t)tag;
    uint32_t s = 0;
    for (int i = 0; i < NFIELDS - 1; ++i) s += v[i];
    v[NFIELDS - 1] = s;
    for (int i = 0; i < NFIELDS; ++i){
        p[i*4+0] = (uint8_t)(v[i] >> 24);
        p[i*4+1] = (uint8_t)(v[i] >> 16);
        p[i*4+2] = (uint8_t)(v[i] >> 8);
        p[i*4+3] = (uint8_t)v[i];
    }
}

static double now_ns(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

#define TIME_BODY(K, NAME)                                              \
__attribute__((noinline)) static double time_##NAME(void) {             \
    uint64_t acc = 0;                                                   \
    double t0 = now_ns();                                               \
    for (int i = 0; i < ITERS; ++i)                                     \
        acc += K(buf + ((i & (NOFF - 1)) * STRIDE), &rec);              \
    double t1 = now_ns();                                               \
    sink += acc;                                                        \
    return t1 - t0;                                                     \
}
TIME_BODY(kern_empty,   empty)
TIME_BODY(kern_nofill,  nofill)
TIME_BODY(kern_nofill2, nofill2)
TIME_BODY(kern_full,    full)

__attribute__((noinline)) static double time_nofill4(void){
    uint64_t acc = 0;
    double t0 = now_ns();
    for (int i = 0; i < ITERS / 4; ++i)
        acc += kern_nofill4(buf + ((i & 3) * 4 * STRIDE));
    double t1 = now_ns();
    sink += acc;
    return t1 - t0;
}

int main(void){
    for (int x = 0; x < 10000; ++x)
        mdlut[x] = (uint16_t)(((x / 100) << 5) | (x % 100));
    for (int o = 0; o < NOFF; ++o) build_msg(buf + o * STRIDE, o);

    Tick64 r0;
    kern_full(buf, &r0);
    int y = 2000 + (r0.date >> 9);
    int m = (r0.date >> 5) & 15;
    int d = r0.date & 31;
    int ok = (r0.px[0]==123456 && r0.px[8]==123600 && r0.px[3]==118000 &&
              r0.volume==3500 && y==2026 && m==11 && d==18 &&
              r0.time==93015123u && r0.b1v==800);
    printf("verify: %s | %04d-%02d-%02d sizeof=%zu\n",
           ok ? "OK" : "FAIL", y, m, d, sizeof(Tick64));

    double e=1e18,f1=1e18,f2=1e18,fu=1e18,l4=1e18;
    for (int r = 0; r < ROUNDS; ++r){
        double a=time_empty(), b=time_nofill(), c=time_nofill2(), x=time_full(), w4=time_nofill4();
        if (a<e) e=a; if (b<f1) f1=b; if (c<f2) f2=c; if (x<fu) fu=x; if (w4<l4) l4=w4;
    }
    double ne=(f1-e)/ITERS, n2=(f2-e)/ITERS, nu=(fu-e)/ITERS, p4=l4/ITERS;

    printf("empty   : %7.2f  ns/tick\n", e/ITERS);
    printf("nofill  : %7.2f  ns/tick  净 %5.2f | %6.3f ns/field | %6.1f Mops\n", f1/ITERS, ne, ne/NFIELDS, 1e3/ne);
    printf("nofill2 : %7.2f  ns/tick  净 %5.2f | %6.3f ns/field | %6.1f Mops\n", f2/ITERS, n2, n2/NFIELDS, 1e3/n2);
    printf("nofill4 : %7.2f  ns/条   批量4条不扣empty | %6.4f ns/field | %6.1f Mops\n", p4, p4/NFIELDS, 1e3/p4);
    printf("full    : %7.2f  ns/tick  净 %5.2f | %6.3f ns/field | %6.1f Mops\n", fu/ITERS, nu, nu/NFIELDS, 1e3/nu);
    return 0;
}

