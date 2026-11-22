// parse_ctp_v4.cpp —— 修正版：wire 为真实大端；V4 全部向量化
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <string>
#include <immintrin.h>

struct Tick {
    char    InstrumentID[81];
    char    ExchangeID[9];
    char    TradingDay[9];
    char    UpdateTime[9];
    char    ActionDay[9];
    int32_t UpdateMillisec;
    double  P[30];
    int32_t V[10];
};

static uint8_t wire[sizeof(Tick)] __attribute__((aligned(64)));
static Tick    rec;
static volatile uint64_t sink;

static inline __m256i bswap64(__m256i v)
{
    const __m256i m = _mm256_setr_epi8(
        7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8,
        7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8);
    return _mm256_shuffle_epi8(v, m);
}
static inline __m256i bswap32x8(__m256i v)
{
    const __m256i m = _mm256_setr_epi8(
        3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12,
        3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12);
    return _mm256_shuffle_epi8(v, m);
}
static inline __m128i bswap128(__m128i v)
{
    const __m128i m = _mm_setr_epi8(7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8);
    return _mm_shuffle_epi8(v, m);
}
static inline __m128i bswap32x4(__m128i v)
{
    const __m128i m = _mm_setr_epi8(3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12);
    return _mm_shuffle_epi8(v, m);
}

/* V2：整块 memcpy（不翻转，作“拷贝成本参照”，数值不对） */
__attribute__((noinline))
static void parse_v2(const uint8_t* w, Tick* t)
{
    memcpy(t, w, sizeof(Tick));
}

/* V3：memcpy + 翻转（双写） */
__attribute__((noinline))
static void parse_v3(const uint8_t* w, Tick* t)
{
    memcpy(t, w, sizeof(Tick));
    uint8_t* p = (uint8_t*)t->P;
    for (int i = 0; i < 7; ++i) {
        __m256i v = _mm256_loadu_si256((const __m256i*)(p + i * 32));
        _mm256_storeu_si256((__m256i*)(p + i * 32), bswap64(v));
    }
    __m128i lo = _mm_loadu_si128((const __m128i*)(p + 224));
    _mm_storeu_si128((__m128i*)(p + 224), bswap128(lo));
    for (int i = 0; i < 10; ++i) t->V[i] = __builtin_bswap32(t->V[i]);
}

/* V4：融合单遍，全部向量化 */
__attribute__((noinline))
static void parse_v4(const uint8_t* w, Tick* t)
{
    const size_t offP = offsetof(Tick, P);
    const size_t offV = offsetof(Tick, V);

    memcpy(t, w, offP);                                   /* char 头段 */

    t->UpdateMillisec = (int32_t)__builtin_bswap32(
        *(const uint32_t*)(w + offsetof(Tick, UpdateMillisec)));

    /* P[30]: 7×32B + 16B */
    for (int i = 0; i < 7; ++i) {
        __m256i v = _mm256_loadu_si256((const __m256i*)(w + offP + i * 32));
        _mm256_storeu_si256((__m256i*)((uint8_t*)t + offP + i * 32), bswap64(v));
    }
    {
        __m128i lo = _mm_loadu_si128((const __m128i*)(w + offP + 224));
        _mm_storeu_si128((__m128i*)((uint8_t*)t + offP + 224), bswap128(lo));
    }

    /* V[10]: 8 int 向量 + 2 int */
    {
        __m256i vv = _mm256_loadu_si256((const __m256i*)(w + offV));
        _mm256_storeu_si256((__m256i*)((uint8_t*)t + offV), bswap32x8(vv));
        __m128i vlo = _mm_loadl_epi64((const __m128i*)(w + offV + 32));
        _mm_storel_epi64((__m128i*)((uint8_t*)t + offV + 32), bswap32x4(vlo));
    }
}

__attribute__((noinline))
static void kern_empty(const uint8_t* w, Tick* t) { t->UpdateMillisec = w[0]; }

