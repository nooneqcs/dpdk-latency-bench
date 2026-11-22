// parse_ctp_tick.cpp —— CTP DepthMarketData 风格报文的三档解析
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <string>
#include <immintrin.h>

/* 报文 ≈ 结构体布局：5 个字符串 + 30 double + 10 int，约 408 字节 */
struct Tick {
    char    InstrumentID[81];        /* 合约代码，CTP 原样 81 字节 */
    char    ExchangeID[9];
    char    TradingDay[9];
    char    UpdateTime[9];
    char    ActionDay[9];
    int32_t UpdateMillisec;
    double  P[30];                   /* LastPrice/Open/High/Low/Bid1..5/Ask1..5... */
    int32_t V[10];                   /* Volume/OpenInterest/BidVol1..5/AskVol5... */
};

static uint8_t wire[sizeof(Tick)] __attribute__((aligned(64)));
static Tick    rec;
static volatile uint64_t sink;

/* ---------- V1 朴素：std::string 构造 + 逐字段拷贝 ---------- */
__attribute__((noinline))
static void parse_v1(const uint8_t* w, Tick* t)
{
    std::string s0((const char*)(w + offsetof(Tick, InstrumentID)));
    std::string s1((const char*)(w + offsetof(Tick, ExchangeID)));
    std::string s2((const char*)(w + offsetof(Tick, TradingDay)));
    std::string s3((const char*)(w + offsetof(Tick, UpdateTime)));
    std::string s4((const char*)(w + offsetof(Tick, ActionDay)));

    strcpy(t->InstrumentID, s0.c_str());
    strcpy(t->ExchangeID,  s1.c_str());
    strcpy(t->TradingDay,  s2.c_str());
    strcpy(t->UpdateTime,  s3.c_str());
    strcpy(t->ActionDay,   s4.c_str());

    t->UpdateMillisec = *(const int32_t*)(w + offsetof(Tick, UpdateMillisec));
    memcpy(t->P, w + offsetof(Tick, P), sizeof(t->P));
    memcpy(t->V, w + offsetof(Tick, V), sizeof(t->V));
}

/* ---------- V2 整块 memcpy：报文格式与内存布局一致时的下限 ---------- */
__attribute__((noinline))
static void parse_v2(const uint8_t* w, Tick* t)
{
    memcpy(t, w, sizeof(Tick));
}

/* ---------- V3 大端入站：memcpy + 数值 8/4 字节翻转 ---------- */
static inline __m256i bswap64(__m256i v)
{
    const __m256i m = _mm256_setr_epi8(
        7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8,
        7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8);
    return _mm256_shuffle_epi8(v, m);
}

__attribute__((noinline))
static void parse_v3(const uint8_t* w, Tick* t)
{
    memcpy(t, w, sizeof(Tick));

    /* 30 个 double = 240 字节 = 7×32B + 1×16B */
    uint8_t* p = (uint8_t*)t->P;
    for (int i = 0; i < 7; ++i) {
        __m256i v = _mm256_load_si256((const __m256i*)(p + i * 32));
        _mm256_store_si256((__m256i*)(p + i * 32), bswap64(v));
    }
    __m128i lo = _mm_load_si128((const __m128i*)(p + 224));
    _mm_store_si128((__m128i*)(p + 224), _mm_shuffle_epi8(lo,
        _mm_setr_epi8(7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8)));

    /* 10 个 int = 40 字节 */
    for (int i = 0; i < 10; ++i) t->V[i] = __builtin_bswap32(t->V[i]);
}

__attribute__((noinline))
static void kern_empty(const uint8_t* w, Tick* t)
{
    t->UpdateMillisec = w[0];
}

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
TIME_BODY(parse_v1,   v1)
TIME_BODY(parse_v2,   v2)
TIME_BODY(parse_v3,   v3)

int main(void)
{
    build_wire();
    printf("sizeof(Tick) = %zu 字节\n\n", sizeof(Tick));

    Tick a, b, c;
    parse_v1(wire, &a);
    parse_v2(wire, &b);
    parse_v3(wire, &c);
    printf("verify V1: %s %s %s %.1f  V[0]=%d\n", a.InstrumentID, a.ExchangeID,
           a.UpdateTime, a.P[0], a.V[0]);
    printf("verify V2: %s %s %s %.1f  V[0]=%d\n", b.InstrumentID, b.ExchangeID,
           b.UpdateTime, b.P[0], b.V[0]);
    printf("verify V3: %s %s %s (数值已翻转，不做公式对照)\n\n",
           c.InstrumentID, c.ExchangeID, c.UpdateTime);

    double e = 1e18, v1 = 1e18, v2 = 1e18, v3 = 1e18;
    for (int r = 0; r < ROUNDS; ++r) {
        double x0 = time_empty(), x1 = time_v1(), x2 = time_v2(), x3 = time_v3();
        if (x0 < e) e = x0;
        if (x1 < v1) v1 = x1;
        if (x2 < v2) v2 = x2;
        if (x3 < v3) v3 = x3;
    }
    printf("empty  : %7.2f ns\n", e / ITERS);
    printf("V1 string+逐字段 : %7.2f ns  净 %6.2f\n", v1 / ITERS, (v1 - e) / ITERS);
    printf("V2 整块 memcpy   : %7.2f ns  净 %6.2f\n", v2 / ITERS, (v2 - e) / ITERS);
    printf("V3 memcpy+大端   : %7.2f ns  净 %6.2f\n", v3 / ITERS, (v3 - e) / ITERS);
    return 0;
}

