// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_V8UTILS_H_
#define V8_V8UTILS_H_

#include "utils.h"
#include "platform.h"  // For va_list on Solaris.

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// I/O support.

#if __GNUC__ >= 4
// On gcc we can ask the compiler to check the types of %d-style format
// specifiers and their associated arguments.  TODO(erikcorry) fix this
// so it works on MacOSX.
#if defined(__MACH__) && defined(__APPLE__)
#define PRINTF_CHECKING
#define FPRINTF_CHECKING
#else  // MacOsX.
#define PRINTF_CHECKING __attribute__ ((format (printf, 1, 2)))
#define FPRINTF_CHECKING __attribute__ ((format (printf, 2, 3)))
#endif
#else
#define PRINTF_CHECKING
#define FPRINTF_CHECKING
#endif

// Our version of printf().
void PRINTF_CHECKING PrintF(const char* format, ...);
void FPRINTF_CHECKING PrintF(FILE* out, const char* format, ...);

// Prepends the current process ID to the output.
void PRINTF_CHECKING PrintPID(const char* format, ...);

// Our version of fflush.
void Flush(FILE* out);

inline void Flush() {
  Flush(stdout);
}


// Read a line of characters after printing the prompt to stdout. The resulting
// char* needs to be disposed off with DeleteArray by the caller.
char* ReadLine(const char* prompt);


// Read and return the raw bytes in a file. the size of the buffer is returned
// in size.
// The returned buffer must be freed by the caller.
byte* ReadBytes(const char* filename, int* size, bool verbose = true);


// Append size chars from str to the file given by filename.
// The file is overwritten. Returns the number of chars written.
int AppendChars(const char* filename,
                const char* str,
                int size,
                bool verbose = true);


// Write size chars from str to the file given by filename.
// The file is overwritten. Returns the number of chars written.
int WriteChars(const char* filename,
               const char* str,
               int size,
               bool verbose = true);


// Write size bytes to the file given by filename.
// The file is overwritten. Returns the number of bytes written.
int WriteBytes(const char* filename,
               const byte* bytes,
               int size,
               bool verbose = true);


// Write the C code
// const char* <varname> = "<str>";
// const int <varname>_len = <len>;
// to the file given by filename. Only the first len chars are written.
int WriteAsCFile(const char* filename, const char* varname,
                 const char* str, int size, bool verbose = true);


// ----------------------------------------------------------------------------
// Data structures

template <typename T>
inline Vector< Handle<Object> > HandleVector(v8::internal::Handle<T>* elms,
                                             int length) {
  return Vector< Handle<Object> >(
      reinterpret_cast<v8::internal::Handle<Object>*>(elms), length);
}


// ----------------------------------------------------------------------------
// Memory

// Copies words from |src| to |dst|. The data spans must not overlap.
template <typename T>
inline void CopyWords(T* dst, const T* src, size_t num_words) {
  STATIC_ASSERT(sizeof(T) == kPointerSize);
  ASSERT(Min(dst, const_cast<T*>(src)) + num_words <=
         Max(dst, const_cast<T*>(src)));
  ASSERT(num_words > 0);

  // Use block copying OS::MemCopy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const size_t kBlockCopyLimit = 16;

  if (num_words < kBlockCopyLimit) {
    do {
      num_words--;
      *dst++ = *src++;
    } while (num_words > 0);
  } else {
    OS::MemCopy(dst, src, num_words * kPointerSize);
  }
}


// Copies words from |src| to |dst|. No restrictions.
template <typename T>
inline void MoveWords(T* dst, const T* src, size_t num_words) {
  STATIC_ASSERT(sizeof(T) == kPointerSize);
  ASSERT(num_words > 0);

  // Use block copying OS::MemCopy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const size_t kBlockCopyLimit = 16;

  if (num_words < kBlockCopyLimit &&
      ((dst < src) || (dst >= (src + num_words * kPointerSize)))) {
    T* end = dst + num_words;
    do {
      num_words--;
      *dst++ = *src++;
    } while (num_words > 0);
  } else {
    OS::MemMove(dst, src, num_words * kPointerSize);
  }
}


// Copies data from |src| to |dst|.  The data spans must not overlap.
template <typename T>
inline void CopyBytes(T* dst, const T* src, size_t num_bytes) {
  STATIC_ASSERT(sizeof(T) == 1);
  ASSERT(Min(dst, const_cast<T*>(src)) + num_bytes <=
         Max(dst, const_cast<T*>(src)));
  if (num_bytes == 0) return;

  // Use block copying OS::MemCopy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const int kBlockCopyLimit = OS::kMinComplexMemCopy;

  if (num_bytes < static_cast<size_t>(kBlockCopyLimit)) {
    do {
      num_bytes--;
      *dst++ = *src++;
    } while (num_bytes > 0);
  } else {
    OS::MemCopy(dst, src, num_bytes);
  }
}


