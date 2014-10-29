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


// Same as strcmp, but can handle NULL arguments.
inline bool CStringEquals(const char* s1, const char* s2) {
  return (s1 == s2) || (s1 != NULL && s2 != NULL && strcmp(s1, s2) == 0);
}


// X must be a power of 2.  Returns the number of trailing zeros.
inline int WhichPowerOf2(uint32_t x) {
  DCHECK(base::bits::IsPowerOfTwo32(x));
  int bits = 0;
#ifdef DEBUG
  int original_x = x;
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
  DCHECK_EQ(1 << bits, original_x);
  return bits;
  return 0;
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


// Returns the absolute value of its argument.
template <typename T>
T Abs(T a) {
  return a < 0 ? -a : a;
}


// Floor(-0.0) == 0.0
inline double Floor(double x) {
#ifdef _MSC_VER
  if (x == 0) return x;  // Fix for issue 3477.
#endif
  return std::floor(x);
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
  static const T kMax = static_cast<T>((1U << size) - 1);

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
};


template<class T, int shift, int size>
class BitField : public BitFieldBase<T, shift, size, uint32_t> { };


template<class T, int shift, int size>
class BitField64 : public BitFieldBase<T, shift, size, uint64_t> { };


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
  return hash;
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

// Initializes the codegen support that depends on CPU features. This is
// called after CPU initialization.
void init_memcopy_functions();

#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X87)
// Limit below which the extra overhead of the MemCopy function is likely
// to outweigh the benefits of faster copying.
const int kMinComplexMemCopy = 64;

// Copy memory area. No restrictions.
void MemMove(void* dest, const void* src, size_t size);
typedef void (*MemMoveFunction)(void* dest, const void* src, size_t size);

// Keep the distinction of "move" vs. "copy" for the benefit of other
// architectures.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  MemMove(dest, src, size);
}
#elif defined(V8_HOST_ARCH_ARM)
typedef void (*MemCopyUint8Function)(uint8_t* dest, const uint8_t* src,
                                     size_t size);
