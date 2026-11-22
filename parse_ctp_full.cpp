// parse_ctp_full.cpp —— 修正版：宏展开，常量尺寸，无 memcpy 调用
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

#define CHR(f)  memcpy((char*)t + offsetof(Fld,f), w + offsetof(Fld,f), \
                       sizeof(((Fld*)0)->f))
#define DBL(f)  do { uint64_t v; __builtin_memcpy(&v, w + offsetof(Fld,f), 8); \
                     v = __builtin_bswap64(v); \
                     __builtin_memcpy((char*)t + offsetof(Fld,f), &v, 8); } while(0)
#define INT(f)  do { uint32_t v; __builtin_memcpy(&v, w + offsetof(Fld,f), 4); \
                     v = __builtin_bswap32(v); \
                     __builtin_memcpy((char*)t + offsetof(Fld,f), &v, 4); } while(0)

__attribute__((noinline))
static void parse_v4(const uint8_t* w, Fld* t)
{
    CHR(TradingDay); CHR(reserve1); CHR(ExchangeID); CHR(reserve2);
    DBL(LastPrice); DBL(PreSettlementPrice); DBL(PreClosePrice); DBL(PreOpenInterest);
    DBL(OpenPrice); DBL(HighestPrice); DBL(LowestPrice);
    INT(Volume);
    DBL(Turnover); DBL(OpenInterest); DBL(ClosePrice); DBL(SettlementPrice);
    DBL(UpperLimitPrice); DBL(LowerLimitPrice); DBL(PreDelta); DBL(CurrDelta);
    CHR(UpdateTime); INT(UpdateMillisec);
    DBL(BidPrice1); INT(BidVolume1); DBL(AskPrice1); INT(AskVolume1);
    DBL(BidPrice2); INT(BidVolume2); DBL(AskPrice2); INT(AskVolume2);
    DBL(BidPrice3); INT(BidVolume3); DBL(AskPrice3); INT(AskVolume3);
    DBL(BidPrice4); INT(BidVolume4); DBL(AskPrice4); INT(AskVolume4);
    DBL(BidPrice5); INT(BidVolume5); DBL(AskPrice5); INT(AskVolume5);
    DBL(AveragePrice); CHR(ActionDay); CHR(InstrumentID); CHR(ExchangeInstID);
    DBL(BandingUpperPrice); DBL(BandingLowerPrice);
}

__attribute__((noinline))
static void kern_empty(const uint8_t* w, Fld* t) { t->UpdateMillisec = w[0]; }

static void build_wire(void)
{
    Fld t; memset(&t, 0, sizeof(t));
    strcpy(t.TradingDay, "20261119"); strcpy(t.ExchangeID, "SHFE");
    strcpy(t.UpdateTime, "21:04:13"); strcpy(t.ActionDay, "20261118");
    strcpy(t.InstrumentID, "au2612"); strcpy(t.ExchangeInstID, "au2612");
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

    const uint8_t* w = wire; uint8_t* t2 = wire;
    (void)t2;
    /* 数值字段翻大端 */
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

static double now_ns(void)
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}
#define ITERS   1000000
#define ROUNDS  7

#define TIME_BODY(K, NAME)                                         \
__attribute__((noinline)) static double time_##NAME(void) {        \
    uint64_t acc = 0; double t0 = now_ns();                        \
    for (int i = 0; i < ITERS; ++i) {                              \
        K(wire, &rec);                                             \
        acc += (uint64_t)rec.UpdateMillisec + (uint64_t)rec.Volume; \
    }                                                              \
    double t1 = now_ns(); sink += acc; return t1 - t0;             \
}
TIME_BODY(kern_empty, empty)
TIME_BODY(parse_v4,   v4)

int main(void)
{
    build_wire();
    printf("sizeof=%zu off(LastPrice)=%zu off(BidPrice1)=%zu\n\n",
           sizeof(Fld), offsetof(Fld,LastPrice), offsetof(Fld,BidPrice1));
    Fld d; parse_v4(wire, &d);
    int ok = (d.LastPrice==836.76 && d.BidPrice1==836.7 && d.AskPrice5==837.2 &&
              d.Volume==12345 && d.BidVolume5==19 && d.UpdateMillisec==500 &&
              !strcmp(d.InstrumentID,"au2612"));
    printf("V4: %s last=%.2f bid1=%.2f ask5=%.2f vol=%d bv5=%d\n",
           d.InstrumentID, d.LastPrice, d.BidPrice1, d.AskPrice5, d.Volume, d.BidVolume5);
    printf("V4 verify: %s\n\n", ok?"OK":"FAIL");

    double e=1e18, v4=1e18;
    for (int r=0;r<ROUNDS;++r){ double x0=time_empty(), x4=time_v4();
        if(x0<e)e=x0; if(x4<v4)v4=x4; }
    printf("empty : %7.2f ns\n", e/ITERS);
    printf("V4 全字段 : %7.2f ns  净 %6.2f\n", v4/ITERS, (v4-e)/ITERS);
    return 0;
}

