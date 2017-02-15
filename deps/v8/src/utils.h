// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_H_
#define V8_UTILS_H_

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "include/v8.h"
#include "src/allocation.h"
#include "src/base/bits.h"
#include "src/base/compiler-specific.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/globals.h"
#include "src/list.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// General helper functions

// Returns the value (0 .. 15) of a hexadecimal character c.
// If c is not a legal hexadecimal character, returns a value < 0.
inline int HexValue(uc32 c) {
  c -= '0';
  if (static_cast<unsigned>(c) <= 9) return c;
  c = (c | 0x20) - ('a' - '0');  // detect 0x11..0x16 and 0x31..0x36.
  if (static_cast<unsigned>(c) <= 5) return c + 10;
  return -1;
}

inline char HexCharOfValue(int value) {
  DCHECK(0 <= value && value <= 16);
  if (value < 10) return value + '0';
  return value - 10 + 'A';
}

inline int BoolToInt(bool b) { return b ? 1 : 0; }


// Same as strcmp, but can handle NULL arguments.
inline bool CStringEquals(const char* s1, const char* s2) {
  return (s1 == s2) || (s1 != NULL && s2 != NULL && strcmp(s1, s2) == 0);
}


// X must be a power of 2.  Returns the number of trailing zeros.
inline int WhichPowerOf2(uint32_t x) {
  DCHECK(base::bits::IsPowerOfTwo32(x));
  int bits = 0;
#ifdef DEBUG
  uint32_t original_x = x;
#endif
  if (x >= 0x10000) {
    bits += 16;
    x >>= 16;
  }
  if (x >= 0x100) {
    bits += 8;
    x >>= 8;
  }
  if (x >= 0x10) {
    bits += 4;
    x >>= 4;
  }
  switch (x) {
    default: UNREACHABLE();
    case 8: bits++;  // Fall through.
    case 4: bits++;  // Fall through.
    case 2: bits++;  // Fall through.
    case 1: break;
  }
  DCHECK_EQ(static_cast<uint32_t>(1) << bits, original_x);
  return bits;
}


// X must be a power of 2.  Returns the number of trailing zeros.
inline int WhichPowerOf2_64(uint64_t x) {
  DCHECK(base::bits::IsPowerOfTwo64(x));
  int bits = 0;
#ifdef DEBUG
  uint64_t original_x = x;
#endif
  if (x >= 0x100000000L) {
    bits += 32;
    x >>= 32;
  }
  if (x >= 0x10000) {
    bits += 16;
    x >>= 16;
  }
  if (x >= 0x100) {
    bits += 8;
    x >>= 8;
  }
  if (x >= 0x10) {
    bits += 4;
    x >>= 4;
  }
  switch (x) {
    default: UNREACHABLE();
    case 8: bits++;  // Fall through.
    case 4: bits++;  // Fall through.
    case 2: bits++;  // Fall through.
    case 1: break;
  }
  DCHECK_EQ(static_cast<uint64_t>(1) << bits, original_x);
  return bits;
}


inline int MostSignificantBit(uint32_t x) {
  static const int msb4[] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
  int nibble = 0;
  if (x & 0xffff0000) {
    nibble += 16;
    x >>= 16;
  }
  if (x & 0xff00) {
    nibble += 8;
    x >>= 8;
  }
  if (x & 0xf0) {
    nibble += 4;
    x >>= 4;
  }
  return nibble + msb4[x];
}


// The C++ standard leaves the semantics of '>>' undefined for
// negative signed operands. Most implementations do the right thing,
// though.
inline int ArithmeticShiftRight(int x, int s) {
  return x >> s;
}


template <typename T>
int Compare(const T& a, const T& b) {
  if (a == b)
    return 0;
  else if (a < b)
    return -1;
  else
    return 1;
}


template <typename T>
int PointerValueCompare(const T* a, const T* b) {
  return Compare<T>(*a, *b);
}


// Compare function to compare the object pointer value of two
// handlified objects. The handles are passed as pointers to the
// handles.
template<typename T> class Handle;  // Forward declaration.
template <typename T>
int HandleObjectPointerCompare(const Handle<T>* a, const Handle<T>* b) {
  return Compare<T*>(*(*a), *(*b));
}


template <typename T, typename U>
inline bool IsAligned(T value, U alignment) {
  return (value & (alignment - 1)) == 0;
}


// Returns true if (addr + offset) is aligned.
inline bool IsAddressAligned(Address addr,
                             intptr_t alignment,
                             int offset = 0) {
  intptr_t offs = OffsetFrom(addr + offset);
  return IsAligned(offs, alignment);
}


// Returns the maximum of the two parameters.
template <typename T>
T Max(T a, T b) {
  return a < b ? b : a;
}


// Returns the minimum of the two parameters.
template <typename T>
T Min(T a, T b) {
  return a < b ? a : b;
}

// Returns the maximum of the two parameters according to JavaScript semantics.
template <typename T>
T JSMax(T x, T y) {
  if (std::isnan(x)) return x;
  if (std::isnan(y)) return y;
  if (std::signbit(x) < std::signbit(y)) return x;
  return x > y ? x : y;
}

