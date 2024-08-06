#include <benchmark/benchmark.h>
#include <array>
#include <cstring>
 
constexpr int len = 6;

#define IS_CONTINUATION_BYTE(code) (((code) >> 6) == 0x02)

static inline int ob_valid_mbcharlen_utf8mb3(const unsigned char *s, const unsigned char *e)
{
  unsigned char c;
 
  c= s[0];
  if (c < 0x80)
    return 1;
  if (c < 0xc2)
    return 0;
  if (c < 0xe0)
  {
    if (s+2 > e) /* We need 2 characters */
      return -102;
    if (!(IS_CONTINUATION_BYTE(s[1])))
      return 0;
    return 2;
  }
  if (s+3 > e) /* We need 3 characters */
    return -103;
  if (!(IS_CONTINUATION_BYTE(s[1]) && IS_CONTINUATION_BYTE(s[2]) && (c >= 0xe1 || s[1] >= 0xa0)))
    return 0;
  return 3;
}

static inline int ob_valid_mbcharlen_utf8mb4(const unsigned char *s, const unsigned char *e)
{
  unsigned char c;
  if (s >= e)
    return -100;
  c= s[0];
  if (c < 0xf0)
    return ob_valid_mbcharlen_utf8mb3(s, e);
  if (c < 0xf5)
  {
    if (s + 4 > e) /* We need 4 characters */
      return -104;
    if (!(IS_CONTINUATION_BYTE(s[1]) &&
          IS_CONTINUATION_BYTE(s[2]) &&
          IS_CONTINUATION_BYTE(s[3]) &&
          (c >= 0xf1 || s[1] >= 0x90) &&
          (c <= 0xf3 || s[1] <= 0x8F)))
      return 0;
    return 4;
  }
  return 0;
}

static inline uint ob_ismbchar_utf8mb4(const char *b, const char *e)
{
  int res= ob_valid_mbcharlen_utf8mb4((const unsigned char*)b, (const unsigned char*)e);
  return (res > 1) ? res : 0;
}


static inline uint ob_mbcharlen_utf8mb4(uint c)
{
  if (c < 0x80)
    return 1;
  if (c < 0xc2)
    return 0; /* Illegal mb head */
  if (c < 0xe0)
    return 2;
  if (c < 0xf0)
    return 3;
  if (c < 0xf8)
    return 4;
  return 0; /* Illegal mb head */;
}

static inline int ascii_u64(const uint8_t *data, int len)
{
    uint8_t orall = 0;

    if (len >= 16) {

        uint64_t or1 = 0, or2 = 0;
        const uint8_t *data2 = data+8;

        do {
            or1 |= *(const uint64_t *)data;
            or2 |= *(const uint64_t *)data2;
            data += 16;
            data2 += 16;
            len -= 16;
        } while (len >= 16);

        /*
         * Idea from Benny Halevy <bhalevy@scylladb.com>
         * - 7-th bit set   ==> orall = !(non-zero) - 1 = 0 - 1 = 0xFF
         * - 7-th bit clear ==> orall = !0 - 1          = 1 - 1 = 0x00
         */
        orall = !((or1 | or2) & 0x8080808080808080ULL) - 1;
    }

    while (len--)
        orall |= *data++;

    return orall < 0x80;
}

static size_t ob_well_formed_len_utf8mb4(
                                         const char *b, const char *e,
                                         size_t pos, int *error)
{
  int len = (int)(e-b);
  if (len <= 0) {
    return 0;
  } else if (len>=15 && ascii_u64((const uint8_t *)b, len)) {
    return (size_t)len;
  }
  const char *b_start= b;
  *error= 0;
  while (pos)
  {
    int mb_len;
    if ((mb_len= ob_valid_mbcharlen_utf8mb4((unsigned char*) b, (unsigned char*) e)) <= 0)
    {
      *error= b < e ? 1 : 0;
      break;
    }
    b+= mb_len;
    pos--;
  }
  return (size_t) (b - b_start);
  
}

static size_t ob_well_formed_len_utf8mb4_org(
                                         const char *b, const char *e,
                                         size_t pos, int *error)
{
  
  const char *b_start= b;
  *error= 0;
  while (pos)
  {
    int mb_len;
    if ((mb_len= ob_valid_mbcharlen_utf8mb4((unsigned char*) b, (unsigned char*) e)) <= 0)
    {
      *error= b < e ? 1 : 0;
      break;
    }
    b+= mb_len;
    pos--;
  }
  return (size_t) (b - b_start);
  
}
char data[1000][1000] = {1};

static void prepare_utf8(int status) {
    for (int i = 0 ; i < 1000 ; i++) {
        if(i%status == 0){
            for (int j = 0 ; j < 1000 ; j++) {
                data[i][j] = 1;
            }
        }
        else {
            for (int j = 0 ; j < 1000 ; j+=4) {
                data[i][j] = 0xf0;
                data[i][j+1] = 0x9f;
                data[i][j+2] = 0x91;
                data[i][j+3] = 0x8b;

            }
        }
        
    }
}

static void prepare_data(benchmark::State& stat) {
    prepare_utf8(4);
    for (auto _: stat) {
        
    }
}



BENCHMARK(prepare_data);
static void bench_ascii_fast_utf8(benchmark::State& state)
{
    int error = 0;
    for (auto _: state) {
        for (int i = 0 ; i < 1000 ; i++){
            ob_well_formed_len_utf8mb4(data[i], data[i]+1000, 1000, &error);
        }
    }
}
BENCHMARK(bench_ascii_fast_utf8);
static void bench_ascii_fast_utf8_org(benchmark::State& state)
{
    int error = 0;
    for (auto _: state) {
        for(int i = 0 ; i < 1000 ; i++){
            ob_well_formed_len_utf8mb4_org(data[i], data[i]+1000, 1000, &error);
        }
        
    }
}
BENCHMARK(bench_ascii_fast_utf8_org);
BENCHMARK_MAIN();
