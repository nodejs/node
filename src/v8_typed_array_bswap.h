// V8 Typed Array implementation.
// (c) Dean McNamee <dean@gmail.com>, 2012.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef V8_TYPED_ARRAY_BSWAP_H_
#define V8_TYPED_ARRAY_BSWAP_H_

// Windows will always be little endian (including ARM), so we just need to
// worry about gcc.
#if defined (__ppc__) || defined (__ppc64__) || defined(__ARMEB__)
#define V8_TYPED_ARRAY_BIG_ENDIAN 1
#else
#define V8_TYPED_ARRAY_LITTLE_ENDIAN 1
#endif

#if defined (_MSC_VER) && (_MSC_VER < 1600)
  typedef unsigned char     uint8_t;
  typedef signed char       int8_t;
  typedef unsigned __int16  uint16_t;
  typedef signed __int16    int16_t;
  typedef unsigned __int32  uint32_t;
  typedef signed __int32    int32_t;
  typedef unsigned __int64  uint64_t;
  typedef signed __int64    int64_t;
  // Definitions to avoid ICU redefinition issue
  #define U_HAVE_INT8_T 1
  #define U_HAVE_UINT8_T 1
  #define U_HAVE_INT16_T 1
  #define U_HAVE_UINT16_T 1
  #define U_HAVE_INT32_T 1
  #define U_HAVE_UINT32_T 1
  #define U_HAVE_INT64_T 1
  #define U_HAVE_UINT64_T 1
#else
  #include <stdint.h>
#endif

#if defined (_MSC_VER)
#define V8_TYPED_ARRAY_BSWAP16 _byteswap_ushort
#define V8_TYPED_ARRAY_BSWAP32 _byteswap_ulong
#define V8_TYPED_ARRAY_BSWAP64 _byteswap_uint64
#else
// On LLVM based compilers we can feature test, but for GCC we unfortunately
// have to rely on the version.  Additionally __builtin_bswap32/64 were added
// in GCC 4.3, but __builtin_bswap16 was not added until GCC 4.8.
// We should be able to assume GCC/LLVM here (and can use ULL constants, etc).
// Fallback swap macros taken from QEMU bswap.h
#ifdef __has_builtin
#define V8_TYPED_ARRAY_BSWAP_HAS_BUILTIN(x) __has_builtin(x)
#define V8_TYPED_ARRAY_BSWAP_HAS_BUILTIN16(x) __has_builtin(x)
#else
#define V8_TYPED_ARRAY_BSWAP_HAS_BUILTIN(x) (defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
#define V8_TYPED_ARRAY_BSWAP_HAS_BUILTIN16(x) (defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))
#endif

#if V8_TYPED_ARRAY_BSWAP_HAS_BUILTIN(__builtin_bswap64)
#define V8_TYPED_ARRAY_BSWAP64 __builtin_bswap64
#else
#define V8_TYPED_ARRAY_BSWAP64(x) \
({ \
	uint64_t __x = (x); \
	((uint64_t)( \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	  (uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})
#endif

#if V8_TYPED_ARRAY_BSWAP_HAS_BUILTIN(__builtin_bswap32)
#define V8_TYPED_ARRAY_BSWAP32 __builtin_bswap32
#else
#define V8_TYPED_ARRAY_BSWAP32(x) \
({ \
	uint32_t __x = (x); \
	((uint32_t)( \
		(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) )); \
})
#endif

#if V8_TYPED_ARRAY_BSWAP_HAS_BUILTIN16(__builtin_bswap16)
#define V8_TYPED_ARRAY_BSWAP16 __builtin_bswap16
#else
#define V8_TYPED_ARRAY_BSWAP16(x) \
({ \
	uint16_t __x = (x); \
	((uint16_t)( \
		(((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
		(((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
})
#endif
#endif


namespace v8_typed_array {

template <typename T>
inline T SwapBytes(T x) {
  typedef char NoSwapBytesForType[sizeof(T) == 0 ? 1 : -1];
  return 0;
}

template <>
inline signed char SwapBytes(signed char x) { return x; }
template <>
inline unsigned char SwapBytes(unsigned char x) { return x; }
template <>
inline uint16_t SwapBytes(uint16_t x) { return V8_TYPED_ARRAY_BSWAP16(x); }
template <>
inline int16_t SwapBytes(int16_t x) { return V8_TYPED_ARRAY_BSWAP16(x); }
template <>
inline uint32_t SwapBytes(uint32_t x) { return V8_TYPED_ARRAY_BSWAP32(x); }
template <>
inline int32_t SwapBytes(int32_t x) { return V8_TYPED_ARRAY_BSWAP32(x); }
template <>
inline uint64_t SwapBytes(uint64_t x) { return V8_TYPED_ARRAY_BSWAP64(x); }
template <>
inline int64_t SwapBytes(int64_t x) { return V8_TYPED_ARRAY_BSWAP64(x); }

template <typename T>  // General implementation for all non-FP types.
inline T LoadAndSwapBytes(void* ptr) {
  T val;
  memcpy(&val, ptr, sizeof(T));
  return SwapBytes(val);
}

template <>
inline float LoadAndSwapBytes<float>(void* ptr) {
  typedef char VerifySizesAreEqual[sizeof(uint32_t) == sizeof(float) ? 1 : -1];
  uint32_t swappable;
  float val;
  memcpy(&swappable, ptr, sizeof(swappable));
  swappable = SwapBytes(swappable);
  memcpy(&val, &swappable, sizeof(swappable));
  return val;
}

template <>
inline double LoadAndSwapBytes<double>(void* ptr) {
  typedef char VerifySizesAreEqual[sizeof(uint64_t) == sizeof(double) ? 1 : -1];
  uint64_t swappable;
  double val;
  memcpy(&swappable, ptr, sizeof(swappable));
  swappable = SwapBytes(swappable);
  memcpy(&val, &swappable, sizeof(swappable));
  return val;
}

template <typename T>  // General implementation for all non-FP types.
inline void SwapBytesAndStore(void* ptr, T val) {
  val = SwapBytes(val);
  memcpy(ptr, &val, sizeof(T));
}

template <>
inline void SwapBytesAndStore(void* ptr, float val) {
  typedef char VerifySizesAreEqual[sizeof(uint32_t) == sizeof(float) ? 1 : -1];
  uint32_t swappable;
  memcpy(&swappable, &val, sizeof(swappable));
  swappable = SwapBytes(swappable);
  memcpy(ptr, &swappable, sizeof(swappable));
}

template <>
inline void SwapBytesAndStore(void* ptr, double val) {
  typedef char VerifySizesAreEqual[sizeof(uint64_t) == sizeof(double) ? 1 : -1];
  uint64_t swappable;
  memcpy(&swappable, &val, sizeof(swappable));
  swappable = SwapBytes(swappable);
  memcpy(ptr, &swappable, sizeof(swappable));
}

}  // namespace v8_typed_array

#endif  // V8_TYPED_ARRAY_BSWAP_H_