// Returns the maximum of the two parameters according to JavaScript semantics.
template <typename T>
T JSMin(T x, T y) {
  if (std::isnan(x)) return x;
  if (std::isnan(y)) return y;
  if (std::signbit(x) < std::signbit(y)) return y;
  return x > y ? y : x;
}

// Returns the absolute value of its argument.
template <typename T>
T Abs(T a) {
  return a < 0 ? -a : a;
}


// Floor(-0.0) == 0.0
inline double Floor(double x) {
#if V8_CC_MSVC
  if (x == 0) return x;  // Fix for issue 3477.
#endif
  return std::floor(x);
}

inline double Pow(double x, double y) {
  if (y == 0.0) return 1.0;
  if (std::isnan(y) || ((x == 1 || x == -1) && std::isinf(y))) {
    return std::numeric_limits<double>::quiet_NaN();
  }
#if (defined(__MINGW64_VERSION_MAJOR) &&                              \
     (!defined(__MINGW64_VERSION_RC) || __MINGW64_VERSION_RC < 1)) || \
    defined(V8_OS_AIX)
  // MinGW64 and AIX have a custom implementation for pow.  This handles certain
  // special cases that are different.
  if ((x == 0.0 || std::isinf(x)) && y != 0.0 && std::isfinite(y)) {
    double f;
    double result = ((x == 0.0) ^ (y > 0)) ? V8_INFINITY : 0;
    /* retain sign if odd integer exponent */
    return ((std::modf(y, &f) == 0.0) && (static_cast<int64_t>(y) & 1))
               ? copysign(result, x)
               : result;
  }

  if (x == 2.0) {
    int y_int = static_cast<int>(y);
    if (y == y_int) {
      return std::ldexp(1.0, y_int);
    }
  }
#endif
  return std::pow(x, y);
}

// TODO(svenpanne) Clean up the whole power-of-2 mess.
inline int32_t WhichPowerOf2Abs(int32_t x) {
  return (x == kMinInt) ? 31 : WhichPowerOf2(Abs(x));
}


// Obtains the unsigned type corresponding to T
// available in C++11 as std::make_unsigned
template<typename T>
struct make_unsigned {
  typedef T type;
};


// Template specializations necessary to have make_unsigned work
template<> struct make_unsigned<int32_t> {
  typedef uint32_t type;
};


template<> struct make_unsigned<int64_t> {
  typedef uint64_t type;
};


// ----------------------------------------------------------------------------
// BitField is a help template for encoding and decode bitfield with
// unsigned content.

template<class T, int shift, int size, class U>
class BitFieldBase {
 public:
  // A type U mask of bit field.  To use all bits of a type U of x bits
  // in a bitfield without compiler warnings we have to compute 2^x
  // without using a shift count of x in the computation.
  static const U kOne = static_cast<U>(1U);
  static const U kMask = ((kOne << shift) << size) - (kOne << shift);
  static const U kShift = shift;
  static const U kSize = size;
  static const U kNext = kShift + kSize;

  // Value for the field with all bits set.
  static const T kMax = static_cast<T>((kOne << size) - 1);

  // Tells whether the provided value fits into the bit field.
  static bool is_valid(T value) {
    return (static_cast<U>(value) & ~static_cast<U>(kMax)) == 0;
  }

  // Returns a type U with the bit field value encoded.
  static U encode(T value) {
    DCHECK(is_valid(value));
    return static_cast<U>(value) << shift;
  }

  // Returns a type U with the bit field value updated.
  static U update(U previous, T value) {
    return (previous & ~kMask) | encode(value);
  }

  // Extracts the bit field from the value.
  static T decode(U value) {
    return static_cast<T>((value & kMask) >> shift);
  }

  STATIC_ASSERT((kNext - 1) / 8 < sizeof(U));
};

template <class T, int shift, int size>
class BitField8 : public BitFieldBase<T, shift, size, uint8_t> {};


template <class T, int shift, int size>
class BitField16 : public BitFieldBase<T, shift, size, uint16_t> {};


template<class T, int shift, int size>
class BitField : public BitFieldBase<T, shift, size, uint32_t> { };


template<class T, int shift, int size>
class BitField64 : public BitFieldBase<T, shift, size, uint64_t> { };


// ----------------------------------------------------------------------------
// BitSetComputer is a help template for encoding and decoding information for
// a variable number of items in an array.
//
// To encode boolean data in a smi array you would use:
// typedef BitSetComputer<bool, 1, kSmiValueSize, uint32_t> BoolComputer;
//
template <class T, int kBitsPerItem, int kBitsPerWord, class U>
class BitSetComputer {
 public:
  static const int kItemsPerWord = kBitsPerWord / kBitsPerItem;
  static const int kMask = (1 << kBitsPerItem) - 1;

  // The number of array elements required to embed T information for each item.
  static int word_count(int items) {
    if (items == 0) return 0;
    return (items - 1) / kItemsPerWord + 1;
  }

  // The array index to look at for item.
  static int index(int base_index, int item) {
    return base_index + item / kItemsPerWord;
  }

  // Extract T data for a given item from data.
  static T decode(U data, int item) {
    return static_cast<T>((data >> shift(item)) & kMask);
  }

