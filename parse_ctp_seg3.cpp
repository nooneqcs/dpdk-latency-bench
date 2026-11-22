// parse_ctp_seg3.cpp —— 极致版：全段按 cache line 切分，消除所有跨行 store
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <immintrin.h>
#include "ThostFtdcUserApiStruct.h"

typedef CThostFtdcDepthMarketDataField Fld;
static uint8_t wire[sizeof(Fld)] __attribute__((aligned(64)));
static Fld rec;
static volatile uint64_t sink;

static inline __m256i bs64x2(__m256i v) {
    const __m256i m = _mm256_setr_epi8(
        7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8,
        7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8);
    return _mm256_shuffle_epi8(v, m);
}
static inline __m128i bs64x1(__m128i v) {
    return _mm_shuffle_epi8(v, _mm_setr_epi8(7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8));
}

__attribute__((noinline))
static void parse_seg3(const uint8_t* w, Fld* t)
{
    uint8_t* tp = (uint8_t*)t;

    /* 段1: 0..80 */
    memcpy(tp+0,  w+0,  32);   /* 0..32  */
    memcpy(tp+32, w+32, 32);   /* 32..64 */
    memcpy(tp+64, w+64, 16);   /* 64..80 */

    /* 段2: 80..144  (32 + 16 + 16，全不跨行) */
    {
        __m256i v = _mm256_loadu_si256((const __m256i*)(w+80));
        _mm256_storeu_si256((__m256i*)(tp+80), bs64x2(v));      /* 80..112 */
        __m128i a = _mm_loadu_si128((const __m128i*)(w+112));
        _mm_storeu_si128((__m128i*)(tp+112), bs64x1(a));        /* 112..128 */
        __m128i b = _mm_loadu_si128((const __m128i*)(w+128));
        _mm_storeu_si128((__m128i*)(tp+128), _mm_shuffle_epi8(b,
            _mm_setr_epi8(7,6,5,4,3,2,1,0, 11,10,9,8, 15,14,13,12))); /* 128..144: Lowest+Vol */
    }

    /* 段4: 144..208  —— 32 + 16 + 16，消除跨 192 行 */
    {
        __m256i v0 = _mm256_loadu_si256((const __m256i*)(w+144));
        _mm256_storeu_si256((__m256i*)(tp+144), bs64x2(v0));    /* 144..176 */
        __m128i v1 = _mm_loadu_si128((const __m128i*)(w+176));
        _mm_storeu_si128((__m128i*)(tp+176), bs64x1(v1));       /* 176..192 */
        __m128i v2 = _mm_loadu_si128((const __m128i*)(w+192));
        _mm_storeu_si128((__m128i*)(tp+192), bs64x1(v2));       /* 192..208 */
    }

    /* 段5: 208..224 */
    {
        __m128i v = _mm_loadu_si128((const __m128i*)(w+208));
        _mm_storeu_si128((__m128i*)(tp+208), _mm_shuffle_epi8(v,
            _mm_setr_epi8(0,1,2,3,4,5,6,7, 8,9,10,11, 15,14,13,12)));
    }

    /* 段6: 224..384  5 档 × 32B，全在各自 cache line 内 */
    {
        const __m256i m = _mm256_setr_epi8(
            7,6,5,4,3,2,1,0, 11,10,9,8, 15,14,13,12,
            7,6,5,4,3,2,1,0, 11,10,9,8, 15,14,13,12);
        #define S6(i) do { \
            __m256i v = _mm256_loadu_si256((const __m256i*)(w+224+(i)*32)); \
            _mm256_storeu_si256((__m256i*)(tp+224+(i)*32), _mm256_shuffle_epi8(v, m)); \
        } while(0)
        S6(0); S6(1); S6(2); S6(3); S6(4);
    }

    /* 段7: 384..392 */
    {
        __m128i v = _mm_loadl_epi64((const __m128i*)(w+384));
        _mm_storel_epi64((__m128i*)(tp+384), bs64x1(v));
    }

    /* 段8: 392..563  —— 按 cache line 切 6 块，每块不跨行 */
    memcpy(tp+392, w+392, 32);   /* 392..424 行内 */
    memcpy(tp+424, w+424, 24);   /* 424..448 行内 */
    memcpy(tp+448, w+448, 32);   /* 448..480 行内 */
    memcpy(tp+480, w+480, 32);   /* 480..512 行内 */
    memcpy(tp+512, w+512, 32);   /* 512..544 行内 */
    memcpy(tp+544, w+544, 19);   /* 544..563 行内 */

    /* 段9: 568..584  —— 拆 2 个 8 字节，消除跨 576 行 */
    {
        __m128i v = _mm_loadu_si128((const __m128i*)(w+568));
        _mm_storel_epi64((__m128i*)(tp+568), bs64x1(v));                    /* 568..576 */
        _mm_storel_epi64((__m128i*)(tp+576), bs64x1(_mm_srli_si128(v, 8))); /* 576..584 */
    }
}

__attribute__((noinline))
static void kern_empty(const uint8_t* w, Fld* t) { t->UpdateMillisec = w[0]; }