/* 构造“大端” wire：数值字段全部 bswap 后写入 */
static void build_wire(void)
{
    Tick t;
    memset(&t, 0, sizeof(t));
    strcpy(t.InstrumentID, "au2612");
    strcpy(t.ExchangeID,  "SHFE");
    strcpy(t.TradingDay,  "20261119");
    strcpy(t.UpdateTime,  "21:04:13");
    strcpy(t.ActionDay,   "20261118");
    t.UpdateMillisec = 500;
    for (int i = 0; i < 30; ++i) t.P[i] = 800.0 + i;
    for (int i = 0; i < 10; ++i) t.V[i] = 1000 + i;
    memcpy(wire, &t, sizeof(Tick));

    uint32_t ms; memcpy(&ms, wire + offsetof(Tick, UpdateMillisec), 4);
    ms = __builtin_bswap32(ms);
    memcpy(wire + offsetof(Tick, UpdateMillisec), &ms, 4);
    for (int i = 0; i < 30; ++i) {
        uint64_t v; memcpy(&v, wire + offsetof(Tick, P) + i*8, 8);
        v = __builtin_bswap64(v);
        memcpy(wire + offsetof(Tick, P) + i*8, &v, 8);
    }
    for (int i = 0; i < 10; ++i) {
        uint32_t v; memcpy(&v, wire + offsetof(Tick, V) + i*4, 4);
        v = __builtin_bswap32(v);
        memcpy(wire + offsetof(Tick, V) + i*4, &v, 4);
    }
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

#define ITERS   1000000
#define ROUNDS  7

#define TIME_BODY(K, NAME)                                         \
__attribute__((noinline)) static double time_##NAME(void) {        \
    uint64_t acc = 0;                                              \
    double t0 = now_ns();                                          \
    for (int i = 0; i < ITERS; ++i) {                              \
        K(wire, &rec);                                             \
        acc += (uint64_t)rec.UpdateMillisec + (uint64_t)rec.V[0];  \
    }                                                              \
    double t1 = now_ns();                                          \
    sink += acc;                                                   \
    return t1 - t0;                                                \
}
TIME_BODY(kern_empty, empty)
TIME_BODY(parse_v2,   v2)
TIME_BODY(parse_v3,   v3)
TIME_BODY(parse_v4,   v4)

int main(void)
{
    build_wire();
    printf("sizeof(Tick)=%zu  offP=%zu  offV=%zu\n\n",
           sizeof(Tick), offsetof(Tick,P), offsetof(Tick,V));

    Tick c, d;
    parse_v3(wire, &c);
    parse_v4(wire, &d);
    /* 期望值：ms=500, P[0]=800.0, P[29]=829.0, V[0]=1000, V[9]=1009 */
    printf("V3: %s %s ms=%d P0=%.1f P29=%.1f V0=%d V9=%d\n",
           c.InstrumentID, c.UpdateTime, c.UpdateMillisec, c.P[0], c.P[29], c.V[0], c.V[9]);
    printf("V4: %s %s ms=%d P0=%.1f P29=%.1f V0=%d V9=%d\n",
           d.InstrumentID, d.UpdateTime, d.UpdateMillisec, d.P[0], d.P[29], d.V[0], d.V[9]);
    int ok = (d.UpdateMillisec==500 && d.P[0]==800.0 && d.P[29]==829.0 &&
              d.V[0]==1000 && d.V[9]==1009 && !strcmp(d.InstrumentID,"au2612"));
    printf("V4 verify: %s\n\n", ok ? "OK" : "FAIL");

    double e = 1e18, v2 = 1e18, v3 = 1e18, v4 = 1e18;
    for (int r = 0; r < ROUNDS; ++r) {
        double x0 = time_empty(), x2 = time_v2(), x3 = time_v3(), x4 = time_v4();
        if (x0 < e) e = x0;
        if (x2 < v2) v2 = x2;
        if (x3 < v3) v3 = x3;
        if (x4 < v4) v4 = x4;
    }
    printf("empty : %7.2f ns\n", e / ITERS);
    printf("V2 memcpy(不翻) : %7.2f ns  净 %6.2f\n", v2 / ITERS, (v2 - e) / ITERS);
    printf("V3 memcpy+翻(双写): %7.2f ns  净 %6.2f\n", v3 / ITERS, (v3 - e) / ITERS);
    printf("V4 融合单遍(全向量化): %7.2f ns  净 %6.2f\n", v4 / ITERS, (v4 - e) / ITERS);
    return 0;
}

