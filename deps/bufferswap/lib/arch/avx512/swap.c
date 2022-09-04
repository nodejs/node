  #include <immintrin.h>
  //#include "../../include/libbufferswap.h"
  void
  swap16_simd ( char* data, size_t nbytes)
  {
    __m512i shuffle_input = _mm512_set_epi64(0x3e3f3c3d3a3b3839,
                                            0x3637343532333031,
                                            0x2e2f2c2d2a2b2829,
                                            0x2627242522232021,
                                            0x1e1f1c1d1a1b1819,
                                            0x1617141512131011,
                                            0x0e0f0c0d0a0b0809,
                                            0x0607040502030001);
    while(nbytes >= 64){
      __m512i v = _mm512_loadu_si512(data);
      __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);
      _mm512_storeu_si512(data, in);
      data += 64;
      nbytes -= 64;
    }
  }

  void
  swap32_simd ( char* data, size_t nbytes)
  {
    __m512i shuffle_input = _mm512_set_epi64(0x3c3d3e3f38393a3b,
                                            0x3435363730313233,
                                            0x2c2d2e2f28292a2b,
                                            0x2425262720212223,
                                            0x1c1d1e1f18191a1b,
                                            0x1415161710111213,
                                            0x0c0d0e0f08090a0b,
                                            0x0405060700010203);
    while(nbytes >= 64){
      __m512i v = _mm512_loadu_si512(data);
      __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);
      _mm512_storeu_si512(data, in);
      data += 64;
      nbytes -= 64;
    }
  }

  void
  swap64_simd ( char* data, size_t nbytes)
  {
    __m512i shuffle_input = _mm512_set_epi64(0x38393a3b3c3d3e3f,
                                            0x3031323334353637,
                                            0x28292a2b2c2d2e2f,
                                            0x2021222324252627,
                                            0x18191a1b1c1d1e1f,
                                            0x1011121314151617,
                                            0x08090a0b0c0d0e0f,
                                            0x0001020304050607);
    while(nbytes >= 64){
      __m512i v = _mm512_loadu_si512(data);
      __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);
      _mm512_storeu_si512(data, in);
      data += 64;
      nbytes -= 64;
    }
  }