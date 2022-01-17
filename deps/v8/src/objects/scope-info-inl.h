// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCOPE_INFO_INL_H_
#define V8_OBJECTS_SCOPE_INFO_INL_H_

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/string.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/scope-info-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(ScopeInfo)

bool ScopeInfo::IsAsmModule() const { return IsAsmModuleBit::decode(Flags()); }

bool ScopeInfo::HasSimpleParameters() const {
  return HasSimpleParametersBit::decode(Flags());
}

int ScopeInfo::Flags() const { return flags(); }
int ScopeInfo::ParameterCount() const { return parameter_count(); }
int ScopeInfo::ContextLocalCount() const { return context_local_count(); }

ObjectSlot ScopeInfo::data_start() { return RawField(OffsetOfElementAt(0)); }

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCOPE_INFO_INL_H_