static void build_wire(void)
{
    Fld t; memset(&t, 0, sizeof(t));
    strcpy(t.TradingDay,"20261119"); strcpy(t.ExchangeID,"SHFE");
    strcpy(t.UpdateTime,"21:04:13"); strcpy(t.ActionDay,"20261118");
    strcpy(t.InstrumentID,"au2612"); strcpy(t.ExchangeInstID,"au2612");
    t.LastPrice=836.76; t.PreSettlementPrice=830.0; t.PreClosePrice=831.0;
    t.PreOpenInterest=10000; t.OpenPrice=835.0; t.HighestPrice=840.0; t.LowestPrice=833.0;
    t.Volume=12345; t.Turnover=5.0e9; t.OpenInterest=10050;
    t.ClosePrice=836.8; t.SettlementPrice=830.0; t.UpperLimitPrice=850.0; t.LowerLimitPrice=820.0;
    t.PreDelta=0.1; t.CurrDelta=0.2; t.UpdateMillisec=500;
    t.BidPrice1=836.7; t.BidVolume1=3;  t.AskPrice1=836.8; t.AskVolume1=5;
    t.BidPrice2=836.6; t.BidVolume2=7;  t.AskPrice2=836.9; t.AskVolume2=9;
    t.BidPrice3=836.5; t.BidVolume3=11; t.AskPrice3=837.0; t.AskVolume3=13;
    t.BidPrice4=836.4; t.BidVolume4=15; t.AskPrice4=837.1; t.AskVolume4=17;
    t.BidPrice5=836.3; t.BidVolume5=19; t.AskPrice5=837.2; t.AskVolume5=21;
    t.AveragePrice=836.6; t.BandingUpperPrice=900.0; t.BandingLowerPrice=760.0;
    memcpy(wire, &t, sizeof(Fld));

    #define DBL2(f) do { uint64_t v; __builtin_memcpy(&v, wire+offsetof(Fld,f), 8); \
                         v=__builtin_bswap64(v); __builtin_memcpy(wire+offsetof(Fld,f), &v, 8); } while(0)
    #define INT2(f) do { uint32_t v; __builtin_memcpy(&v, wire+offsetof(Fld,f), 4); \
                         v=__builtin_bswap32(v); __builtin_memcpy(wire+offsetof(Fld,f), &v, 4); } while(0)
    DBL2(LastPrice); DBL2(PreSettlementPrice); DBL2(PreClosePrice); DBL2(PreOpenInterest);
    DBL2(OpenPrice); DBL2(HighestPrice); DBL2(LowestPrice); INT2(Volume);
    DBL2(Turnover); DBL2(OpenInterest); DBL2(ClosePrice); DBL2(SettlementPrice);
    DBL2(UpperLimitPrice); DBL2(LowerLimitPrice); DBL2(PreDelta); DBL2(CurrDelta);
    INT2(UpdateMillisec);
    DBL2(BidPrice1); INT2(BidVolume1); DBL2(AskPrice1); INT2(AskVolume1);
    DBL2(BidPrice2); INT2(BidVolume2); DBL2(AskPrice2); INT2(AskVolume2);
    DBL2(BidPrice3); INT2(BidVolume3); DBL2(AskPrice3); INT2(AskVolume3);
    DBL2(BidPrice4); INT2(BidVolume4); DBL2(AskPrice4); INT2(AskVolume4);
    DBL2(BidPrice5); INT2(BidVolume5); DBL2(AskPrice5); INT2(AskVolume5);
    DBL2(AveragePrice); DBL2(BandingUpperPrice); DBL2(BandingLowerPrice);
}

static double now_ns(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return ts.tv_sec*1e9+ts.tv_nsec; }
#define ITERS 1000000
#define ROUNDS 7
#define TIME_BODY(K,NAME) \
__attribute__((noinline)) static double time_##NAME(void){ \
    uint64_t acc=0; double t0=now_ns(); \
    for(int i=0;i<ITERS;++i){ K(wire,&rec); \
        acc += (uint64_t)rec.UpdateMillisec + (uint64_t)rec.Volume; } \
    double t1=now_ns(); sink+=acc; return t1-t0; }
TIME_BODY(kern_empty, empty)
TIME_BODY(parse_seg3, seg3)

int main(void)
{
    build_wire();
    printf("sizeof=%zu\n\n", sizeof(Fld));
    Fld d; parse_seg3(wire, &d);
    int ok = (d.LastPrice==836.76 && d.PreSettlementPrice==830.0 &&
              d.OpenPrice==835.0 && d.HighestPrice==840.0 && d.Volume==12345 &&
              d.BidPrice1==836.7 && d.BidVolume1==3 && d.AskPrice1==836.8 && d.AskVolume1==5 &&
              d.BidPrice5==836.3 && d.AskVolume5==21 &&
              d.AveragePrice==836.6 && d.BandingUpperPrice==900.0 && d.BandingLowerPrice==760.0 &&
              d.UpdateMillisec==500 && !strcmp(d.InstrumentID,"au2612"));
    printf("check: last=%.2f vol=%d bid1=%.2f/%.0f ask5=%.2f/%.0f avg=%.2f bandU=%.0f\n",
           d.LastPrice, d.Volume, d.BidPrice1,(double)d.BidVolume1,
           d.AskPrice5,(double)d.AskVolume5, d.AveragePrice, d.BandingUpperPrice);
    printf("verify: %s\n\n", ok?"OK":"FAIL");

    double e=1e18, v=1e18;
    for(int r=0;r<ROUNDS;++r){ double x0=time_empty(), x1=time_seg3();
        if(x0<e)e=x0; if(x1<v)v=x1; }
    printf("empty : %7.2f ns\n", e/ITERS);
    printf("seg3  : %7.2f ns  净 %6.2f\n", v/ITERS, (v-e)/ITERS);
    return 0;
}