  // Return the encoding for a store of value for item in previous.
  static U encode(U previous, int item, T value) {
    int shift_value = shift(item);
    int set_bits = (static_cast<int>(value) << shift_value);
    return (previous & ~(kMask << shift_value)) | set_bits;
  }

  static int shift(int item) { return (item % kItemsPerWord) * kBitsPerItem; }
};


// ----------------------------------------------------------------------------
// Hash function.

static const uint32_t kZeroHashSeed = 0;

// Thomas Wang, Integer Hash Functions.
// http://www.concentric.net/~Ttwang/tech/inthash.htm
inline uint32_t ComputeIntegerHash(uint32_t key, uint32_t seed) {
  uint32_t hash = key;
  hash = hash ^ seed;
  hash = ~hash + (hash << 15);  // hash = (hash << 15) - hash - 1;
  hash = hash ^ (hash >> 12);
  hash = hash + (hash << 2);
  hash = hash ^ (hash >> 4);
  hash = hash * 2057;  // hash = (hash + (hash << 3)) + (hash << 11);
  hash = hash ^ (hash >> 16);
  return hash & 0x3fffffff;
}


inline uint32_t ComputeLongHash(uint64_t key) {
  uint64_t hash = key;
  hash = ~hash + (hash << 18);  // hash = (hash << 18) - hash - 1;
  hash = hash ^ (hash >> 31);
  hash = hash * 21;  // hash = (hash + (hash << 2)) + (hash << 4);
  hash = hash ^ (hash >> 11);
  hash = hash + (hash << 6);
  hash = hash ^ (hash >> 22);
  return static_cast<uint32_t>(hash);
}


inline uint32_t ComputePointerHash(void* ptr) {
  return ComputeIntegerHash(
      static_cast<uint32_t>(reinterpret_cast<intptr_t>(ptr)),
      v8::internal::kZeroHashSeed);
}


// ----------------------------------------------------------------------------
// Generated memcpy/memmove

// Initializes the codegen support that depends on CPU features.
void init_memcopy_functions(Isolate* isolate);

#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X87)
// Limit below which the extra overhead of the MemCopy function is likely
// to outweigh the benefits of faster copying.
const int kMinComplexMemCopy = 64;

// Copy memory area. No restrictions.
V8_EXPORT_PRIVATE void MemMove(void* dest, const void* src, size_t size);
typedef void (*MemMoveFunction)(void* dest, const void* src, size_t size);

// Keep the distinction of "move" vs. "copy" for the benefit of other
// architectures.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  MemMove(dest, src, size);
}
#elif defined(V8_HOST_ARCH_ARM)
typedef void (*MemCopyUint8Function)(uint8_t* dest, const uint8_t* src,
                                     size_t size);
V8_EXPORT_PRIVATE extern MemCopyUint8Function memcopy_uint8_function;
V8_INLINE void MemCopyUint8Wrapper(uint8_t* dest, const uint8_t* src,
                                   size_t chars) {
  memcpy(dest, src, chars);
}
// For values < 16, the assembler function is slower than the inlined C code.
const int kMinComplexMemCopy = 16;
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  (*memcopy_uint8_function)(reinterpret_cast<uint8_t*>(dest),
                            reinterpret_cast<const uint8_t*>(src), size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}

typedef void (*MemCopyUint16Uint8Function)(uint16_t* dest, const uint8_t* src,
                                           size_t size);
extern MemCopyUint16Uint8Function memcopy_uint16_uint8_function;
void MemCopyUint16Uint8Wrapper(uint16_t* dest, const uint8_t* src,
                               size_t chars);
// For values < 12, the assembler function is slower than the inlined C code.
const int kMinComplexConvertMemCopy = 12;
V8_INLINE void MemCopyUint16Uint8(uint16_t* dest, const uint8_t* src,
                                  size_t size) {
  (*memcopy_uint16_uint8_function)(dest, src, size);
}
#elif defined(V8_HOST_ARCH_MIPS)
typedef void (*MemCopyUint8Function)(uint8_t* dest, const uint8_t* src,
                                     size_t size);
V8_EXPORT_PRIVATE extern MemCopyUint8Function memcopy_uint8_function;
V8_INLINE void MemCopyUint8Wrapper(uint8_t* dest, const uint8_t* src,
                                   size_t chars) {
  memcpy(dest, src, chars);
}
// For values < 16, the assembler function is slower than the inlined C code.
const int kMinComplexMemCopy = 16;
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  (*memcopy_uint8_function)(reinterpret_cast<uint8_t*>(dest),
                            reinterpret_cast<const uint8_t*>(src), size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}
#else
// Copy memory area to disjoint memory area.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  memcpy(dest, src, size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}
const int kMinComplexMemCopy = 16 * kPointerSize;
#endif  // V8_TARGET_ARCH_IA32


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
    DCHECK(!resource->is_reserved_);
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


// A pointer that can only be set once and doesn't allow NULL values.
template<typename T>
class SetOncePointer {
 public:
  SetOncePointer() : pointer_(NULL) { }

  bool is_set() const { return pointer_ != NULL; }

  T* get() const {
    DCHECK(pointer_ != NULL);
    return pointer_;
  }