// Copies data from |src| to |dst|. No restrictions.
template <typename T>
inline void MoveBytes(T* dst, const T* src, size_t num_bytes) {
  STATIC_ASSERT(sizeof(T) == 1);
  switch (num_bytes) {
  case 0: return;
  case 1:
    *dst = *src;
    return;
#ifdef V8_HOST_CAN_READ_UNALIGNED
  case 2:
    *reinterpret_cast<uint16_t*>(dst) = *reinterpret_cast<const uint16_t*>(src);
    return;
  case 3: {
    uint16_t part1 = *reinterpret_cast<const uint16_t*>(src);
    byte part2 = *(src + 2);
    *reinterpret_cast<uint16_t*>(dst) = part1;
    *(dst + 2) = part2;
    return;
  }
  case 4:
    *reinterpret_cast<uint32_t*>(dst) = *reinterpret_cast<const uint32_t*>(src);
    return;
  case 5:
  case 6:
  case 7:
  case 8: {
    uint32_t part1 = *reinterpret_cast<const uint32_t*>(src);
    uint32_t part2 = *reinterpret_cast<const uint32_t*>(src + num_bytes - 4);
    *reinterpret_cast<uint32_t*>(dst) = part1;
    *reinterpret_cast<uint32_t*>(dst + num_bytes - 4) = part2;
    return;
  }
  case 9:
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:
  case 16: {
    double part1 = *reinterpret_cast<const double*>(src);
    double part2 = *reinterpret_cast<const double*>(src + num_bytes - 8);
    *reinterpret_cast<double*>(dst) = part1;
    *reinterpret_cast<double*>(dst + num_bytes - 8) = part2;
    return;
  }
#endif
  default:
    OS::MemMove(dst, src, num_bytes);
    return;
  }
}


template <typename T, typename U>
inline void MemsetPointer(T** dest, U* value, int counter) {
#ifdef DEBUG
  T* a = NULL;
  U* b = NULL;
  a = b;  // Fake assignment to check assignability.
  USE(a);
#endif  // DEBUG
#if V8_HOST_ARCH_IA32
#define STOS "stosl"
#elif V8_HOST_ARCH_X64
#define STOS "stosq"
#endif
#if defined(__native_client__)
  // This STOS sequence does not validate for x86_64 Native Client.
  // Here we #undef STOS to force use of the slower C version.
  // TODO(bradchen): Profile V8 and implement a faster REP STOS
  // here if the profile indicates it matters.
#undef STOS
#endif

#if defined(__GNUC__) && defined(STOS)
  asm volatile(
      "cld;"
      "rep ; " STOS
      : "+&c" (counter), "+&D" (dest)
      : "a" (value)
      : "memory", "cc");
#else
  for (int i = 0; i < counter; i++) {
    dest[i] = value;
  }
#endif

#undef STOS
}


// Simple wrapper that allows an ExternalString to refer to a
// Vector<const char>. Doesn't assume ownership of the data.
class AsciiStringAdapter: public v8::String::ExternalAsciiStringResource {
 public:
  explicit AsciiStringAdapter(Vector<const char> data) : data_(data) {}

  virtual const char* data() const { return data_.start(); }

  virtual size_t length() const { return data_.length(); }

 private:
  Vector<const char> data_;
};


// Simple support to read a file into a 0-terminated C-string.
// The returned buffer must be freed by the caller.
// On return, *exits tells whether the file existed.
Vector<const char> ReadFile(const char* filename,
                            bool* exists,
                            bool verbose = true);
Vector<const char> ReadFile(FILE* file,
                            bool* exists,
                            bool verbose = true);


template <typename sourcechar, typename sinkchar>
INLINE(static void CopyCharsUnsigned(sinkchar* dest,
                                     const sourcechar* src,
                                     int chars));
#if defined(V8_HOST_ARCH_ARM)
INLINE(void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, int chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src, int chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, int chars));
#endif

// Copy from ASCII/16bit chars to ASCII/16bit chars.
template <typename sourcechar, typename sinkchar>
INLINE(void CopyChars(sinkchar* dest, const sourcechar* src, int chars));

