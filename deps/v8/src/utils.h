// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_UTILS_H_
#define V8_UTILS_H_

#include <stdlib.h>

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// General helper functions

// Returns true iff x is a power of 2 (or zero). Cannot be used with the
// maximally negative value of the type T (the -1 overflows).
template <typename T>
static inline bool IsPowerOf2(T x) {
  return (x & (x - 1)) == 0;
}


// The C++ standard leaves the semantics of '>>' undefined for
// negative signed operands. Most implementations do the right thing,
// though.
static inline int ArithmeticShiftRight(int x, int s) {
  return x >> s;
}


// Compute the 0-relative offset of some absolute value x of type T.
// This allows conversion of Addresses and integral types into
// 0-relative int offsets.
template <typename T>
static inline intptr_t OffsetFrom(T x) {
  return x - static_cast<T>(0);
}


// Compute the absolute value of type T for some 0-relative offset x.
// This allows conversion of 0-relative int offsets into Addresses and
// integral types.
template <typename T>
static inline T AddressFrom(intptr_t x) {
  return static_cast<T>(static_cast<T>(0) + x);
}


// Return the largest multiple of m which is <= x.
template <typename T>
static inline T RoundDown(T x, int m) {
  ASSERT(IsPowerOf2(m));
  return AddressFrom<T>(OffsetFrom(x) & -m);
}


// Return the smallest multiple of m which is >= x.
template <typename T>
static inline T RoundUp(T x, int m) {
  return RoundDown(x + m - 1, m);
}


template <typename T>
static int Compare(const T& a, const T& b) {
  if (a == b)
    return 0;
  else if (a < b)
    return -1;
  else
    return 1;
}


template <typename T>
static int PointerValueCompare(const T* a, const T* b) {
  return Compare<T>(*a, *b);
}


// Returns the smallest power of two which is >= x. If you pass in a
// number that is already a power of two, it is returned as is.
uint32_t RoundUpToPowerOf2(uint32_t x);


template <typename T>
static inline bool IsAligned(T value, T alignment) {
  ASSERT(IsPowerOf2(alignment));
  return (value & (alignment - 1)) == 0;
}


// Returns true if (addr + offset) is aligned.
static inline bool IsAddressAligned(Address addr,
                                    intptr_t alignment,
                                    int offset) {
  intptr_t offs = OffsetFrom(addr + offset);
  return IsAligned(offs, alignment);
}


// Returns the maximum of the two parameters.
template <typename T>
static T Max(T a, T b) {
  return a < b ? b : a;
}


// Returns the minimum of the two parameters.
template <typename T>
static T Min(T a, T b) {
  return a < b ? a : b;
}


inline int StrLength(const char* string) {
  size_t length = strlen(string);
  ASSERT(length == static_cast<size_t>(static_cast<int>(length)));
  return static_cast<int>(length);
}


// ----------------------------------------------------------------------------
// BitField is a help template for encoding and decode bitfield with
// unsigned content.
template<class T, int shift, int size>
class BitField {
 public:
  // Tells whether the provided value fits into the bit field.
  static bool is_valid(T value) {
    return (static_cast<uint32_t>(value) & ~((1U << (size)) - 1)) == 0;
  }

  // Returns a uint32_t mask of bit field.
  static uint32_t mask() {
    // To use all bits of a uint32 in a bitfield without compiler warnings we
    // have to compute 2^32 without using a shift count of 32.
    return ((1U << shift) << size) - (1U << shift);
  }

  // Returns a uint32_t with the bit field value encoded.
  static uint32_t encode(T value) {
    ASSERT(is_valid(value));
    return static_cast<uint32_t>(value) << shift;
  }

  // Extracts the bit field from the value.
  static T decode(uint32_t value) {
    return static_cast<T>((value & mask()) >> shift);
  }
};


// ----------------------------------------------------------------------------
// Hash function.

uint32_t ComputeIntegerHash(uint32_t key);


// ----------------------------------------------------------------------------
// I/O support.

// Our version of printf(). Avoids compilation errors that we get
// with standard printf when attempting to print pointers, etc.
// (the errors are due to the extra compilation flags, which we
// want elsewhere).
void PrintF(const char* format, ...);

// Our version of fflush.
void Flush();