  void set(T* value) {
    DCHECK(pointer_ == NULL && value != NULL);
    pointer_ = value;
  }

 private:
  T* pointer_;
};


template <typename T, int kSize>
class EmbeddedVector : public Vector<T> {
 public:
  EmbeddedVector() : Vector<T>(buffer_, kSize) { }

  explicit EmbeddedVector(T initial_value) : Vector<T>(buffer_, kSize) {
    for (int i = 0; i < kSize; ++i) {
      buffer_[i] = initial_value;
    }
  }

  // When copying, make underlying Vector to reference our buffer.
  EmbeddedVector(const EmbeddedVector& rhs)
      : Vector<T>(rhs) {
    MemCopy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    this->set_start(buffer_);
  }

  EmbeddedVector& operator=(const EmbeddedVector& rhs) {
    if (this == &rhs) return *this;
    Vector<T>::operator=(rhs);
    MemCopy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    this->set_start(buffer_);
    return *this;
  }

 private:
  T buffer_[kSize];
};

// Compare 8bit/16bit chars to 8bit/16bit chars.
template <typename lchar, typename rchar>
inline int CompareCharsUnsigned(const lchar* lhs, const rchar* rhs,
                                size_t chars) {
  const lchar* limit = lhs + chars;
  if (sizeof(*lhs) == sizeof(char) && sizeof(*rhs) == sizeof(char)) {
    // memcmp compares byte-by-byte, yielding wrong results for two-byte
    // strings on little-endian systems.
    return memcmp(lhs, rhs, chars);
  }
  while (lhs < limit) {
    int r = static_cast<int>(*lhs) - static_cast<int>(*rhs);
    if (r != 0) return r;
    ++lhs;
    ++rhs;
  }
  return 0;
}

template <typename lchar, typename rchar>
inline int CompareChars(const lchar* lhs, const rchar* rhs, size_t chars) {
  DCHECK(sizeof(lchar) <= 2);
  DCHECK(sizeof(rchar) <= 2);
  if (sizeof(lchar) == 1) {
    if (sizeof(rchar) == 1) {
      return CompareCharsUnsigned(reinterpret_cast<const uint8_t*>(lhs),
                                  reinterpret_cast<const uint8_t*>(rhs),
                                  chars);
    } else {
      return CompareCharsUnsigned(reinterpret_cast<const uint8_t*>(lhs),
                                  reinterpret_cast<const uint16_t*>(rhs),
                                  chars);
    }
  } else {
    if (sizeof(rchar) == 1) {
      return CompareCharsUnsigned(reinterpret_cast<const uint16_t*>(lhs),
                                  reinterpret_cast<const uint8_t*>(rhs),
                                  chars);
    } else {
      return CompareCharsUnsigned(reinterpret_cast<const uint16_t*>(lhs),
                                  reinterpret_cast<const uint16_t*>(rhs),
                                  chars);
    }
  }
}


// Calculate 10^exponent.
inline int TenToThe(int exponent) {
  DCHECK(exponent <= 9);
  DCHECK(exponent >= 1);
  int answer = 10;
  for (int i = 1; i < exponent; i++) answer *= 10;
  return answer;
}


template<typename ElementType, int NumElements>
class EmbeddedContainer {
 public:
  EmbeddedContainer() : elems_() { }

  int length() const { return NumElements; }
  const ElementType& operator[](int i) const {
    DCHECK(i < length());
    return elems_[i];
  }
  ElementType& operator[](int i) {
    DCHECK(i < length());
    return elems_[i];
  }

 private:
  ElementType elems_[NumElements];
};


template<typename ElementType>
class EmbeddedContainer<ElementType, 0> {
 public:
  int length() const { return 0; }
  const ElementType& operator[](int i) const {
    UNREACHABLE();
    static ElementType t = 0;
    return t;
  }
  ElementType& operator[](int i) {
    UNREACHABLE();
    static ElementType t = 0;
    return t;
  }
};


// Helper class for building result strings in a character buffer. The
// purpose of the class is to use safe operations that checks the
// buffer bounds on all operations in debug mode.
// This simple base class does not allow formatted output.
class SimpleStringBuilder {
 public:
  // Create a string builder with a buffer of the given size. The
  // buffer is allocated through NewArray<char> and must be
  // deallocated by the caller of Finalize().
  explicit SimpleStringBuilder(int size);

  SimpleStringBuilder(char* buffer, int size)
      : buffer_(buffer, size), position_(0) { }

  ~SimpleStringBuilder() { if (!is_finalized()) Finalize(); }

  int size() const { return buffer_.length(); }

  // Get the current position in the builder.
  int position() const {
    DCHECK(!is_finalized());
    return position_;
  }

  // Reset the position.
  void Reset() { position_ = 0; }

  // Add a single character to the builder. It is not allowed to add
  // 0-characters; use the Finalize() method to terminate the string
  // instead.
  void AddCharacter(char c) {
    DCHECK(c != '\0');
    DCHECK(!is_finalized() && position_ < buffer_.length());
    buffer_[position_++] = c;
  }

  // Add an entire string to the builder. Uses strlen() internally to
  // compute the length of the input string.
  void AddString(const char* s);

