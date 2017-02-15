// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/stub-cache.h"

#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/type-info.h"

namespace v8 {
namespace internal {

StubCache::StubCache(Isolate* isolate, Code::Kind ic_kind)
    : isolate_(isolate), ic_kind_(ic_kind) {}

void StubCache::Initialize() {
  DCHECK(base::bits::IsPowerOfTwo32(kPrimaryTableSize));
  DCHECK(base::bits::IsPowerOfTwo32(kSecondaryTableSize));
  Clear();
}

#ifdef DEBUG
namespace {

bool CommonStubCacheChecks(StubCache* stub_cache, Name* name, Map* map,
                           Code* code) {
  // Validate that the name does not move on scavenge, and that we
  // can use identity checks instead of structural equality checks.
  DCHECK(!name->GetHeap()->InNewSpace(name));
  DCHECK(name->IsUniqueName());
  DCHECK(name->HasHashCode());
  if (code) {
    Code::Flags expected_flags = Code::RemoveHolderFromFlags(
        Code::ComputeHandlerFlags(stub_cache->ic_kind()));
    Code::Flags flags = Code::RemoveHolderFromFlags(code->flags());
    DCHECK_EQ(expected_flags, flags);
    DCHECK_EQ(Code::HANDLER, Code::ExtractKindFromFlags(code->flags()));
  }
  return true;
}

}  // namespace
#endif

Code* StubCache::Set(Name* name, Map* map, Code* code) {
  DCHECK(CommonStubCacheChecks(this, name, map, code));

  // Compute the primary entry.
  int primary_offset = PrimaryOffset(name, map);
  Entry* primary = entry(primary_, primary_offset);
  Code* old_code = primary->value;

  // If the primary entry has useful data in it, we retire it to the
  // secondary cache before overwriting it.
  if (old_code != isolate_->builtins()->builtin(Builtins::kIllegal)) {
    Map* old_map = primary->map;
    int seed = PrimaryOffset(primary->key, old_map);
    int secondary_offset = SecondaryOffset(primary->key, seed);
    Entry* secondary = entry(secondary_, secondary_offset);
    *secondary = *primary;
  }

  // Update primary cache.
  primary->key = name;
  primary->value = code;
  primary->map = map;
  isolate()->counters()->megamorphic_stub_cache_updates()->Increment();
  return code;
}

Code* StubCache::Get(Name* name, Map* map) {
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
  return NULL;
}


void StubCache::Clear() {
  Code* empty = isolate_->builtins()->builtin(Builtins::kIllegal);
  for (int i = 0; i < kPrimaryTableSize; i++) {
    primary_[i].key = isolate()->heap()->empty_string();
    primary_[i].map = NULL;
    primary_[i].value = empty;
  }
  for (int j = 0; j < kSecondaryTableSize; j++) {
    secondary_[j].key = isolate()->heap()->empty_string();
    secondary_[j].map = NULL;
    secondary_[j].value = empty;
  }
}


void StubCache::CollectMatchingMaps(SmallMapList* types, Handle<Name> name,
                                    Handle<Context> native_context,
                                    Zone* zone) {
  for (int i = 0; i < kPrimaryTableSize; i++) {
    if (primary_[i].key == *name) {
      Map* map = primary_[i].map;
      // Map can be NULL, if the stub is constant function call
      // with a primitive receiver.
      if (map == NULL) continue;

      int offset = PrimaryOffset(*name, map);
      if (entry(primary_, offset) == &primary_[i] &&
          TypeFeedbackOracle::IsRelevantFeedback(map, *native_context)) {
        types->AddMapIfMissing(Handle<Map>(map), zone);
      }
    }
  }

  for (int i = 0; i < kSecondaryTableSize; i++) {
    if (secondary_[i].key == *name) {
      Map* map = secondary_[i].map;
      // Map can be NULL, if the stub is constant function call
      // with a primitive receiver.
      if (map == NULL) continue;

      // Lookup in primary table and skip duplicates.
      int primary_offset = PrimaryOffset(*name, map);

      // Lookup in secondary table and add matches.
      int offset = SecondaryOffset(*name, primary_offset);
      if (entry(secondary_, offset) == &secondary_[i] &&
          TypeFeedbackOracle::IsRelevantFeedback(map, *native_context)) {
        types->AddMapIfMissing(Handle<Map>(map), zone);
      }
    }
  }
}
}  // namespace internal
}  // namespace v8
