// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ADDRESS_MAP_H_
#define V8_ADDRESS_MAP_H_

#include "include/v8.h"
#include "src/assert-scope.h"
#include "src/base/hashmap.h"
#include "src/objects/heap-object.h"
#include "src/roots.h"

namespace v8 {
namespace internal {

template <typename Type>
class PointerToIndexHashMap
    : public base::TemplateHashMapImpl<uintptr_t, uint32_t,
                                       base::KeyEqualityMatcher<intptr_t>,
                                       base::DefaultAllocationPolicy> {
 public:
  typedef base::TemplateHashMapEntry<uintptr_t, uint32_t> Entry;

  inline void Set(Type value, uint32_t index) {
    uintptr_t key = Key(value);
    LookupOrInsert(key, Hash(key))->value = index;
  }

  inline Maybe<uint32_t> Get(Type value) const {
    uintptr_t key = Key(value);
    Entry* entry = Lookup(key, Hash(key));
    if (entry == nullptr) return Nothing<uint32_t>();
    return Just(entry->value);
  }

 private:
  static inline uintptr_t Key(Type value);

  static uint32_t Hash(uintptr_t key) { return static_cast<uint32_t>(key); }
};

template <>
inline uintptr_t PointerToIndexHashMap<Address>::Key(Address value) {
  return static_cast<uintptr_t>(value);
}

template <>
inline uintptr_t PointerToIndexHashMap<HeapObject>::Key(HeapObject value) {
  return value.ptr();
}

class AddressToIndexHashMap : public PointerToIndexHashMap<Address> {};
class HeapObjectToIndexHashMap : public PointerToIndexHashMap<HeapObject> {};

class RootIndexMap {
 public:
  explicit RootIndexMap(Isolate* isolate);

  // Returns true on successful lookup and sets *|out_root_list|.
  bool Lookup(HeapObject obj, RootIndex* out_root_list) const {
    Maybe<uint32_t> maybe_index = map_->Get(obj);
    if (maybe_index.IsJust()) {
      *out_root_list = static_cast<RootIndex>(maybe_index.FromJust());
      return true;
    }
    return false;
  }
  bool Lookup(Address obj, RootIndex* out_root_list) const;

 private:
  HeapObjectToIndexHashMap* map_;

  DISALLOW_COPY_AND_ASSIGN(RootIndexMap);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ADDRESS_MAP_H_
