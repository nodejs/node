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


// Data structures

template <typename T>
inline Vector< Handle<Object> > HandleVector(v8::internal::Handle<T>* elms,
                                             int length) {
  return Vector< Handle<Object> >(
      reinterpret_cast<v8::internal::Handle<Object>*>(elms), length);
}

// Memory

// Copies data from |src| to |dst|.  The data spans MUST not overlap.
template <typename T>
inline void CopyWords(T* dst, T* src, int num_words) {
  STATIC_ASSERT(sizeof(T) == kPointerSize);
  ASSERT(Min(dst, src) + num_words <= Max(dst, src));
  ASSERT(num_words > 0);

  // Use block copying memcpy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const int kBlockCopyLimit = 16;

  if (num_words >= kBlockCopyLimit) {
    memcpy(dst, src, num_words * kPointerSize);
  } else {
    int remaining = num_words;
    do {
      remaining--;
      *dst++ = *src++;
    } while (remaining > 0);
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
#if defined(V8_HOST_ARCH_IA32)
#define STOS "stosl"
#elif defined(V8_HOST_ARCH_X64)
#define STOS "stosq"
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


// Copy from ASCII/16bit chars to ASCII/16bit chars.
template <typename sourcechar, typename sinkchar>
INLINE(void CopyChars(sinkchar* dest, const sourcechar* src, int chars));


template <typename sourcechar, typename sinkchar>
void CopyChars(sinkchar* dest, const sourcechar* src, int chars) {
  sinkchar* limit = dest + chars;
#ifdef V8_HOST_CAN_READ_UNALIGNED
  if (sizeof(*dest) == sizeof(*src)) {
    if (chars >= static_cast<int>(OS::kMinComplexMemCopy / sizeof(*dest))) {
      OS::MemCopy(dest, src, chars * sizeof(*dest));
      return;
    }
    // Number of characters in a uintptr_t.
    static const int kStepSize = sizeof(uintptr_t) / sizeof(*dest);  // NOLINT
    while (dest <= limit - kStepSize) {
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


// A resource for using mmapped files to back external strings that are read
// from files.
class MemoryMappedExternalResource: public
    v8::String::ExternalAsciiStringResource {
 public:
  explicit MemoryMappedExternalResource(const char* filename);
  MemoryMappedExternalResource(const char* filename,
                               bool remove_file_on_cleanup);
  virtual ~MemoryMappedExternalResource();

  virtual const char* data() const { return data_; }
  virtual size_t length() const { return length_; }

  bool exists() const { return file_ != NULL; }
  bool is_empty() const { return length_ == 0; }

  bool EnsureIsAscii(bool abort_if_failed) const;
  bool EnsureIsAscii() const { return EnsureIsAscii(true); }
  bool IsAscii() const { return EnsureIsAscii(false); }

 private:
  void Init(const char* filename);

  char* filename_;
  OS::MemoryMappedFile* file_;

  const char* data_;
  size_t length_;
  bool remove_file_on_cleanup_;
};

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
