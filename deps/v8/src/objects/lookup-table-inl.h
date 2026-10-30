// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LOOKUP_TABLE_INL_H_
#define V8_OBJECTS_LOOKUP_TABLE_INL_H_

#include "src/objects/lookup-table.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/fixed-primitive-array-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/name-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

template <uint32_t IndexBits, typename U>
struct CastTraits<LookupTable<IndexBits, U>> : public CastTraits<ByteArray> {};

template <uint32_t IndexBits, typename U>
template <typename IsolateT>
Handle<LookupTable<IndexBits, U>> LookupTable<IndexBits, U>::New(
    IsolateT* isolate, uint32_t number_of_elements, AllocationType allocation) {
  Handle<LookupTable> result = Cast<LookupTable>(
      Super::New(isolate, CapacityFor(number_of_elements), allocation));
  result->Clear();
  return result;
}

template <uint32_t IndexBits, typename U>
uint32_t LookupTable<IndexBits, U>::capacity() const {
  return this->length().value();
}

template <uint32_t IndexBits, typename U>
uint32_t LookupTable<IndexBits, U>::slot_mask() const {
  return capacity() - 1;
}

template <uint32_t IndexBits, typename U>
void LookupTable<IndexBits, U>::Clear() {
  std::memset(this->begin(), 0, this->capacity() * sizeof(Entry));
}

template <uint32_t IndexBits, typename U>
void LookupTable<IndexBits, U>::CopyFrom(Tagged<LookupTable> src) {
  DCHECK_EQ(this->capacity(), src->capacity());
  std::memcpy(this->begin(), src->begin(), this->capacity() * sizeof(Entry));
}

template <uint32_t IndexBits, typename U>
template <typename KeyGetter>
bool LookupTable<IndexBits, U>::Lookup(Tagged<Name> name,
                                       uint32_t valid_entries,
                                       KeyGetter&& get_key,
                                       uint32_t* out_index) const {
  uint32_t mask = slot_mask();
  uint32_t hash = name->hash();
  uint32_t hash_tag = ExtractHashTag(hash);
  uint32_t count = 1;
  uint32_t slot = FirstProbe(hash, mask);
  while (true) {
    Entry entry = this->get(slot);
    if (entry == kEmptyEntry) return false;
    if (entry.hash_tag() == hash_tag) {
      uint32_t index = entry.one_based_index() - 1;
      if (index < valid_entries && get_key(index) == name) {
        *out_index = index;
        return true;
      }
    }
    slot = NextProbe(slot, count++, mask);
  }
}

template <uint32_t IndexBits, typename U>
template <typename KeyGetter>
void LookupTable<IndexBits, U>::Insert(Tagged<Name> key, uint32_t hash,
                                       uint32_t index, KeyGetter&& get_key) {
  Insert(slot_mask(), key, hash, index, std::forward<KeyGetter>(get_key));
}

template <uint32_t IndexBits, typename U>
template <typename KeyGetter>
void LookupTable<IndexBits, U>::Insert(uint32_t mask, Tagged<Name> key,
                                       uint32_t hash, uint32_t index,
                                       KeyGetter&& get_key) {
  USE(key, get_key);
#ifdef DEBUG
  uint32_t hash_tag = ExtractHashTag(hash);
#endif
  uint32_t count = 1;
  uint32_t slot = FirstProbe(hash, mask);
  while (true) {
    Entry entry = this->get(slot);
    if (entry == kEmptyEntry) break;
    DCHECK_IMPLIES(entry.hash_tag() == hash_tag,
                   get_key(entry.one_based_index() - 1) != key);
    slot = NextProbe(slot, count++, mask);
  }
  this->set(slot, EncodeEntry(hash, index));
}

template <uint32_t IndexBits, typename U>
template <typename KeyGetter>
void LookupTable<IndexBits, U>::Rebuild(uint32_t number_of_elements,
                                        KeyGetter&& get_key) {
  Clear();
  uint32_t mask = slot_mask();
  for (uint32_t i = 0; i < number_of_elements; ++i) {
    Tagged<Name> key = get_key(i);
    Insert(mask, key, key->hash(), i, get_key);
  }
}

template <uint32_t IndexBits, typename U>
void LookupTable<IndexBits, U>::Remove(uint32_t mask, uint32_t hash,
                                       uint32_t index) {
  U expected = EncodeEntry(hash, index);
  uint32_t count = 1;
  uint32_t slot = FirstProbe(hash, mask);
  while (this->get(slot) != expected) {
    DCHECK_NE(this->get(slot), kEmptyEntry);
    slot = NextProbe(slot, count++, mask);
  }
  this->set(slot, kEmptyEntry);
}

template <uint32_t IndexBits, typename U>
template <typename KeyGetter>
void LookupTable<IndexBits, U>::RemoveEntries(uint32_t old_nof,
                                              uint32_t new_nof,
                                              KeyGetter&& get_key) {
  DCHECK_LE(new_nof, old_nof);
  uint32_t mask = slot_mask();
  while (old_nof > new_nof) {
    --old_nof;
    Remove(mask, get_key(old_nof)->hash(), old_nof);
  }
}

#ifdef DEBUG
template <uint32_t IndexBits, typename U>
template <typename KeyGetter>
bool LookupTable<IndexBits, U>::Verify(uint32_t number_of_elements,
                                       uint32_t max_elements,
                                       KeyGetter&& get_key) const {
  uint32_t cap = capacity();
  if (cap < CapacityFor(max_elements)) {
    return false;
  }

  uint32_t non_empty_count = 0;
  for (uint32_t slot = 0; slot < cap; ++slot) {
    if (this->get(slot) != kEmptyEntry) non_empty_count++;
  }
  if (non_empty_count != number_of_elements) return false;

  for (uint32_t i = 0; i < number_of_elements; ++i) {
    Tagged<Name> key = get_key(i);
    uint32_t hash;
    const bool has_hash = key->TryGetHash(&hash);
    CHECK(has_hash);
    uint32_t lookup_index;
    if (!Lookup(key, number_of_elements, get_key, &lookup_index) ||
        lookup_index != i) {
      return false;
    }
  }
  return true;
}
#endif  // DEBUG

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LOOKUP_TABLE_INL_H_
