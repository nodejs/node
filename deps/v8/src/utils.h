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
#include <string.h>

#include "globals.h"
#include "checks.h"
#include "allocation.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// General helper functions

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)

// Returns true iff x is a power of 2 (or zero). Cannot be used with the
// maximally negative value of the type T (the -1 overflows).
template <typename T>
static inline bool IsPowerOf2(T x) {
  return IS_POWER_OF_TWO(x);
}


// X must be a power of 2.  Returns the number of trailing zeros.
template <typename T>
static inline int WhichPowerOf2(T x) {
  ASSERT(IsPowerOf2(x));
  ASSERT(x != 0);
  if (x < 0) return 31;
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
  ASSERT_EQ(1 << bits, original_x);
  return bits;
  return 0;
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
// Implementation is from "Hacker's Delight" by Henry S. Warren, Jr.,
// figure 3-3, page 48, where the function is called clp2.
static inline uint32_t RoundUpToPowerOf2(uint32_t x) {
  ASSERT(x <= 0x80000000u);
  x = x - 1;
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x + 1;
}



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

  // Value for the field with all bits set.
  static T max() {
    return decode(mask());
  }
};


// ----------------------------------------------------------------------------
// Hash function.

// Thomas Wang, Integer Hash Functions.
// http://www.concentric.net/~Ttwang/tech/inthash.htm
static inline uint32_t ComputeIntegerHash(uint32_t key) {
  uint32_t hash = key;
  hash = ~hash + (hash << 15);  // hash = (hash << 15) - hash - 1;
  hash = hash ^ (hash >> 12);
  hash = hash + (hash << 2);
  hash = hash ^ (hash >> 4);
  hash = hash * 2057;  // hash = (hash + (hash << 3)) + (hash << 11);
  hash = hash ^ (hash >> 16);
  return hash;
}


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
    ASSERT(to <= length_);
    ASSERT(from < to);
    ASSERT(0 <= from);
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

  const T& at(int index) const { return operator[](index); }

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

  template<typename S>
  static Vector<T> cast(Vector<S> input) {
    return Vector<T>(reinterpret_cast<T*>(input.start()),
                     input.length() * sizeof(S) / sizeof(T));
  }

 protected:
  void set_start(T* start) { start_ = start; }

 private:
  T* start_;
  int length_;
};


// A pointer that can only be set once and doesn't allow NULL values.
template<typename T>
class SetOncePointer {
 public:
  SetOncePointer() : pointer_(NULL) { }

  bool is_set() const { return pointer_ != NULL; }

  T* get() const {
    ASSERT(pointer_ != NULL);
    return pointer_;
  }

  void set(T* value) {
    ASSERT(pointer_ == NULL && value != NULL);
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
    memcpy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    set_start(buffer_);
  }

  EmbeddedVector& operator=(const EmbeddedVector& rhs) {
    if (this == &rhs) return *this;
    Vector<T>::operator=(rhs);
    memcpy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    this->set_start(buffer_);
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

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedVector);
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
    if (initial_capacity < kMinCapacity) {
      initial_capacity = kMinCapacity;
    }
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
    ASSERT(size > 0);
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
    ASSERT(size_ <= destination.length());
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
  virtual void Reset() {
    for (int i = chunks_.length() - 1; i >= 0; i--) {
      chunks_.at(i).Dispose();
    }
    chunks_.Rewind(0);
    index_ = 0;
    size_ = 0;
  }

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
    ASSERT(growth_factor > 1);
    int growth = current_chunk_.length() * (growth_factor - 1);
    if (growth > max_growth) {
      growth = max_growth;
    }
    int new_capacity = current_chunk_.length() + growth;
    if (new_capacity < min_capacity) {
      new_capacity = min_capacity + growth;
    }
    Vector<T> new_chunk = Vector<T>::New(new_capacity);
    int new_index = PrepareGrow(new_chunk);
    if (index_ > 0) {
      chunks_.Add(current_chunk_.SubVector(0, index_));
    } else {
      // Can happen if the call to PrepareGrow moves everything into
      // the new chunk.
      current_chunk_.Dispose();
    }
    current_chunk_ = new_chunk;
    index_ = new_index;
    ASSERT(index_ + min_capacity <= current_chunk_.length());
  }

  // Before replacing the current chunk, give a subclass the option to move
  // some of the current data into the new chunk. The function may update
  // the current index_ value to represent data no longer in the current chunk.
  // Returns the initial index of the new chunk (after copied data).
  virtual int PrepareGrow(Vector<T> new_chunk)  {
    return 0;
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
    ASSERT(sequence_start_ == kNoSequence);
    sequence_start_ = this->index_;
  }

  Vector<T> EndSequence() {
    ASSERT(sequence_start_ != kNoSequence);
    int sequence_start = sequence_start_;
    sequence_start_ = kNoSequence;
    if (sequence_start == this->index_) return Vector<T>();
    return this->current_chunk_.SubVector(sequence_start, this->index_);
  }

  // Drops the currently added sequence, and all collected elements in it.
  void DropSequence() {
    ASSERT(sequence_start_ != kNoSequence);
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
  virtual int PrepareGrow(Vector<T> new_chunk) {
    if (sequence_start_ != kNoSequence) {
      int sequence_length = this->index_ - sequence_start_;
      // The new chunk is always larger than the current chunk, so there
      // is room for the copy.
      ASSERT(sequence_length < new_chunk.length());
      for (int i = 0; i < sequence_length; i++) {
        new_chunk[i] = this->current_chunk_[sequence_start_ + i];
      }
      this->index_ = sequence_start_;
      sequence_start_ = 0;
      return sequence_length;
    }
    return 0;
  }
};