  // Add the first 'n' characters of the given 0-terminated string 's' to the
  // builder. The input string must have enough characters.
  void AddSubstring(const char* s, int n);

  // Add character padding to the builder. If count is non-positive,
  // nothing is added to the builder.
  void AddPadding(char c, int count);

  // Add the decimal representation of the value.
  void AddDecimalInteger(int value);

  // Finalize the string by 0-terminating it and returning the buffer.
  char* Finalize();

 protected:
  Vector<char> buffer_;
  int position_;

  bool is_finalized() const { return position_ < 0; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SimpleStringBuilder);
};


// A poor man's version of STL's bitset: A bit set of enums E (without explicit
// values), fitting into an integral type T.
template <class E, class T = int>
class EnumSet {
 public:
  explicit EnumSet(T bits = 0) : bits_(bits) {}
  bool IsEmpty() const { return bits_ == 0; }
  bool Contains(E element) const { return (bits_ & Mask(element)) != 0; }
  bool ContainsAnyOf(const EnumSet& set) const {
    return (bits_ & set.bits_) != 0;
  }
  void Add(E element) { bits_ |= Mask(element); }
  void Add(const EnumSet& set) { bits_ |= set.bits_; }
  void Remove(E element) { bits_ &= ~Mask(element); }
  void Remove(const EnumSet& set) { bits_ &= ~set.bits_; }
  void RemoveAll() { bits_ = 0; }
  void Intersect(const EnumSet& set) { bits_ &= set.bits_; }
  T ToIntegral() const { return bits_; }
  bool operator==(const EnumSet& set) { return bits_ == set.bits_; }
  bool operator!=(const EnumSet& set) { return bits_ != set.bits_; }
  EnumSet<E, T> operator|(const EnumSet& set) const {
    return EnumSet<E, T>(bits_ | set.bits_);
  }

 private:
  T Mask(E element) const {
    // The strange typing in DCHECK is necessary to avoid stupid warnings, see:
    // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43680
    DCHECK(static_cast<int>(element) < static_cast<int>(sizeof(T) * CHAR_BIT));
    return static_cast<T>(1) << element;
  }

  T bits_;
};

// Bit field extraction.
inline uint32_t unsigned_bitextract_32(int msb, int lsb, uint32_t x) {
  return (x >> lsb) & ((1 << (1 + msb - lsb)) - 1);
}

inline uint64_t unsigned_bitextract_64(int msb, int lsb, uint64_t x) {
  return (x >> lsb) & ((static_cast<uint64_t>(1) << (1 + msb - lsb)) - 1);
}

inline int32_t signed_bitextract_32(int msb, int lsb, int32_t x) {
  return (x << (31 - msb)) >> (lsb + 31 - msb);
}

inline int signed_bitextract_64(int msb, int lsb, int x) {
  // TODO(jbramley): This is broken for big bitfields.
  return (x << (63 - msb)) >> (lsb + 63 - msb);
}

// Check number width.
inline bool is_intn(int64_t x, unsigned n) {
  DCHECK((0 < n) && (n < 64));
  int64_t limit = static_cast<int64_t>(1) << (n - 1);
  return (-limit <= x) && (x < limit);
}

inline bool is_uintn(int64_t x, unsigned n) {
  DCHECK((0 < n) && (n < (sizeof(x) * kBitsPerByte)));
  return !(x >> n);
}

template <class T>
inline T truncate_to_intn(T x, unsigned n) {
  DCHECK((0 < n) && (n < (sizeof(x) * kBitsPerByte)));
  return (x & ((static_cast<T>(1) << n) - 1));
}

#define INT_1_TO_63_LIST(V)                                                    \
V(1)  V(2)  V(3)  V(4)  V(5)  V(6)  V(7)  V(8)                                 \
V(9)  V(10) V(11) V(12) V(13) V(14) V(15) V(16)                                \
V(17) V(18) V(19) V(20) V(21) V(22) V(23) V(24)                                \
V(25) V(26) V(27) V(28) V(29) V(30) V(31) V(32)                                \
V(33) V(34) V(35) V(36) V(37) V(38) V(39) V(40)                                \
V(41) V(42) V(43) V(44) V(45) V(46) V(47) V(48)                                \
V(49) V(50) V(51) V(52) V(53) V(54) V(55) V(56)                                \
V(57) V(58) V(59) V(60) V(61) V(62) V(63)

#define DECLARE_IS_INT_N(N)                                                    \
inline bool is_int##N(int64_t x) { return is_intn(x, N); }
#define DECLARE_IS_UINT_N(N)                                                   \
template <class T>                                                             \
inline bool is_uint##N(T x) { return is_uintn(x, N); }
#define DECLARE_TRUNCATE_TO_INT_N(N)                                           \
template <class T>                                                             \
inline T truncate_to_int##N(T x) { return truncate_to_intn(x, N); }
INT_1_TO_63_LIST(DECLARE_IS_INT_N)
INT_1_TO_63_LIST(DECLARE_IS_UINT_N)
INT_1_TO_63_LIST(DECLARE_TRUNCATE_TO_INT_N)
#undef DECLARE_IS_INT_N
#undef DECLARE_IS_UINT_N
#undef DECLARE_TRUNCATE_TO_INT_N

