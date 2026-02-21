// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NUMBER_STRING_CACHE_H_
#define V8_OBJECTS_NUMBER_STRING_CACHE_H_

#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

// Used for mapping non-zero Smi to Strings.
V8_OBJECT class SmiStringCache : public FixedArray {
 public:
  using Super = FixedArray;

  // Empty entries are initialized with this sentinel (both key and value).
  static constexpr Tagged<Smi> kEmptySentinel = Smi::zero();

  static constexpr int kEntryKeyIndex = 0;
  static constexpr int kEntryValueIndex = 1;
  static constexpr int kEntrySize = 2;

  static constexpr int kInitialSize = 128;

  // Maximal allowed capacity in number of entries.
  static constexpr int kMaxCapacity = FixedArray::kMaxCapacity / kEntrySize;

  inline uint32_t capacity() const;

  // Clears all entries in the table.
  inline void Clear();

  // Iterates the table and computes the number of occupied entries.
  uint32_t GetUsedEntriesCount();

  // Prints contents of the cache with a comment.
  void Print(const char* comment);

  // Returns entry index corresponding to given number.
  inline InternalIndex GetEntryFor(Tagged<Smi> number) const;
  static inline InternalIndex GetEntryFor(Isolate* isolate, Tagged<Smi> number);

  // Attempt to find the number in a cache. In case of success, returns
  // the string representation of the number. Otherwise returns undefined.
  static inline Handle<Object> Get(Isolate* isolate, InternalIndex entry,
                                   Tagged<Smi> number);

  // Puts <number, string> entry to the cache, potentially overwriting
  // existing entry.
  static inline void Set(Isolate* isolate, InternalIndex entry,
                         Tagged<Smi> number, DirectHandle<String> string);

  template <class IsolateT>
  static inline DirectHandle<SmiStringCache> New(IsolateT* isolate,
                                                 int capacity);

 protected:
  using Super::capacity;
  using Super::get;
  using Super::length;
  using Super::OffsetOfElementAt;
  using Super::set;
} V8_OBJECT_END;

// Used for mapping raw doubles to Strings.
V8_OBJECT class DoubleStringCache : public HeapObjectLayout {
 public:
  V8_OBJECT struct Entry {
    UnalignedDoubleMember key_;
    TaggedMember<UnionOf<Smi, String>> value_;
  } V8_OBJECT_END;

  using Header = HeapObjectLayout;

  // Empty entries are initialized with this sentinel (both key and value).
  static constexpr Tagged<Smi> kEmptySentinel = Smi::zero();

  static constexpr int kInitialSize = 128;
  static constexpr int kMaxCapacity = TAGGED_SIZE_8_BYTES ? 0x1000 : 0x2000;

  inline uint32_t capacity() const { return capacity_; }

  // Clears all entries in the table.
  inline void Clear();

  // Iterates the table and computes the number of occupied entries.
  uint32_t GetUsedEntriesCount();

  // Prints contents of the cache with a comment.
  void Print(const char* comment);

  // Returns entry index corresponding to given bitwise representation of
  // a double value.
  inline InternalIndex GetEntryFor(uint64_t number_bits) const;
  static inline InternalIndex GetEntryFor(Isolate* isolate,
                                          uint64_t number_bits);

  // Attempt to find the number in a cache by given bitwise representation.
  // In case of success, returns the string representation of the number.
  // Otherwise returns undefined.
  static inline Handle<Object> Get(Isolate* isolate, InternalIndex entry,
                                   uint64_t number_bits);

  // Puts <number, string> entry to the cache, potentially overwriting
  // existing entry. The number is given in bitwise representation.
  static inline void Set(Isolate* isolate, InternalIndex entry,
                         uint64_t number_bits, DirectHandle<String> string);

  template <class IsolateT>
  static inline DirectHandle<DoubleStringCache> New(IsolateT* isolate,
                                                    int capacity);

  DECL_PRINTER(DoubleStringCache)
  DECL_VERIFIER(DoubleStringCache)

  inline int AllocatedSize() const { return SizeFor(this->capacity()); }

  static inline constexpr int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(OffsetOfElementAt(length));
  }
  static inline constexpr int OffsetOfElementAt(int index) {
    return sizeof(Header) + kUInt32Size + index * sizeof(Entry);
  }

  class BodyDescriptor;

 private:
  friend class CodeStubAssembler;

  static inline InternalIndex GetEntryFor(uint64_t number_bits,
                                          uint32_t capacity);

  inline Entry* begin() { return &entries()[0]; }
  inline const Entry* begin() const { return &entries()[0]; }
  inline Entry* end() { return &entries()[capacity_]; }
  inline const Entry* end() const { return &entries()[capacity_]; }

  uint32_t capacity_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
  FLEXIBLE_ARRAY_MEMBER(Entry, entries);
} V8_OBJECT_END;

static_assert(DoubleStringCache::SizeFor(DoubleStringCache::kMaxCapacity) <
              kMaxRegularHeapObjectSize);

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_NUMBER_STRING_CACHE_H_
