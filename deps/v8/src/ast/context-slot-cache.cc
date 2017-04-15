// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/context-slot-cache.h"

#include <stdlib.h>

#include "src/ast/scopes.h"
#include "src/bootstrapper.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/feedback-vector.h ->
// src/feedback-vector-inl.h
#include "src/feedback-vector-inl.h"

namespace v8 {
namespace internal {

int ContextSlotCache::Hash(Object* data, String* name) {
  // Uses only lower 32 bits if pointers are larger.
  uintptr_t addr_hash =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)) >> 2;
  return static_cast<int>((addr_hash ^ name->Hash()) % kLength);
}

int ContextSlotCache::Lookup(Object* data, String* name, VariableMode* mode,
                             InitializationFlag* init_flag,
                             MaybeAssignedFlag* maybe_assigned_flag) {
  int index = Hash(data, name);
  Key& key = keys_[index];
  if ((key.data == data) && key.name->Equals(name)) {
    Value result(values_[index]);
    if (mode != nullptr) *mode = result.mode();
    if (init_flag != nullptr) *init_flag = result.initialization_flag();
    if (maybe_assigned_flag != nullptr)
      *maybe_assigned_flag = result.maybe_assigned_flag();
    return result.index() + kNotFound;
  }
  return kNotFound;
}

void ContextSlotCache::Update(Handle<Object> data, Handle<String> name,
                              VariableMode mode, InitializationFlag init_flag,
                              MaybeAssignedFlag maybe_assigned_flag,
                              int slot_index) {
  DisallowHeapAllocation no_gc;
  Handle<String> internalized_name;
  DCHECK(slot_index > kNotFound);
  if (StringTable::InternalizeStringIfExists(name->GetIsolate(), name)
          .ToHandle(&internalized_name)) {
    int index = Hash(*data, *internalized_name);
    Key& key = keys_[index];
    key.data = *data;
    key.name = *internalized_name;
    // Please note value only takes a uint as index.
    values_[index] =
        Value(mode, init_flag, maybe_assigned_flag, slot_index - kNotFound)
            .raw();
#ifdef DEBUG
    ValidateEntry(data, name, mode, init_flag, maybe_assigned_flag, slot_index);
#endif
  }
}

void ContextSlotCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].data = nullptr;
}

#ifdef DEBUG

void ContextSlotCache::ValidateEntry(Handle<Object> data, Handle<String> name,
                                     VariableMode mode,
                                     InitializationFlag init_flag,
                                     MaybeAssignedFlag maybe_assigned_flag,
                                     int slot_index) {
  DisallowHeapAllocation no_gc;
  Handle<String> internalized_name;
  if (StringTable::InternalizeStringIfExists(name->GetIsolate(), name)
          .ToHandle(&internalized_name)) {
    int index = Hash(*data, *name);
    Key& key = keys_[index];
    DCHECK(key.data == *data);
    DCHECK(key.name->Equals(*name));
    Value result(values_[index]);
    DCHECK(result.mode() == mode);
    DCHECK(result.initialization_flag() == init_flag);
    DCHECK(result.maybe_assigned_flag() == maybe_assigned_flag);
    DCHECK(result.index() + kNotFound == slot_index);
  }
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
