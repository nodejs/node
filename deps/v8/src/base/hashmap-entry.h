// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_HASHMAP_ENTRY_H_
#define V8_BASE_HASHMAP_ENTRY_H_

#include <cstdint>

namespace v8 {
namespace base {

// HashMap entries are (key, value, hash) triplets, with a boolean indicating if
// they are an empty entry. Some clients may not need to use the value slot
// (e.g. implementers of sets, where the key is the value).
template <typename Key, typename Value>
struct TemplateHashMapEntry {
  Key key;
  Value value;
  uint32_t hash;  // The full hash value for key

  TemplateHashMapEntry(Key key, Value value, uint32_t hash)
      : key(key), value(value), hash(hash), exists_(true) {}

  bool exists() const { return exists_; }

  void clear() { exists_ = false; }

 private:
  bool exists_;
};

// Specialization for pointer-valued keys
template <typename Key, typename Value>
struct TemplateHashMapEntry<Key*, Value> {
  Key* key;
  Value value;
  uint32_t hash;  // The full hash value for key

  TemplateHashMapEntry(Key* key, Value value, uint32_t hash)
      : key(key), value(value), hash(hash) {}

  bool exists() const { return key != nullptr; }

  void clear() { key = nullptr; }
};

// TODO(leszeks): There could be a specialisation for void values (e.g. for
// sets), which omits the value field

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_HASHMAP_ENTRY_H_