extern MemCopyUint8Function memcopy_uint8_function;
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
V8_INLINE void MemMove(void* dest, const void* src, size_t size) {
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
extern MemCopyUint8Function memcopy_uint8_function;
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
V8_INLINE void MemMove(void* dest, const void* src, size_t size) {
  memmove(dest, src, size);
}
#else
// Copy memory area to disjoint memory area.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  memcpy(dest, src, size);
}
V8_INLINE void MemMove(void* dest, const void* src, size_t size) {
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


/*
 * A class that collects values into a backing store.
 * Specialized versions of the class can allow access to the backing store
 * in different ways.
 * There is no guarantee that the backing store is contiguous (and, as a
 * consequence, no guarantees that consecutively added elements are adjacent
 * in memory). The collector may move elements unless it has guaranteed not
 * to.
 */
template <typename T, int growth_factor = 2, int max_growth = 1 * MB>
class Collector {
 public:
  explicit Collector(int initial_capacity = kMinCapacity)
      : index_(0), size_(0) {
    current_chunk_ = Vector<T>::New(initial_capacity);
  }

  virtual ~Collector() {
    // Free backing store (in reverse allocation order).
    current_chunk_.Dispose();
    for (int i = chunks_.length() - 1; i >= 0; i--) {
      chunks_.at(i).Dispose();
    }
  }

  // Add a single element.
  inline void Add(T value) {
    if (index_ >= current_chunk_.length()) {
      Grow(1);
    }
    current_chunk_[index_] = value;
    index_++;
    size_++;
  }

  // Add a block of contiguous elements and return a Vector backed by the
  // memory area.
  // A basic Collector will keep this vector valid as long as the Collector
  // is alive.
  inline Vector<T> AddBlock(int size, T initial_value) {
    DCHECK(size > 0);
    if (size > current_chunk_.length() - index_) {
      Grow(size);
    }
    T* position = current_chunk_.start() + index_;
    index_ += size;
    size_ += size;
    for (int i = 0; i < size; i++) {
      position[i] = initial_value;
    }
    return Vector<T>(position, size);
  }


  // Add a contiguous block of elements and return a vector backed
  // by the added block.
  // A basic Collector will keep this vector valid as long as the Collector
  // is alive.
  inline Vector<T> AddBlock(Vector<const T> source) {
    if (source.length() > current_chunk_.length() - index_) {
      Grow(source.length());
    }
    T* position = current_chunk_.start() + index_;
    index_ += source.length();
    size_ += source.length();
    for (int i = 0; i < source.length(); i++) {
      position[i] = source[i];
    }
    return Vector<T>(position, source.length());
  }


  // Write the contents of the collector into the provided vector.
  void WriteTo(Vector<T> destination) {
    DCHECK(size_ <= destination.length());
    int position = 0;
    for (int i = 0; i < chunks_.length(); i++) {
      Vector<T> chunk = chunks_.at(i);
      for (int j = 0; j < chunk.length(); j++) {
        destination[position] = chunk[j];
        position++;
      }
    }
    for (int i = 0; i < index_; i++) {
      destination[position] = current_chunk_[i];
      position++;
    }
  }

  // Allocate a single contiguous vector, copy all the collected
  // elements to the vector, and return it.
  // The caller is responsible for freeing the memory of the returned
  // vector (e.g., using Vector::Dispose).
  Vector<T> ToVector() {
    Vector<T> new_store = Vector<T>::New(size_);
    WriteTo(new_store);
    return new_store;
  }

  // Resets the collector to be empty.
  virtual void Reset();

  // Total number of elements added to collector so far.
  inline int size() { return size_; }

 protected:
  static const int kMinCapacity = 16;
  List<Vector<T> > chunks_;
  Vector<T> current_chunk_;  // Block of memory currently being written into.
  int index_;  // Current index in current chunk.
  int size_;  // Total number of elements in collector.

  // Creates a new current chunk, and stores the old chunk in the chunks_ list.
  void Grow(int min_capacity) {
    DCHECK(growth_factor > 1);
    int new_capacity;
    int current_length = current_chunk_.length();
    if (current_length < kMinCapacity) {
      // The collector started out as empty.
      new_capacity = min_capacity * growth_factor;
      if (new_capacity < kMinCapacity) new_capacity = kMinCapacity;
    } else {
      int growth = current_length * (growth_factor - 1);
      if (growth > max_growth) {
        growth = max_growth;
      }
      new_capacity = current_length + growth;
      if (new_capacity < min_capacity) {
        new_capacity = min_capacity + growth;
      }
    }
    NewChunk(new_capacity);
    DCHECK(index_ + min_capacity <= current_chunk_.length());
  }

  // Before replacing the current chunk, give a subclass the option to move
  // some of the current data into the new chunk. The function may update
  // the current index_ value to represent data no longer in the current chunk.
  // Returns the initial index of the new chunk (after copied data).
  virtual void NewChunk(int new_capacity)  {
    Vector<T> new_chunk = Vector<T>::New(new_capacity);
    if (index_ > 0) {
      chunks_.Add(current_chunk_.SubVector(0, index_));
    } else {
      current_chunk_.Dispose();
    }
    current_chunk_ = new_chunk;
    index_ = 0;
  }
};


/*
 * A collector that allows sequences of values to be guaranteed to
 * stay consecutive.
 * If the backing store grows while a sequence is active, the current
 * sequence might be moved, but after the sequence is ended, it will
 * not move again.
 * NOTICE: Blocks allocated using Collector::AddBlock(int) can move
 * as well, if inside an active sequence where another element is added.
 */
template <typename T, int growth_factor = 2, int max_growth = 1 * MB>
class SequenceCollector : public Collector<T, growth_factor, max_growth> {
 public:
  explicit SequenceCollector(int initial_capacity)
      : Collector<T, growth_factor, max_growth>(initial_capacity),
        sequence_start_(kNoSequence) { }

  virtual ~SequenceCollector() {}

  void StartSequence() {
    DCHECK(sequence_start_ == kNoSequence);
    sequence_start_ = this->index_;
  }

  Vector<T> EndSequence() {
    DCHECK(sequence_start_ != kNoSequence);
    int sequence_start = sequence_start_;
    sequence_start_ = kNoSequence;
    if (sequence_start == this->index_) return Vector<T>();
    return this->current_chunk_.SubVector(sequence_start, this->index_);
  }

  // Drops the currently added sequence, and all collected elements in it.
  void DropSequence() {
    DCHECK(sequence_start_ != kNoSequence);
    int sequence_length = this->index_ - sequence_start_;
    this->index_ = sequence_start_;
    this->size_ -= sequence_length;
    sequence_start_ = kNoSequence;
  }

  virtual void Reset() {
    sequence_start_ = kNoSequence;
    this->Collector<T, growth_factor, max_growth>::Reset();
  }

 private:
  static const int kNoSequence = -1;
  int sequence_start_;

  // Move the currently active sequence to the new chunk.
  virtual void NewChunk(int new_capacity) {
    if (sequence_start_ == kNoSequence) {
      // Fall back on default behavior if no sequence has been started.
      this->Collector<T, growth_factor, max_growth>::NewChunk(new_capacity);
      return;
    }
    int sequence_length = this->index_ - sequence_start_;
    Vector<T> new_chunk = Vector<T>::New(sequence_length + new_capacity);
    DCHECK(sequence_length < new_chunk.length());
    for (int i = 0; i < sequence_length; i++) {
      new_chunk[i] = this->current_chunk_[sequence_start_ + i];
    }
    if (sequence_start_ > 0) {
      this->chunks_.Add(this->current_chunk_.SubVector(0, sequence_start_));
    } else {
      this->current_chunk_.Dispose();
    }
    this->current_chunk_ = new_chunk;
    this->index_ = sequence_length;
    sequence_start_ = 0;
  }
};


// Compare 8bit/16bit chars to 8bit/16bit chars.
template <typename lchar, typename rchar>
inline int CompareCharsUnsigned(const lchar* lhs,
                                const rchar* rhs,
                                int chars) {
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

template<typename lchar, typename rchar>
inline int CompareChars(const lchar* lhs, const rchar* rhs, int chars) {
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

  // Add the first 'n' characters of the given string 's' to the
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


class BailoutId {
 public:
  explicit BailoutId(int id) : id_(id) { }
  int ToInt() const { return id_; }

  static BailoutId None() { return BailoutId(kNoneId); }
  static BailoutId FunctionEntry() { return BailoutId(kFunctionEntryId); }
  static BailoutId Declarations() { return BailoutId(kDeclarationsId); }
  static BailoutId FirstUsable() { return BailoutId(kFirstUsableId); }
  static BailoutId StubEntry() { return BailoutId(kStubEntryId); }

  bool IsNone() const { return id_ == kNoneId; }
  bool operator==(const BailoutId& other) const { return id_ == other.id_; }
  bool operator!=(const BailoutId& other) const { return id_ != other.id_; }

 private:
  static const int kNoneId = -1;

  // Using 0 could disguise errors.
  static const int kFunctionEntryId = 2;

  // This AST id identifies the point after the declarations have been visited.
  // We need it to capture the environment effects of declarations that emit
  // code (function declarations).
  static const int kDeclarationsId = 3;

  // Every FunctionState starts with this id.
  static const int kFirstUsableId = 4;

  // Every compiled stub starts with this id.
  static const int kStubEntryId = 5;

  int id_;
};


template <class C>
class ContainerPointerWrapper {
 public:
  typedef typename C::iterator iterator;
  typedef typename C::reverse_iterator reverse_iterator;
  explicit ContainerPointerWrapper(C* container) : container_(container) {}
  iterator begin() { return container_->begin(); }
  iterator end() { return container_->end(); }
  reverse_iterator rbegin() { return container_->rbegin(); }
  reverse_iterator rend() { return container_->rend(); }
 private:
  C* container_;
};


// ----------------------------------------------------------------------------
// I/O support.

#if __GNUC__ >= 4
// On gcc we can ask the compiler to check the types of %d-style format
// specifiers and their associated arguments.  TODO(erikcorry) fix this
// so it works on MacOSX.
#if defined(__MACH__) && defined(__APPLE__)
#define PRINTF_CHECKING
#define FPRINTF_CHECKING
#define PRINTF_METHOD_CHECKING
#define FPRINTF_METHOD_CHECKING
#else  // MacOsX.
#define PRINTF_CHECKING __attribute__ ((format (printf, 1, 2)))
#define FPRINTF_CHECKING __attribute__ ((format (printf, 2, 3)))
#define PRINTF_METHOD_CHECKING __attribute__ ((format (printf, 2, 3)))
#define FPRINTF_METHOD_CHECKING __attribute__ ((format (printf, 3, 4)))
#endif
#else
#define PRINTF_CHECKING
#define FPRINTF_CHECKING
#define PRINTF_METHOD_CHECKING
#define FPRINTF_METHOD_CHECKING
#endif

// Our version of printf().
void PRINTF_CHECKING PrintF(const char* format, ...);
void FPRINTF_CHECKING PrintF(FILE* out, const char* format, ...);

// Prepends the current process ID to the output.
void PRINTF_CHECKING PrintPID(const char* format, ...);

// Safe formatting print. Ensures that str is always null-terminated.
// Returns the number of chars written, or -1 if output was truncated.
int FPRINTF_CHECKING SNPrintF(Vector<char> str, const char* format, ...);
int VSNPrintF(Vector<char> str, const char* format, va_list args);

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
#if defined(__native_client__)
  // This STOS sequence does not validate for x86_64 Native Client.
  // Here we #undef STOS to force use of the slower C version.
  // TODO(bradchen): Profile V8 and implement a faster REP STOS
  // here if the profile indicates it matters.
#undef STOS
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
INLINE(static void CopyCharsUnsigned(sinkchar* dest,
                                     const sourcechar* src,
                                     int chars));
#if defined(V8_HOST_ARCH_ARM)
INLINE(void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, int chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src, int chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, int chars));
#elif defined(V8_HOST_ARCH_MIPS)
INLINE(void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, int chars));
INLINE(void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, int chars));
#endif

