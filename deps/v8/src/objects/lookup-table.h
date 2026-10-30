// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LOOKUP_TABLE_H_
#define V8_OBJECTS_LOOKUP_TABLE_H_

#include <bit>

#include "src/objects/bit-field-group.h"
#include "src/objects/fixed-primitive-array.h"
#include "src/objects/name.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

// Open-addressed hash table with quadratic probing mapping Name keys to
// indices. Each entry of type `U` encodes a 1-based index in the upper
// `kIndexBits` bits and a hash tag in the lower `kHashTagBits` bits. The hash
// tag is a partial fingerprint consisting of the upper `kHashTagBits` bits of
// the Name's hash (the low bits are already used for the initial bucket
// calculation), used to filter out probe collisions before loading and
// comparing the full Name key. Entry value 0 denotes an empty bucket.
V8_OBJECT
template <uint32_t kIndexBits_, typename U = uint32_t>
class LookupTable : public FixedIntegerArrayBase<U, ByteArray> {
  V8_IT_REUSE_PARENT;
  using Super = FixedIntegerArrayBase<U, ByteArray>;

 public:
  // Number of bits in each Entry used to store the 1-based element index.
  static constexpr int kIndexBits = kIndexBits_;
  // Number of remaining bits in each Entry used to store the hash tag.
  static constexpr int kHashTagBits = sizeof(U) * kBitsPerByte - kIndexBits;
  // Right-shift amount to extract the upper `kHashTagBits` bits from a
  // `Name::HashBits::kSize`-bit Name hash.
  static constexpr int kHashTagShift = Name::HashBits::kSize - kHashTagBits;
  static_assert(kIndexBits > 0 && kHashTagBits > 0);
  static_assert(kHashTagBits <= Name::HashBits::kSize);

  struct Entry : BitFieldGroup<Entry, U> {
    using Base = BitFieldGroup<Entry, U>;
    using Base::Base;
    using typename Base::GroupType;

    using HashTagBits = typename Base::template First<uint32_t, kHashTagBits>;
    using OneBasedIndexBits =
        typename HashTagBits::template Next<uint32_t, kIndexBits>;

    BIT_FIELD_GROUP_ACCESSORS(hash_tag, HashTagBits)
    BIT_FIELD_GROUP_ACCESSORS(one_based_index, OneBasedIndexBits)
  };

  static constexpr U kEmptyEntry = 0;
  static constexpr uint32_t kMaxElements = Entry::OneBasedIndexBits::kMax;

  static constexpr uint32_t kMinCapacity = 4;

  static constexpr uint32_t CapacityFor(uint32_t number_of_elements) {
    DCHECK_GT(number_of_elements, 0);
    DCHECK_LE(number_of_elements, kMaxElements);
    // Add 50% slack to make slot collisions sufficiently unlikely.
    uint32_t raw_cap = number_of_elements + (number_of_elements >> 1);
    uint32_t capacity = std::bit_ceil(raw_cap);
    return std::max(capacity, kMinCapacity);
  }

  template <typename IsolateT>
  static inline Handle<LookupTable> New(
      IsolateT* isolate, uint32_t number_of_elements,
      AllocationType allocation = AllocationType::kYoung);

  V8_INLINE uint32_t capacity() const;
  inline void Clear();
  inline void CopyFrom(Tagged<LookupTable> src);

  template <typename KeyGetter>
  V8_INLINE bool Lookup(Tagged<Name> name, uint32_t valid_entries,
                        KeyGetter&& get_key, uint32_t* out_index) const;

  template <typename KeyGetter>
  V8_INLINE void Insert(Tagged<Name> key, uint32_t hash, uint32_t index,
                        KeyGetter&& get_key);

  template <typename KeyGetter>
  inline void Rebuild(uint32_t number_of_elements, KeyGetter&& get_key);

  template <typename KeyGetter>
  inline void RemoveEntries(uint32_t old_nof, uint32_t new_nof,
                            KeyGetter&& get_key);

#ifdef DEBUG
  template <typename KeyGetter>
  bool Verify(uint32_t number_of_elements, uint32_t max_elements,
              KeyGetter&& get_key) const;
#endif  // DEBUG

 private:
  template <typename KeyGetter>
  V8_INLINE void Insert(uint32_t mask, Tagged<Name> key, uint32_t hash,
                        uint32_t index, KeyGetter&& get_key);

  // Removes the most recently inserted entry (`index`). Only supports removing
  // the last entry (in reverse insertion order, as done by `RemoveEntries`):
  // since the table does not use tombstones, clearing an earlier entry's slot
  // to `kEmptyEntry` could break the probe chain of entries inserted after it.
  // TODO(572302954): implement proper remove.
  inline void Remove(uint32_t mask, uint32_t hash, uint32_t index);

  V8_INLINE uint32_t slot_mask() const;

  // Extract the upper kHashTagBits of the Name's hash as a hash tag, since the
  // low bits are already used for the initial bucket calculation.
  static constexpr uint32_t ExtractHashTag(uint32_t hash) {
    DCHECK_LE(hash, Name::HashBits::kMax);
    return hash >> kHashTagShift;
  }

  static constexpr Entry EncodeEntry(uint32_t hash, uint32_t index) {
    // The 1-based index is guaranteed to fit in the bit field.
    DCHECK_LT(index, kMaxElements);
    return Entry()
        .with_one_based_index_unchecked(index + 1)
        .with_hash_tag_unchecked(ExtractHashTag(hash));
  }

  static constexpr uint32_t FirstProbe(uint32_t hash, uint32_t mask) {
    return hash & mask;
  }

  static constexpr uint32_t NextProbe(uint32_t slot, uint32_t number,
                                      uint32_t mask) {
    return (slot + number) & mask;
  }
} V8_OBJECT_END;

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LOOKUP_TABLE_H_
