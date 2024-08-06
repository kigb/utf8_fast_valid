#include <array>
#include <cstring>
#include <stdlib.h>
#include <iostream>

unsigned char *utf8_check(unsigned char *s)
{
  while (*s) {
    if (*s < 0x80)
      /* 0xxxxxxx */
      s++;
    else if ((s[0] & 0xe0) == 0xc0) {
      /* 110XXXXx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
	  (s[0] & 0xfe) == 0xc0)                        /* overlong? */
	return s;
      else
	s += 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
      /* 1110XXXX 10Xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
	  (s[2] & 0xc0) != 0x80 ||
	  (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||    /* overlong? */
	  (s[0] == 0xed && (s[1] & 0xe0) == 0xa0) ||    /* surrogate? */
	  (s[0] == 0xef && s[1] == 0xbf &&
	   (s[2] & 0xfe) == 0xbe))                      /* U+FFFE or U+FFFF? */
	return s;
      else
	s += 3;
    } else if ((s[0] & 0xf8) == 0xf0) {
      /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
	  (s[2] & 0xc0) != 0x80 ||
	  (s[3] & 0xc0) != 0x80 ||
	  (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||    /* overlong? */
	  (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) /* > U+10FFFF? */
	return s;
      else
	s += 4;
    } else
      return s;
  }

  return NULL;
}

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

char data[1000][10000] = {1};

static void prepare_utf8(int status) {
    for (int i = 0 ; i < 1000 ; i++) {
        if(i%status == 0){
            for (int j = 0 ; j < 10000 ; j++) {
                data[i][j] = 1;
            }
            
        }
        else {
            for (int j = 0 ; j < 10000 ; j+=4) {
                data[i][j] = 0xf0;
                data[i][j+1] = 0x9f;
                data[i][j+2] = 0x91;
                data[i][j+3] = 0x8b;

            }
        }
        data[i][5002] = 0xC0;
        data[i][9999] = '\0';
        
    }
}



int main() {
    prepare_utf8(2);
    auto p = utf8_check((unsigned char*)data[1]);
    int error = 0;
    auto len = ob_well_formed_len_utf8mb4(data[1], data[1]+10000, 10000, &error);
    if(p==NULL) std::cout<<"NULL"<<std::endl;
    else {
        std::cout<< (char*)p-data[1] <<std::endl;
    }
    std::cout<<len<<std::endl;
}