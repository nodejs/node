// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/stub-cache.h"

#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/counters.h"
#include "src/heap/heap.h"
#include "src/ic/ic-inl.h"

namespace v8 {
namespace internal {

StubCache::StubCache(Isolate* isolate) : isolate_(isolate) {
  // Ensure the nullptr (aka Smi::kZero) which StubCache::Get() returns
  // when the entry is not found is not considered as a handler.
  DCHECK(!IC::IsHandler(nullptr));
}

void StubCache::Initialize() {
  DCHECK(base::bits::IsPowerOfTwo(kPrimaryTableSize));
  DCHECK(base::bits::IsPowerOfTwo(kSecondaryTableSize));
  Clear();
}

// Hash algorithm for the primary table.  This algorithm is replicated in
// assembler for every architecture.  Returns an index into the table that
// is scaled by 1 << kCacheIndexShift.
int StubCache::PrimaryOffset(Name* name, Map* map) {
  STATIC_ASSERT(kCacheIndexShift == Name::kHashShift);
  // Compute the hash of the name (use entire hash field).
  DCHECK(name->HasHashCode());
  uint32_t field = name->hash_field();
  // Using only the low bits in 64-bit mode is unlikely to increase the
  // risk of collision even if the heap is spread over an area larger than
  // 4Gb (and not at all if it isn't).
  uint32_t map_low32bits =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(map));
  // Base the offset on a simple combination of name and map.
  uint32_t key = (map_low32bits + field) ^ kPrimaryMagic;
  return key & ((kPrimaryTableSize - 1) << kCacheIndexShift);
}

// Hash algorithm for the secondary table.  This algorithm is replicated in
// assembler for every architecture.  Returns an index into the table that
// is scaled by 1 << kCacheIndexShift.
int StubCache::SecondaryOffset(Name* name, int seed) {
  // Use the seed from the primary cache in the secondary cache.
  uint32_t name_low32bits =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name));
  uint32_t key = (seed - name_low32bits) + kSecondaryMagic;
  return key & ((kSecondaryTableSize - 1) << kCacheIndexShift);
}

#ifdef DEBUG
namespace {

bool CommonStubCacheChecks(StubCache* stub_cache, Name* name, Map* map,
                           Object* handler) {
  // Validate that the name and handler do not move on scavenge, and that we
  // can use identity checks instead of structural equality checks.
  DCHECK(!name->GetHeap()->InNewSpace(name));
  DCHECK(!name->GetHeap()->InNewSpace(handler));
  DCHECK(name->IsUniqueName());
  DCHECK(name->HasHashCode());
  if (handler) DCHECK(IC::IsHandler(handler));
  return true;
}

}  // namespace
#endif

Object* StubCache::Set(Name* name, Map* map, Object* handler) {
  DCHECK(CommonStubCacheChecks(this, name, map, handler));

  // Compute the primary entry.
  int primary_offset = PrimaryOffset(name, map);
  Entry* primary = entry(primary_, primary_offset);
  Object* old_handler = primary->value;

  // If the primary entry has useful data in it, we retire it to the
  // secondary cache before overwriting it.
  if (old_handler != isolate_->builtins()->builtin(Builtins::kIllegal)) {
    Map* old_map = primary->map;
    int seed = PrimaryOffset(primary->key, old_map);
    int secondary_offset = SecondaryOffset(primary->key, seed);
    Entry* secondary = entry(secondary_, secondary_offset);
    *secondary = *primary;
  }

  // Update primary cache.
  primary->key = name;
  primary->value = handler;
  primary->map = map;
  isolate()->counters()->megamorphic_stub_cache_updates()->Increment();
  return handler;
}

Object* StubCache::Get(Name* name, Map* map) {
  DCHECK(CommonStubCacheChecks(this, name, map, nullptr));
  int primary_offset = PrimaryOffset(name, map);
  Entry* primary = entry(primary_, primary_offset);
  if (primary->key == name && primary->map == map) {
    return primary->value;
  }
  int secondary_offset = SecondaryOffset(name, primary_offset);
  Entry* secondary = entry(secondary_, secondary_offset);
  if (secondary->key == name && secondary->map == map) {
    return secondary->value;
  }
  return nullptr;
}


void StubCache::Clear() {
  Code* empty = isolate_->builtins()->builtin(Builtins::kIllegal);
  for (int i = 0; i < kPrimaryTableSize; i++) {
    primary_[i].key = isolate()->heap()->empty_string();
    primary_[i].map = nullptr;
    primary_[i].value = empty;
  }
  for (int j = 0; j < kSecondaryTableSize; j++) {
    secondary_[j].key = isolate()->heap()->empty_string();
    secondary_[j].map = nullptr;
    secondary_[j].value = empty;
  }
}

}  // namespace internal
}  // namespace v8
