// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_HASHMAP_ENTRY_H_
#define V8_BASE_HASHMAP_ENTRY_H_

#include <cstdint>
#include <type_traits>

#include "src/base/bit-field.h"
#include "src/base/memory.h"

namespace v8 {
namespace base {

// Marker type for hashmaps without a value (i.e. hashsets). These won't
// allocate space for the value in the entry.
struct NoHashMapValue {};

// HashMap entries are (key, value, hash) triplets, with a boolean indicating if
// they are an empty entry. Some clients may not need to use the value slot
// (e.g. implementers of sets, where the key is the value), in which case they
// should use NoHashMapValue.
template <typename Key, typename Value>
struct TemplateHashMapEntry {
  static_assert((!std::is_same_v<Value, NoHashMapValue>));

  Key key;
  Value value;

  TemplateHashMapEntry(Key key, Value value, uint32_t hash)
      : key(key),
        value(value),
        hash_and_exists_(HashField::encode(hash) |
                         ExistsField::encode(true)) {}

  using HashField = BitField<uint32_t, 0, 31>;
  using ExistsField = HashField::Next<bool, 1>;
  static constexpr uint32_t kHashValueMask = HashField::kMax;

  uint32_t hash() const { return HashField::decode(hash_and_exists_); }
  bool exists() const { return ExistsField::decode(hash_and_exists_); }

  void clear() {
    hash_and_exists_ = ExistsField::update(hash_and_exists_, false);
  }

 private:
  uint32_t hash_and_exists_;
};

// Specialization for no value.
template <typename Key>
struct TemplateHashMapEntry<Key, NoHashMapValue> {
  union {
    Key key;
    NoHashMapValue value;  // Value in union with key to not take up space.
  };

  TemplateHashMapEntry(Key key, NoHashMapValue value, uint32_t hash)
      : key(key),
        hash_and_exists_(HashField::encode(hash) |
                         ExistsField::encode(true)) {}

  using HashField = BitField<uint32_t, 0, 31>;
  using ExistsField = HashField::Next<bool, 1>;
  static constexpr uint32_t kHashValueMask = HashField::kMax;

  uint32_t hash() const { return HashField::decode(hash_and_exists_); }
  bool exists() const { return ExistsField::decode(hash_and_exists_); }

  void clear() {
    hash_and_exists_ = ExistsField::update(hash_and_exists_, false);
  }

 private:
  uint32_t hash_and_exists_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_HASHMAP_ENTRY_H_
