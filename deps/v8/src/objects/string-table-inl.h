// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_TABLE_INL_H_
#define V8_OBJECTS_STRING_TABLE_INL_H_

#include "src/objects/string-table.h"

#include "src/objects/string-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(StringSet)
CAST_ACCESSOR(StringTable)

StringTable::StringTable(Address ptr)
    : HashTable<StringTable, StringTableShape>(ptr) {
  SLOW_DCHECK(IsStringTable());
}

StringSet::StringSet(Address ptr) : HashTable<StringSet, StringSetShape>(ptr) {
  SLOW_DCHECK(IsStringSet());
}

bool StringSetShape::IsMatch(String key, Object value) {
  DCHECK(value->IsString());
  return key->Equals(String::cast(value));
}

uint32_t StringSetShape::Hash(Isolate* isolate, String key) {
  return key->Hash();
}

uint32_t StringSetShape::HashForObject(ReadOnlyRoots roots, Object object) {
  return String::cast(object)->Hash();
}

StringTableKey::StringTableKey(uint32_t hash_field)
    : HashTableKey(hash_field >> Name::kHashShift), hash_field_(hash_field) {}

void StringTableKey::set_hash_field(uint32_t hash_field) {
  hash_field_ = hash_field;
  set_hash(hash_field >> Name::kHashShift);
}

Handle<Object> StringTableShape::AsHandle(Isolate* isolate,
                                          StringTableKey* key) {
  return key->AsHandle(isolate);
}

uint32_t StringTableShape::HashForObject(ReadOnlyRoots roots, Object object) {
  return String::cast(object)->Hash();
}

RootIndex StringTableShape::GetMapRootIndex() {
  return RootIndex::kStringTableMap;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_TABLE_INL_H_