class TypeFeedbackId {
 public:
  explicit TypeFeedbackId(int id) : id_(id) { }
  int ToInt() const { return id_; }

  static TypeFeedbackId None() { return TypeFeedbackId(kNoneId); }
  bool IsNone() const { return id_ == kNoneId; }

 private:
  static const int kNoneId = -1;

  int id_;
};

inline bool operator<(TypeFeedbackId lhs, TypeFeedbackId rhs) {
  return lhs.ToInt() < rhs.ToInt();
}
inline bool operator>(TypeFeedbackId lhs, TypeFeedbackId rhs) {
  return lhs.ToInt() > rhs.ToInt();
}


class FeedbackVectorSlot {
 public:
  FeedbackVectorSlot() : id_(kInvalidSlot) {}
  explicit FeedbackVectorSlot(int id) : id_(id) {}

  int ToInt() const { return id_; }

  static FeedbackVectorSlot Invalid() { return FeedbackVectorSlot(); }
  bool IsInvalid() const { return id_ == kInvalidSlot; }

  bool operator==(FeedbackVectorSlot that) const {
    return this->id_ == that.id_;
  }
  bool operator!=(FeedbackVectorSlot that) const { return !(*this == that); }

  friend size_t hash_value(FeedbackVectorSlot slot) { return slot.ToInt(); }
  friend std::ostream& operator<<(std::ostream& os, FeedbackVectorSlot);

 private:
  static const int kInvalidSlot = -1;

  int id_;
};


class BailoutId {
 public:
  explicit BailoutId(int id) : id_(id) { }
  int ToInt() const { return id_; }

  static BailoutId None() { return BailoutId(kNoneId); }
  static BailoutId ScriptContext() { return BailoutId(kScriptContextId); }
  static BailoutId FunctionContext() { return BailoutId(kFunctionContextId); }
  static BailoutId FunctionEntry() { return BailoutId(kFunctionEntryId); }
  static BailoutId Declarations() { return BailoutId(kDeclarationsId); }
  static BailoutId FirstUsable() { return BailoutId(kFirstUsableId); }
  static BailoutId StubEntry() { return BailoutId(kStubEntryId); }

  bool IsNone() const { return id_ == kNoneId; }
  bool operator==(const BailoutId& other) const { return id_ == other.id_; }
  bool operator!=(const BailoutId& other) const { return id_ != other.id_; }
  friend size_t hash_value(BailoutId);
  friend std::ostream& operator<<(std::ostream&, BailoutId);

 private:
  static const int kNoneId = -1;

  // Using 0 could disguise errors.
  static const int kScriptContextId = 1;
  static const int kFunctionContextId = 2;
  static const int kFunctionEntryId = 3;

  // This AST id identifies the point after the declarations have been visited.
  // We need it to capture the environment effects of declarations that emit
  // code (function declarations).
  static const int kDeclarationsId = 4;

  // Every FunctionState starts with this id.
  static const int kFirstUsableId = 5;

  // Every compiled stub starts with this id.
  static const int kStubEntryId = 6;

  int id_;
};

class TokenDispenserForFinally {
 public:
  int GetBreakContinueToken() { return next_token_++; }
  static const int kFallThroughToken = 0;
  static const int kThrowToken = 1;
  static const int kReturnToken = 2;

  static const int kFirstBreakContinueToken = 3;
  static const int kInvalidToken = -1;

 private:
  int next_token_ = kFirstBreakContinueToken;
};

// ----------------------------------------------------------------------------
// I/O support.

// Our version of printf().
void PRINTF_FORMAT(1, 2) PrintF(const char* format, ...);
void PRINTF_FORMAT(2, 3) PrintF(FILE* out, const char* format, ...);

// Prepends the current process ID to the output.
void PRINTF_FORMAT(1, 2) PrintPID(const char* format, ...);

// Prepends the current process ID and given isolate pointer to the output.
void PRINTF_FORMAT(2, 3) PrintIsolate(void* isolate, const char* format, ...);

// Safe formatting print. Ensures that str is always null-terminated.
// Returns the number of chars written, or -1 if output was truncated.
int PRINTF_FORMAT(2, 3) SNPrintF(Vector<char> str, const char* format, ...);
int PRINTF_FORMAT(2, 0)
    VSNPrintF(Vector<char> str, const char* format, va_list args);

void StrNCpy(Vector<char> dest, const char* src, size_t n);

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
// Memory

// Copies words from |src| to |dst|. The data spans must not overlap.
template <typename T>
inline void CopyWords(T* dst, const T* src, size_t num_words) {
  STATIC_ASSERT(sizeof(T) == kPointerSize);
  // TODO(mvstanton): disabled because mac builds are bogus failing on this
  // assert. They are doing a signed comparison. Investigate in
  // the morning.
  // DCHECK(Min(dst, const_cast<T*>(src)) + num_words <=
  //       Max(dst, const_cast<T*>(src)));
  DCHECK(num_words > 0);

  // Use block copying MemCopy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const size_t kBlockCopyLimit = 16;

  if (num_words < kBlockCopyLimit) {
    do {
      num_words--;
      *dst++ = *src++;
    } while (num_words > 0);
  } else {
    MemCopy(dst, src, num_words * kPointerSize);
  }
}


