// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/stub-cache.h"

#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/counters.h"
#include "src/heap/heap-inl.h"  // For InYoungGeneration().
#include "src/ic/ic-inl.h"

namespace v8 {
namespace internal {

StubCache::StubCache(Isolate* isolate) : isolate_(isolate) {
  // Ensure the nullptr (aka Smi::kZero) which StubCache::Get() returns
  // when the entry is not found is not considered as a handler.
  DCHECK(!IC::IsHandler(MaybeObject()));
}

void StubCache::Initialize() {
  DCHECK(base::bits::IsPowerOfTwo(kPrimaryTableSize));
  DCHECK(base::bits::IsPowerOfTwo(kSecondaryTableSize));
  Clear();
}

// Hash algorithm for the primary table.  This algorithm is replicated in
// assembler for every architecture.  Returns an index into the table that
// is scaled by 1 << kCacheIndexShift.
int StubCache::PrimaryOffset(Name name, Map map) {
  STATIC_ASSERT(kCacheIndexShift == Name::kHashShift);
  // Compute the hash of the name (use entire hash field).
  DCHECK(name->HasHashCode());
  uint32_t field = name->hash_field();
  // Using only the low bits in 64-bit mode is unlikely to increase the
  // risk of collision even if the heap is spread over an area larger than
  // 4Gb (and not at all if it isn't).
  uint32_t map_low32bits =
      static_cast<uint32_t>(map.ptr() ^ (map.ptr() >> kMapKeyShift));
  // Base the offset on a simple combination of name and map.
  uint32_t key = map_low32bits + field;
  return key & ((kPrimaryTableSize - 1) << kCacheIndexShift);
}

// Hash algorithm for the secondary table.  This algorithm is replicated in
// assembler for every architecture.  Returns an index into the table that
// is scaled by 1 << kCacheIndexShift.
int StubCache::SecondaryOffset(Name name, int seed) {
  // Use the seed from the primary cache in the secondary cache.
  uint32_t name_low32bits = static_cast<uint32_t>(name.ptr());
  uint32_t key = (seed - name_low32bits) + kSecondaryMagic;
  return key & ((kSecondaryTableSize - 1) << kCacheIndexShift);
}

int StubCache::PrimaryOffsetForTesting(Name name, Map map) {
  return PrimaryOffset(name, map);
}

int StubCache::SecondaryOffsetForTesting(Name name, int seed) {
  return SecondaryOffset(name, seed);
}

#ifdef DEBUG
namespace {

bool CommonStubCacheChecks(StubCache* stub_cache, Name name, Map map,
                           MaybeObject handler) {
  // Validate that the name and handler do not move on scavenge, and that we
  // can use identity checks instead of structural equality checks.
  DCHECK(!Heap::InYoungGeneration(name));
  DCHECK(!Heap::InYoungGeneration(handler));
  DCHECK(name->IsUniqueName());
  DCHECK(name->HasHashCode());
  if (handler->ptr() != kNullAddress) DCHECK(IC::IsHandler(handler));
  return true;
}

}  // namespace
#endif

void StubCache::Set(Name name, Map map, MaybeObject handler) {
  DCHECK(CommonStubCacheChecks(this, name, map, handler));

  // Compute the primary entry.
  int primary_offset = PrimaryOffset(name, map);
  Entry* primary = entry(primary_, primary_offset);
  MaybeObject old_handler(primary->value);

  // If the primary entry has useful data in it, we retire it to the
  // secondary cache before overwriting it.
  if (old_handler != MaybeObject::FromObject(
                         isolate_->builtins()->builtin(Builtins::kIllegal)) &&
      primary->map != kNullAddress) {
    Map old_map = Map::cast(Object(primary->map));
    int seed = PrimaryOffset(Name::cast(Object(primary->key)), old_map);
    int secondary_offset =
        SecondaryOffset(Name::cast(Object(primary->key)), seed);
    Entry* secondary = entry(secondary_, secondary_offset);
    *secondary = *primary;
  }

  // Update primary cache.
  primary->key = name.ptr();
  primary->value = handler.ptr();
  primary->map = map.ptr();
  isolate()->counters()->megamorphic_stub_cache_updates()->Increment();
}

MaybeObject StubCache::Get(Name name, Map map) {
  DCHECK(CommonStubCacheChecks(this, name, map, MaybeObject()));
  int primary_offset = PrimaryOffset(name, map);
  Entry* primary = entry(primary_, primary_offset);
  if (primary->key == name.ptr() && primary->map == map.ptr()) {
    return MaybeObject(primary->value);
  }
  int secondary_offset = SecondaryOffset(name, primary_offset);
  Entry* secondary = entry(secondary_, secondary_offset);
  if (secondary->key == name.ptr() && secondary->map == map.ptr()) {
    return MaybeObject(secondary->value);
  }
  return MaybeObject();
}

void StubCache::Clear() {
  MaybeObject empty = MaybeObject::FromObject(
      isolate_->builtins()->builtin(Builtins::kIllegal));
  Name empty_string = ReadOnlyRoots(isolate()).empty_string();
  for (int i = 0; i < kPrimaryTableSize; i++) {
    primary_[i].key = empty_string.ptr();
    primary_[i].map = kNullAddress;
    primary_[i].value = empty.ptr();
  }
  for (int j = 0; j < kSecondaryTableSize; j++) {
    secondary_[j].key = empty_string.ptr();
    secondary_[j].map = kNullAddress;
    secondary_[j].value = empty.ptr();
  }
}

}  // namespace internal
}  // namespace v8