template<typename sourcechar, typename sinkchar>
void CopyChars(sinkchar* dest, const sourcechar* src, int chars) {
  ASSERT(sizeof(sourcechar) <= 2);
  ASSERT(sizeof(sinkchar) <= 2);
  if (sizeof(sinkchar) == 1) {
    if (sizeof(sourcechar) == 1) {
      CopyCharsUnsigned(reinterpret_cast<uint8_t*>(dest),
                        reinterpret_cast<const uint8_t*>(src),
                        chars);
    } else {
      CopyCharsUnsigned(reinterpret_cast<uint8_t*>(dest),
                        reinterpret_cast<const uint16_t*>(src),
                        chars);
    }
  } else {
    if (sizeof(sourcechar) == 1) {
      CopyCharsUnsigned(reinterpret_cast<uint16_t*>(dest),
                        reinterpret_cast<const uint8_t*>(src),
                        chars);
    } else {
      CopyCharsUnsigned(reinterpret_cast<uint16_t*>(dest),
                        reinterpret_cast<const uint16_t*>(src),
                        chars);
    }
  }
}

template <typename sourcechar, typename sinkchar>
void CopyCharsUnsigned(sinkchar* dest, const sourcechar* src, int chars) {
  sinkchar* limit = dest + chars;
#ifdef V8_HOST_CAN_READ_UNALIGNED
  if (sizeof(*dest) == sizeof(*src)) {
    if (chars >= static_cast<int>(OS::kMinComplexMemCopy / sizeof(*dest))) {
      OS::MemCopy(dest, src, chars * sizeof(*dest));
      return;
    }
    // Number of characters in a uintptr_t.
    static const int kStepSize = sizeof(uintptr_t) / sizeof(*dest);  // NOLINT
    ASSERT(dest + kStepSize > dest);  // Check for overflow.
    while (dest + kStepSize <= limit) {
      *reinterpret_cast<uintptr_t*>(dest) =
          *reinterpret_cast<const uintptr_t*>(src);
      dest += kStepSize;
      src += kStepSize;
    }
  }
#endif
  while (dest < limit) {
    *dest++ = static_cast<sinkchar>(*src++);
  }
}


#if defined(V8_HOST_ARCH_ARM)
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, int chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
    case 2:
      memcpy(dest, src, 2);
      break;
    case 3:
      memcpy(dest, src, 3);
      break;
    case 4:
      memcpy(dest, src, 4);
      break;
    case 5:
      memcpy(dest, src, 5);
      break;
    case 6:
      memcpy(dest, src, 6);
      break;
    case 7:
      memcpy(dest, src, 7);
      break;
    case 8:
      memcpy(dest, src, 8);
      break;
    case 9:
      memcpy(dest, src, 9);
      break;
    case 10:
      memcpy(dest, src, 10);
      break;
    case 11:
      memcpy(dest, src, 11);
      break;
    case 12:
      memcpy(dest, src, 12);
      break;
    case 13:
      memcpy(dest, src, 13);
      break;
    case 14:
      memcpy(dest, src, 14);
      break;
    case 15:
      memcpy(dest, src, 15);
      break;
    default:
      OS::MemCopy(dest, src, chars);
      break;
  }
}


void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src, int chars) {
  if (chars >= OS::kMinComplexConvertMemCopy) {
    OS::MemCopyUint16Uint8(dest, src, chars);
  } else {
    OS::MemCopyUint16Uint8Wrapper(dest, src, chars);
  }
}


void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, int chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
    case 2:
      memcpy(dest, src, 4);
      break;
    case 3:
      memcpy(dest, src, 6);
      break;
    case 4:
      memcpy(dest, src, 8);
      break;
    case 5:
      memcpy(dest, src, 10);
      break;
    case 6:
      memcpy(dest, src, 12);
      break;
    case 7:
      memcpy(dest, src, 14);
      break;
    default:
      OS::MemCopy(dest, src, chars * sizeof(*dest));
      break;
  }
}
#endif


class StringBuilder : public SimpleStringBuilder {
 public:
  explicit StringBuilder(int size) : SimpleStringBuilder(size) { }
  StringBuilder(char* buffer, int size) : SimpleStringBuilder(buffer, size) { }

  // Add formatted contents to the builder just like printf().
  void AddFormatted(const char* format, ...);

  // Add formatted contents like printf based on a va_list.
  void AddFormattedList(const char* format, va_list list);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringBuilder);
};

} }  // namespace v8::internal

#endif  // V8_V8UTILS_H_
