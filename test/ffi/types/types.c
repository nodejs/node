#include "stdint.h"
#define test(typ, name) typ test_add_##name(typ a, typ b) { return a + b; } \
  typ * test_add_ptr_##name(typ * ret, typ * a, typ * b) { \
    *ret = *a + *b; \
    return ret; \
  }
test(char, char);
test(signed char, signed_char);
test(unsigned char, unsigned_char);
test(short, short);
test(short int, short_int);
test(signed short, signed_short);
test(signed short int, signed_short_int);
test(unsigned short, unsigned_short);
test(unsigned short int, unsigned_short_int);
test(int, int);
test(signed, signed);
test(signed int, signed_int);
test(unsigned, unsigned);
test(unsigned int, unsigned_int);
test(long, long);
test(long int, long_int);
test(signed long, signed_long);
test(signed long int, signed_long_int);
test(unsigned long, unsigned_long);
test(unsigned long int, unsigned_long_int);
test(long long, long_long);
test(long long int, long_long_int);
test(signed long long, signed_long_long);
test(signed long long int, signed_long_long_int);
test(unsigned long long, unsigned_long_long);
test(unsigned long long int, unsigned_long_long_int);
test(float, float);
test(double, double);
test(uint8_t, uint8_t);
test(int8_t, int8_t);
test(uint16_t, uint16_t);
test(int16_t, int16_t);
test(uint32_t, uint32_t);
test(int32_t, int32_t);
test(uint64_t, uint64_t);
test(int64_t, int64_t);