// Compare ASCII/16bit chars to ASCII/16bit chars.
template <typename lchar, typename rchar>
static inline int CompareChars(const lchar* lhs, const rchar* rhs, int chars) {
  const lchar* limit = lhs + chars;
#ifdef V8_HOST_CAN_READ_UNALIGNED
  if (sizeof(*lhs) == sizeof(*rhs)) {
    // Number of characters in a uintptr_t.
    static const int kStepSize = sizeof(uintptr_t) / sizeof(*lhs);  // NOLINT
    while (lhs <= limit - kStepSize) {
      if (*reinterpret_cast<const uintptr_t*>(lhs) !=
          *reinterpret_cast<const uintptr_t*>(rhs)) {
        break;
      }
      lhs += kStepSize;
      rhs += kStepSize;
    }
  }
#endif
  while (lhs < limit) {
    int r = static_cast<int>(*lhs) - static_cast<int>(*rhs);
    if (r != 0) return r;
    ++lhs;
    ++rhs;
  }
  return 0;
}


// Calculate 10^exponent.
static inline int TenToThe(int exponent) {
  ASSERT(exponent <= 9);
  ASSERT(exponent >= 1);
  int answer = 10;
  for (int i = 1; i < exponent; i++) answer *= 10;
  return answer;
}


// The type-based aliasing rule allows the compiler to assume that pointers of
// different types (for some definition of different) never alias each other.
// Thus the following code does not work:
//
// float f = foo();
// int fbits = *(int*)(&f);
//
// The compiler 'knows' that the int pointer can't refer to f since the types
// don't match, so the compiler may cache f in a register, leaving random data
// in fbits.  Using C++ style casts makes no difference, however a pointer to
// char data is assumed to alias any other pointer.  This is the 'memcpy
// exception'.
//
// Bit_cast uses the memcpy exception to move the bits from a variable of one
// type of a variable of another type.  Of course the end result is likely to
// be implementation dependent.  Most compilers (gcc-4.2 and MSVC 2005)
// will completely optimize BitCast away.
//
// There is an additional use for BitCast.
// Recent gccs will warn when they see casts that may result in breakage due to
// the type-based aliasing rule.  If you have checked that there is no breakage
// you can use BitCast to cast one pointer type to another.  This confuses gcc
// enough that it can no longer see that you have cast one pointer type to
// another thus avoiding the warning.
template <class Dest, class Source>
inline Dest BitCast(const Source& source) {
  // Compile time assertion: sizeof(Dest) == sizeof(Source)
  // A compile error here means your Dest and Source have different sizes.
  typedef char VerifySizesAreEqual[sizeof(Dest) == sizeof(Source) ? 1 : -1];

  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

template <class Dest, class Source>
inline Dest BitCast(Source* source) {
  return BitCast<Dest>(reinterpret_cast<uintptr_t>(source));
}

} }  // namespace v8::internal

#endif  // V8_UTILS_H_
