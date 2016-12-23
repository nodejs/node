// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/lookup-cache.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {

void DescriptorLookupCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].source = NULL;
}

int KeyedLookupCache::Hash(Handle<Map> map, Handle<Name> name) {
  DisallowHeapAllocation no_gc;
  // Uses only lower 32 bits if pointers are larger.
  uintptr_t addr_hash =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(*map)) >> kMapHashShift;
  return static_cast<uint32_t>((addr_hash ^ name->Hash()) & kCapacityMask);
}

int KeyedLookupCache::Lookup(Handle<Map> map, Handle<Name> name) {
  DisallowHeapAllocation no_gc;
  int index = (Hash(map, name) & kHashMask);
  for (int i = 0; i < kEntriesPerBucket; i++) {
    Key& key = keys_[index + i];
    if ((key.map == *map) && key.name->Equals(*name)) {
      return field_offsets_[index + i];
    }
  }
  return kNotFound;
}

void KeyedLookupCache::Update(Handle<Map> map, Handle<Name> name,
                              int field_offset) {
  DisallowHeapAllocation no_gc;
  if (!name->IsUniqueName()) {
    if (!StringTable::InternalizeStringIfExists(name->GetIsolate(),
                                                Handle<String>::cast(name))
             .ToHandle(&name)) {
      return;
    }
  }
  // This cache is cleared only between mark compact passes, so we expect the
  // cache to only contain old space names.
  DCHECK(!map->GetIsolate()->heap()->InNewSpace(*name));

  int index = (Hash(map, name) & kHashMask);
  // After a GC there will be free slots, so we use them in order (this may
  // help to get the most frequently used one in position 0).
  for (int i = 0; i < kEntriesPerBucket; i++) {
    Key& key = keys_[index];
    Object* free_entry_indicator = NULL;
    if (key.map == free_entry_indicator) {
      key.map = *map;
      key.name = *name;
      field_offsets_[index + i] = field_offset;
      return;
    }
  }
  // No free entry found in this bucket, so we move them all down one and
  // put the new entry at position zero.
  for (int i = kEntriesPerBucket - 1; i > 0; i--) {
    Key& key = keys_[index + i];
    Key& key2 = keys_[index + i - 1];
    key = key2;
    field_offsets_[index + i] = field_offsets_[index + i - 1];
  }

  // Write the new first entry.
  Key& key = keys_[index];
  key.map = *map;
  key.name = *name;
  field_offsets_[index] = field_offset;
}

void KeyedLookupCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].map = NULL;
}

}  // namespace internal
}  // namespace v8