// Read a line of characters after printing the prompt to stdout. The resulting
// char* needs to be disposed off with DeleteArray by the caller.
char* ReadLine(const char* prompt);


// Read and return the raw bytes in a file. the size of the buffer is returned
// in size.
// The returned buffer must be freed by the caller.
byte* ReadBytes(const char* filename, int* size, bool verbose = true);


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
// Miscellaneous

// A static resource holds a static instance that can be reserved in
// a local scope using an instance of Access.  Attempts to re-reserve
// the instance will cause an error.
template <typename T>
class StaticResource {
 public:
  StaticResource() : is_reserved_(false)  {}

 private:
  template <typename S> friend class Access;
  T instance_;
  bool is_reserved_;
};


// Locally scoped access to a static resource.
template <typename T>
class Access {
 public:
  explicit Access(StaticResource<T>* resource)
    : resource_(resource)
    , instance_(&resource->instance_) {
    ASSERT(!resource->is_reserved_);
    resource->is_reserved_ = true;
  }

  ~Access() {
    resource_->is_reserved_ = false;
    resource_ = NULL;
    instance_ = NULL;
  }

  T* value()  { return instance_; }
  T* operator -> ()  { return instance_; }

 private:
  StaticResource<T>* resource_;
  T* instance_;
};


template <typename T>
class Vector {
 public:
  Vector() : start_(NULL), length_(0) {}
  Vector(T* data, int length) : start_(data), length_(length) {
    ASSERT(length == 0 || (length > 0 && data != NULL));
  }

  static Vector<T> New(int length) {
    return Vector<T>(NewArray<T>(length), length);
  }

  // Returns a vector using the same backing storage as this one,
  // spanning from and including 'from', to but not including 'to'.
  Vector<T> SubVector(int from, int to) {
    ASSERT(from < length_);
    ASSERT(to <= length_);
    ASSERT(from < to);
    return Vector<T>(start() + from, to - from);
  }

  // Returns the length of the vector.
  int length() const { return length_; }

  // Returns whether or not the vector is empty.
  bool is_empty() const { return length_ == 0; }

  // Returns the pointer to the start of the data in the vector.
  T* start() const { return start_; }

  // Access individual vector elements - checks bounds in debug mode.
  T& operator[](int index) const {
    ASSERT(0 <= index && index < length_);
    return start_[index];
  }

  T& first() { return start_[0]; }

  T& last() { return start_[length_ - 1]; }

  // Returns a clone of this vector with a new backing store.
  Vector<T> Clone() const {
    T* result = NewArray<T>(length_);
    for (int i = 0; i < length_; i++) result[i] = start_[i];
    return Vector<T>(result, length_);
  }

  void Sort(int (*cmp)(const T*, const T*)) {
    typedef int (*RawComparer)(const void*, const void*);
    qsort(start(),
          length(),
          sizeof(T),
          reinterpret_cast<RawComparer>(cmp));
  }

  void Sort() {
    Sort(PointerValueCompare<T>);
  }

  void Truncate(int length) {
    ASSERT(length <= length_);
    length_ = length;
  }

  // Releases the array underlying this vector. Once disposed the
  // vector is empty.
  void Dispose() {
    if (is_empty()) return;
    DeleteArray(start_);
    start_ = NULL;
    length_ = 0;
  }

  inline Vector<T> operator+(int offset) {
    ASSERT(offset < length_);
    return Vector<T>(start_ + offset, length_ - offset);
  }

  // Factory method for creating empty vectors.
  static Vector<T> empty() { return Vector<T>(NULL, 0); }

 protected:
  void set_start(T* start) { start_ = start; }

 private:
  T* start_;
  int length_;
};


// A temporary assignment sets a (non-local) variable to a value on
// construction and resets it the value on destruction.
template <typename T>
class TempAssign {
 public:
  TempAssign(T* var, T value): var_(var), old_value_(*var) {
    *var = value;
  }

  ~TempAssign() { *var_ = old_value_; }

 private:
  T* var_;
  T old_value_;
};


template <typename T, int kSize>
class EmbeddedVector : public Vector<T> {
 public:
  EmbeddedVector() : Vector<T>(buffer_, kSize) { }