// Copy from 8bit/16bit chars to 8bit/16bit chars.
template <typename sourcechar, typename sinkchar>
INLINE(void CopyChars(sinkchar* dest, const sourcechar* src, int chars));

template<typename sourcechar, typename sinkchar>
void CopyChars(sinkchar* dest, const sourcechar* src, int chars) {
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
void CopyCharsUnsigned(sinkchar* dest, const sourcechar* src, int chars) {
  sinkchar* limit = dest + chars;
  if ((sizeof(*dest) == sizeof(*src)) &&
      (chars >= static_cast<int>(kMinComplexMemCopy / sizeof(*dest)))) {
    MemCopy(dest, src, chars * sizeof(*dest));
  } else {
    while (dest < limit) *dest++ = static_cast<sinkchar>(*src++);
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
      MemCopy(dest, src, chars);
      break;
  }
}


void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src, int chars) {
  if (chars >= kMinComplexConvertMemCopy) {
    MemCopyUint16Uint8(dest, src, chars);
  } else {
    MemCopyUint16Uint8Wrapper(dest, src, chars);
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
      MemCopy(dest, src, chars * sizeof(*dest));
      break;
  }
}


#elif defined(V8_HOST_ARCH_MIPS)
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, int chars) {
  if (chars < kMinComplexMemCopy) {
    memcpy(dest, src, chars);
  } else {
    MemCopy(dest, src, chars);
  }
}

void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, int chars) {
  if (chars < kMinComplexMemCopy) {
    memcpy(dest, src, chars * sizeof(*dest));
  } else {
    MemCopy(dest, src, chars * sizeof(*dest));
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
    if (result > 429496729U - ((d > 5) ? 1 : 0)) return false;
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

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_H_