// Copies words from |src| to |dst|. No restrictions.
template <typename T>
inline void MoveWords(T* dst, const T* src, size_t num_words) {
  STATIC_ASSERT(sizeof(T) == kPointerSize);
  DCHECK(num_words > 0);

  // Use block copying MemCopy if the segment we're copying is
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
    MemMove(dst, src, num_words * kPointerSize);
  }
}


// Copies data from |src| to |dst|.  The data spans must not overlap.
template <typename T>
inline void CopyBytes(T* dst, const T* src, size_t num_bytes) {
  STATIC_ASSERT(sizeof(T) == 1);
  DCHECK(Min(dst, const_cast<T*>(src)) + num_bytes <=
         Max(dst, const_cast<T*>(src)));
  if (num_bytes == 0) return;

  // Use block copying MemCopy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const int kBlockCopyLimit = kMinComplexMemCopy;

  if (num_bytes < static_cast<size_t>(kBlockCopyLimit)) {
    do {
      num_bytes--;
      *dst++ = *src++;
    } while (num_bytes > 0);
  } else {
    MemCopy(dst, src, num_bytes);
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
#if V8_HOST_ARCH_32_BIT
#define STOS "addr32 stosl"
#else
#define STOS "stosq"
#endif
#endif

#if defined(MEMORY_SANITIZER)
  // MemorySanitizer does not understand inline assembly.
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
INLINE(static void CopyCharsUnsigned(sinkchar* dest, const sourcechar* src,
                                     size_t chars));
#if defined(V8_HOST_ARCH_ARM)
INLINE(void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src,
                              size_t chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src,
                              size_t chars));
#elif defined(V8_HOST_ARCH_MIPS)
INLINE(void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src,
                              size_t chars));
#elif defined(V8_HOST_ARCH_PPC) || defined(V8_HOST_ARCH_S390)
INLINE(void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src,
                              size_t chars));
#endif

// Copy from 8bit/16bit chars to 8bit/16bit chars.
template <typename sourcechar, typename sinkchar>
INLINE(void CopyChars(sinkchar* dest, const sourcechar* src, size_t chars));

template <typename sourcechar, typename sinkchar>
void CopyChars(sinkchar* dest, const sourcechar* src, size_t chars) {
  DCHECK(sizeof(sourcechar) <= 2);
  DCHECK(sizeof(sinkchar) <= 2);
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
void CopyCharsUnsigned(sinkchar* dest, const sourcechar* src, size_t chars) {
  sinkchar* limit = dest + chars;
  if ((sizeof(*dest) == sizeof(*src)) &&
      (chars >= static_cast<int>(kMinComplexMemCopy / sizeof(*dest)))) {
    MemCopy(dest, src, chars * sizeof(*dest));
  } else {
    while (dest < limit) *dest++ = static_cast<sinkchar>(*src++);
  }
}


#if defined(V8_HOST_ARCH_ARM)
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars) {
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
      MemCopy(dest, src, chars);
      break;
  }
}


void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src, size_t chars) {
  if (chars >= static_cast<size_t>(kMinComplexConvertMemCopy)) {
    MemCopyUint16Uint8(dest, src, chars);
  } else {
    MemCopyUint16Uint8Wrapper(dest, src, chars);
  }
}


void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, size_t chars) {
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
      MemCopy(dest, src, chars * sizeof(*dest));
      break;
  }
}


#elif defined(V8_HOST_ARCH_MIPS)
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars) {
  if (chars < kMinComplexMemCopy) {
    memcpy(dest, src, chars);
  } else {
    MemCopy(dest, src, chars);
  }
}

void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, size_t chars) {
  if (chars < kMinComplexMemCopy) {
    memcpy(dest, src, chars * sizeof(*dest));
  } else {
    MemCopy(dest, src, chars * sizeof(*dest));
  }
}
#elif defined(V8_HOST_ARCH_PPC) || defined(V8_HOST_ARCH_S390)
#define CASE(n)           \
  case n:                 \
    memcpy(dest, src, n); \
    break
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
      CASE(2);
      CASE(3);
      CASE(4);
      CASE(5);
      CASE(6);
      CASE(7);
      CASE(8);
      CASE(9);
      CASE(10);
      CASE(11);
      CASE(12);
      CASE(13);
      CASE(14);
      CASE(15);
      CASE(16);
      CASE(17);
      CASE(18);
      CASE(19);
      CASE(20);
      CASE(21);
      CASE(22);
      CASE(23);
      CASE(24);
      CASE(25);
      CASE(26);
      CASE(27);
      CASE(28);
      CASE(29);
      CASE(30);
      CASE(31);
      CASE(32);
      CASE(33);
      CASE(34);
      CASE(35);
      CASE(36);
      CASE(37);
      CASE(38);
      CASE(39);
      CASE(40);
      CASE(41);
      CASE(42);
      CASE(43);
      CASE(44);
      CASE(45);
      CASE(46);
      CASE(47);
      CASE(48);
      CASE(49);
      CASE(50);
      CASE(51);
      CASE(52);
      CASE(53);
      CASE(54);
      CASE(55);
      CASE(56);
      CASE(57);
      CASE(58);
      CASE(59);
      CASE(60);
      CASE(61);
      CASE(62);
      CASE(63);
      CASE(64);
    default:
      memcpy(dest, src, chars);
      break;
  }
}
#undef CASE

