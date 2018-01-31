// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/context-slot-cache.h"

#include <stdlib.h>

#include "src/ast/scopes.h"
#include "src/bootstrapper.h"
#include "src/objects-inl.h"

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
  DCHECK(name->IsInternalizedString());
  Key& key = keys_[index];
  if (key.data == data && key.name == name) {
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
  DCHECK(name->IsInternalizedString());
  DCHECK_LT(kNotFound, slot_index);
  int index = Hash(*data, *name);
  Key& key = keys_[index];
  key.data = *data;
  key.name = *name;
  // Please note value only takes a uint as index.
  values_[index] =
      Value(mode, init_flag, maybe_assigned_flag, slot_index - kNotFound).raw();
#ifdef DEBUG
  ValidateEntry(data, name, mode, init_flag, maybe_assigned_flag, slot_index);
#endif
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
  DCHECK(name->IsInternalizedString());
  int index = Hash(*data, *name);
  Key& key = keys_[index];
  DCHECK_EQ(key.data, *data);
  DCHECK_EQ(key.name, *name);
  Value result(values_[index]);
  DCHECK_EQ(result.mode(), mode);
  DCHECK_EQ(result.initialization_flag(), init_flag);
  DCHECK_EQ(result.maybe_assigned_flag(), maybe_assigned_flag);
  DCHECK_EQ(result.index() + kNotFound, slot_index);
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
