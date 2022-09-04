#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

#include "../include/libbufferswap.h"

#if (__x86_64__ || _M_X64)
  #define BUFFERSWAP_X86
#endif

#ifdef BUFFERSWAP_X86
  #ifdef _MSC_VER
    #include <intrin.h>
    #define __cpuid_count(__level, __count, __eax, __ebx, __ecx, __edx) \
    {						\
      int info[4];				\
      __cpuidex(info, __level, __count);	\
      __eax = info[0];			\
      __ebx = info[1];			\
      __ecx = info[2];			\
      __edx = info[3];			\
    }
    #define __cpuid(__level, __eax, __ebx, __ecx, __edx) \
      __cpuid_count(__level, 0, __eax, __ebx, __ecx, __edx)
  #else
	  #include <cpuid.h>
		#if ((__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 2) || (__clang_major__ >= 3))
			static inline uint64_t _xgetbv (uint32_t index)
			{
				uint32_t eax, edx;
				__asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
				return ((uint64_t)edx << 32) | eax;
			}
		#else
			#error "Platform not supported"
		#endif
  #endif

  #define bit_XSAVE_XRSTORE (1 << 27)
  #define bit_AVX512VBMI (1 << 1)
  #ifndef _XCR_XFEATURE_ENABLED_MASK
    #define _XCR_XFEATURE_ENABLED_MASK 0
  #endif
  #define _XCR_XMM_AND_YMM_STATE_ENABLED_BY_OS 0x6

  // These static variable is initialized once when the library is first
  // used, and remain in use for the remaining lifetime of the program.
  // The idea being that CPU features don't change at runtime.
  static bool supported = false;

  bool support_simd() {
    if (supported) {
      return true;
    }
    else {
      unsigned int eax, ebx = 0, ecx = 0, edx;
      unsigned int max_level;

      #ifdef _MSC_VER
        int info[4];
        __cpuidex(info, 0, 0);
        max_level = info[0];
      #else
        max_level = __get_cpuid_max(0, NULL);
      #endif

      // Check for AVX512 support:
      // 1) CPUID indicates that the OS uses XSAVE and XRSTORE instructions
      //    (allowing saving YMM registers on context switch)
      // 2) CPUID indicates support for AVX
      // 3) XGETBV indicates the AVX registers will be saved and restored on
      //    context switch
      //
      // Note that XGETBV is only available on 686 or later CPUs, so the
      // instruction needs to be conditionally run.
      if (max_level >= 1) {
        __cpuid_count(1, 0, eax, ebx, ecx, edx);
        if (ecx & bit_XSAVE_XRSTORE) {
          uint64_t xcr_mask;
          xcr_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
          if (xcr_mask & _XCR_XMM_AND_YMM_STATE_ENABLED_BY_OS) {
            if (max_level >= 7) {
              __cpuid_count(7, 0, eax, ebx, ecx, edx);
              if (ecx & bit_AVX512VBMI) {
                supported = true;
              }
            }
          }
        }
      }
    }
    return supported;
  }
  #include <immintrin.h>
  void
  swap16 ( char* data, size_t nbytes)
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
  swap32 ( char* data, size_t nbytes)
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
  swap64 ( char* data, size_t nbytes)
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
#else // only support X64
  bool support_simd() {
    return false;
    }
#endif

void
swap_simd (char* data, size_t nbytes, size_t size) {
  if(!support_simd()) 
    return;

  switch(size) {
    case 16:
      swap16(data, nbytes);
      break;
    case 32:
      swap32(data, nbytes);
      break;
    case 64:
      swap64(data, nbytes);
      break;
  }
}