#define CASE(n)               \
  case n:                     \
    memcpy(dest, src, n * 2); \
    break
void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, size_t chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
      CASE(2);
      CASE(3);
      CASE(4);
      CASE(5);
      CASE(6);
      CASE(7);
      CASE(8);
      CASE(9);
      CASE(10);
      CASE(11);
      CASE(12);
      CASE(13);
      CASE(14);
      CASE(15);
      CASE(16);
      CASE(17);
      CASE(18);
      CASE(19);
      CASE(20);
      CASE(21);
      CASE(22);
      CASE(23);
      CASE(24);
      CASE(25);
      CASE(26);
      CASE(27);
      CASE(28);
      CASE(29);
      CASE(30);
      CASE(31);
      CASE(32);
    default:
      memcpy(dest, src, chars * 2);
      break;
  }
}
#undef CASE
#endif


class StringBuilder : public SimpleStringBuilder {
 public:
  explicit StringBuilder(int size) : SimpleStringBuilder(size) { }
  StringBuilder(char* buffer, int size) : SimpleStringBuilder(buffer, size) { }

  // Add formatted contents to the builder just like printf().
  void PRINTF_FORMAT(2, 3) AddFormatted(const char* format, ...);

  // Add formatted contents like printf based on a va_list.
  void PRINTF_FORMAT(2, 0) AddFormattedList(const char* format, va_list list);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringBuilder);
};


bool DoubleToBoolean(double d);

template <typename Stream>
bool StringToArrayIndex(Stream* stream, uint32_t* index) {
  uint16_t ch = stream->GetNext();

  // If the string begins with a '0' character, it must only consist
  // of it to be a legal array index.
  if (ch == '0') {
    *index = 0;
    return !stream->HasMore();
  }

  // Convert string to uint32 array index; character by character.
  int d = ch - '0';
  if (d < 0 || d > 9) return false;
  uint32_t result = d;
  while (stream->HasMore()) {
    d = stream->GetNext() - '0';
    if (d < 0 || d > 9) return false;
    // Check that the new result is below the 32 bit limit.
    if (result > 429496729U - ((d + 3) >> 3)) return false;
    result = (result * 10) + d;
  }

  *index = result;
  return true;
}


// Returns current value of top of the stack. Works correctly with ASAN.
DISABLE_ASAN
inline uintptr_t GetCurrentStackPosition() {
  // Takes the address of the limit variable in order to find out where
  // the top of stack is right now.
  uintptr_t limit = reinterpret_cast<uintptr_t>(&limit);
  return limit;
}

template <typename V>
static inline V ReadUnalignedValue(const void* p) {
#if !(V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM)
  return *reinterpret_cast<const V*>(p);
#else   // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
  V r;
  memmove(&r, p, sizeof(V));
  return r;
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
}

template <typename V>
static inline void WriteUnalignedValue(void* p, V value) {
#if !(V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM)
  *(reinterpret_cast<V*>(p)) = value;
#else   // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
  memmove(p, &value, sizeof(V));
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
}

static inline double ReadFloatValue(const void* p) {
  return ReadUnalignedValue<float>(p);
}

static inline double ReadDoubleValue(const void* p) {
  return ReadUnalignedValue<double>(p);
}

static inline void WriteDoubleValue(void* p, double value) {
  WriteUnalignedValue(p, value);
}

static inline uint16_t ReadUnalignedUInt16(const void* p) {
  return ReadUnalignedValue<uint16_t>(p);
}

static inline void WriteUnalignedUInt16(void* p, uint16_t value) {
  WriteUnalignedValue(p, value);
}

static inline uint32_t ReadUnalignedUInt32(const void* p) {
  return ReadUnalignedValue<uint32_t>(p);
}

static inline void WriteUnalignedUInt32(void* p, uint32_t value) {
  WriteUnalignedValue(p, value);
}

template <typename V>
static inline V ReadLittleEndianValue(const void* p) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  return ReadUnalignedValue<V>(p);
#elif defined(V8_TARGET_BIG_ENDIAN)
  V ret = 0;
  const byte* src = reinterpret_cast<const byte*>(p);
  byte* dst = reinterpret_cast<byte*>(&ret);
  for (size_t i = 0; i < sizeof(V); i++) {
    dst[i] = src[sizeof(V) - i - 1];
  }
  return ret;
#endif  // V8_TARGET_LITTLE_ENDIAN
}

template <typename V>
static inline void WriteLittleEndianValue(void* p, V value) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  WriteUnalignedValue<V>(p, value);
#elif defined(V8_TARGET_BIG_ENDIAN)
  byte* src = reinterpret_cast<byte*>(&value);
  byte* dst = reinterpret_cast<byte*>(p);
  for (size_t i = 0; i < sizeof(V); i++) {
    dst[i] = src[sizeof(V) - i - 1];
  }
#endif  // V8_TARGET_LITTLE_ENDIAN
}
}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_H_
