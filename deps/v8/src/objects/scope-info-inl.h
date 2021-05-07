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

Object ScopeInfo::get(int index) const {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return get(isolate, index);
}

Object ScopeInfo::get(IsolateRoot isolate, int index) const {
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  return TaggedField<Object>::Relaxed_Load(
      isolate, *this, FixedArray::OffsetOfElementAt(index));
}

void ScopeInfo::set(int index, Smi value) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  DCHECK(Object(value).IsSmi());
  int offset = FixedArray::OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
}

void ScopeInfo::set(int index, Object value, WriteBarrierMode mode) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK(IsScopeInfo());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  int offset = FixedArray::OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

void ScopeInfo::CopyElements(Isolate* isolate, int dst_index, ScopeInfo src,
                             int src_index, int len, WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, length());
  DCHECK_LE(src_index + len, src.length());
  DisallowGarbageCollection no_gc;

  ObjectSlot dst_slot(RawFieldOfElementAt(dst_index));
  ObjectSlot src_slot(src.RawFieldOfElementAt(src_index));
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

ObjectSlot ScopeInfo::RawFieldOfElementAt(int index) {
  return RawField(FixedArray::OffsetOfElementAt(index));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCOPE_INFO_INL_H_
