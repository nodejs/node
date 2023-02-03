// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/stub-cache.h"

#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/heap/heap-inl.h"  // For InYoungGeneration().
#include "src/ic/ic-inl.h"
#include "src/logging/counters.h"
#include "src/objects/tagged-value-inl.h"

namespace v8 {
namespace internal {

// static
void StubCache::ClearCallback(v8::Isolate* isolate, v8::GCType type,
                              v8::GCCallbackFlags flags, void* data) {
  StubCache* cache = static_cast<StubCache*>(data);
  cache->Clear();
}

StubCache::StubCache(Isolate* isolate) : isolate_(isolate) {
  // Ensure the nullptr (aka Smi::zero()) which StubCache::Get() returns
  // when the entry is not found is not considered as a handler.
  DCHECK(!IC::IsHandler(MaybeObject()));

  // The stub caches are not traversed during GC; clear them to force
  // their lazy re-initialization. This must be done after the
  // GC, because it relies on the new address of certain old space
  // objects (empty string, illegal builtin).

  isolate_->heap()->AddGCEpilogueCallback(ClearCallback,
                                          kGCTypeMarkSweepCompact, this);
}

StubCache::~StubCache() {
  isolate_->heap()->RemoveGCEpilogueCallback(ClearCallback, this);
}

void StubCache::Initialize() {
  DCHECK(base::bits::IsPowerOfTwo(kPrimaryTableSize));
  DCHECK(base::bits::IsPowerOfTwo(kSecondaryTableSize));
  Clear();
}

// Hash algorithm for the primary table. This algorithm is replicated in
// the AccessorAssembler.  Returns an index into the table that
// is scaled by 1 << kCacheIndexShift.
int StubCache::PrimaryOffset(Name name, Map map) {
  // Compute the hash of the name (use entire hash field).
  uint32_t field = name.RawHash();
  DCHECK(Name::IsHashFieldComputed(field));
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
// assembler. This hash should be sufficiently different from the primary one
// in order to avoid collisions for minified code with short names.
// Returns an index into the table that is scaled by 1 << kCacheIndexShift.
int StubCache::SecondaryOffset(Name name, Map old_map) {
  uint32_t name_low32bits = static_cast<uint32_t>(name.ptr());
  uint32_t map_low32bits = static_cast<uint32_t>(old_map.ptr());
  uint32_t key = (map_low32bits + name_low32bits);
  key = key + (key >> kSecondaryKeyShift);
  return key & ((kSecondaryTableSize - 1) << kCacheIndexShift);
}

int StubCache::PrimaryOffsetForTesting(Name name, Map map) {
  return PrimaryOffset(name, map);
}

int StubCache::SecondaryOffsetForTesting(Name name, Map map) {
  return SecondaryOffset(name, map);
}

#ifdef DEBUG
namespace {

bool CommonStubCacheChecks(StubCache* stub_cache, Name name, Map map,
                           MaybeObject handler) {
  // Validate that the name and handler do not move on scavenge, and that we
  // can use identity checks instead of structural equality checks.
  DCHECK(!Heap::InYoungGeneration(name));
  DCHECK(!Heap::InYoungGeneration(handler));
  DCHECK(name.IsUniqueName());
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
  MaybeObject old_handler(
      TaggedValue::ToMaybeObject(isolate(), primary->value));
  // If the primary entry has useful data in it, we retire it to the
  // secondary cache before overwriting it.
  if (old_handler != MaybeObject::FromObject(
                         isolate()->builtins()->code(Builtin::kIllegal)) &&
      !primary->map.IsSmi()) {
    Map old_map =
        Map::cast(StrongTaggedValue::ToObject(isolate(), primary->map));
    Name old_name =
        Name::cast(StrongTaggedValue::ToObject(isolate(), primary->key));
    int secondary_offset = SecondaryOffset(old_name, old_map);
    Entry* secondary = entry(secondary_, secondary_offset);
    *secondary = *primary;
  }

  // Update primary cache.
  primary->key = StrongTaggedValue(name);
  primary->value = TaggedValue(handler);
  primary->map = StrongTaggedValue(map);
  isolate()->counters()->megamorphic_stub_cache_updates()->Increment();
}

MaybeObject StubCache::Get(Name name, Map map) {
  DCHECK(CommonStubCacheChecks(this, name, map, MaybeObject()));
  int primary_offset = PrimaryOffset(name, map);
  Entry* primary = entry(primary_, primary_offset);
  if (primary->key == name && primary->map == map) {
    return TaggedValue::ToMaybeObject(isolate(), primary->value);
  }
  int secondary_offset = SecondaryOffset(name, map);
  Entry* secondary = entry(secondary_, secondary_offset);
  if (secondary->key == name && secondary->map == map) {
    return TaggedValue::ToMaybeObject(isolate(), secondary->value);
  }
  return MaybeObject();
}

void StubCache::Clear() {
  MaybeObject empty =
      MaybeObject::FromObject(isolate_->builtins()->code(Builtin::kIllegal));
  Name empty_string = ReadOnlyRoots(isolate()).empty_string();
  for (int i = 0; i < kPrimaryTableSize; i++) {
    primary_[i].key = StrongTaggedValue(empty_string);
    primary_[i].map = StrongTaggedValue(Smi::zero());
    primary_[i].value = TaggedValue(empty);
  }
  for (int j = 0; j < kSecondaryTableSize; j++) {
    secondary_[j].key = StrongTaggedValue(empty_string);
    secondary_[j].map = StrongTaggedValue(Smi::zero());
    secondary_[j].value = TaggedValue(empty);
  }
}

}  // namespace internal
}  // namespace v8