  // When copying, make underlying Vector to reference our buffer.
  EmbeddedVector(const EmbeddedVector& rhs)
      : Vector<T>(rhs) {
    memcpy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    set_start(buffer_);
  }

  EmbeddedVector& operator=(const EmbeddedVector& rhs) {
    if (this == &rhs) return *this;
    Vector<T>::operator=(rhs);
    memcpy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    set_start(buffer_);
    return *this;
  }

 private:
  T buffer_[kSize];
};


template <typename T>
class ScopedVector : public Vector<T> {
 public:
  explicit ScopedVector(int length) : Vector<T>(NewArray<T>(length), length) { }
  ~ScopedVector() {
    DeleteArray(this->start());
  }
};


inline Vector<const char> CStrVector(const char* data) {
  return Vector<const char>(data, StrLength(data));
}

inline Vector<char> MutableCStrVector(char* data) {
  return Vector<char>(data, StrLength(data));
}

inline Vector<char> MutableCStrVector(char* data, int max) {
  int length = StrLength(data);
  return Vector<char>(data, (length < max) ? length : max);
}

template <typename T>
inline Vector< Handle<Object> > HandleVector(v8::internal::Handle<T>* elms,
                                             int length) {
  return Vector< Handle<Object> >(
      reinterpret_cast<v8::internal::Handle<Object>*>(elms), length);
}


// Simple support to read a file into a 0-terminated C-string.
// The returned buffer must be freed by the caller.
// On return, *exits tells whether the file existed.
Vector<const char> ReadFile(const char* filename,
                            bool* exists,
                            bool verbose = true);


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


// Helper class for building result strings in a character buffer. The
// purpose of the class is to use safe operations that checks the
// buffer bounds on all operations in debug mode.
class StringBuilder {
 public:
  // Create a string builder with a buffer of the given size. The
  // buffer is allocated through NewArray<char> and must be
  // deallocated by the caller of Finalize().
  explicit StringBuilder(int size);

  StringBuilder(char* buffer, int size)
      : buffer_(buffer, size), position_(0) { }

  ~StringBuilder() { if (!is_finalized()) Finalize(); }

  int size() const { return buffer_.length(); }

  // Get the current position in the builder.
  int position() const {
    ASSERT(!is_finalized());
    return position_;
  }

  // Reset the position.
  void Reset() { position_ = 0; }

  // Add a single character to the builder. It is not allowed to add
  // 0-characters; use the Finalize() method to terminate the string
  // instead.
  void AddCharacter(char c) {
    ASSERT(c != '\0');
    ASSERT(!is_finalized() && position_ < buffer_.length());
    buffer_[position_++] = c;
  }

  // Add an entire string to the builder. Uses strlen() internally to
  // compute the length of the input string.
  void AddString(const char* s);

  // Add the first 'n' characters of the given string 's' to the
  // builder. The input string must have enough characters.
  void AddSubstring(const char* s, int n);

  // Add formatted contents to the builder just like printf().
  void AddFormatted(const char* format, ...);

  // Add character padding to the builder. If count is non-positive,
  // nothing is added to the builder.
  void AddPadding(char c, int count);

  // Finalize the string by 0-terminating it and returning the buffer.
  char* Finalize();

 private:
  Vector<char> buffer_;
  int position_;

  bool is_finalized() const { return position_ < 0; }

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringBuilder);
};


// Copy from ASCII/16bit chars to ASCII/16bit chars.
template <typename sourcechar, typename sinkchar>
static inline void CopyChars(sinkchar* dest, const sourcechar* src, int chars) {
  sinkchar* limit = dest + chars;
#ifdef V8_HOST_CAN_READ_UNALIGNED
  if (sizeof(*dest) == sizeof(*src)) {
    // Number of characters in a uint32_t.
    static const int kStepSize = sizeof(uint32_t) / sizeof(*dest);  // NOLINT
    while (dest <= limit - kStepSize) {
      *reinterpret_cast<uint32_t*>(dest) =
          *reinterpret_cast<const uint32_t*>(src);
      dest += kStepSize;
      src += kStepSize;
    }
  }
#endif
  while (dest < limit) {
    *dest++ = static_cast<sinkchar>(*src++);
  }
}


// Calculate 10^exponent.
int TenToThe(int exponent);

} }  // namespace v8::internal

#endif  // V8_UTILS_H_
