// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TRUSTED_OBJECT_INL_H_
#define V8_OBJECTS_TRUSTED_OBJECT_INL_H_

#include "src/objects/trusted-object.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/trusted-pointer-inl.h"
#include "src/sandbox/indirect-pointer-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
Tagged<T> TrustedObject::ReadProtectedPointerField(int offset) const {
  return TaggedField<T, 0, TrustedSpaceCompressionScheme>::load(this, offset);
}

template <typename T>
Tagged<T> TrustedObject::ReadProtectedPointerField(int offset,
                                                   AcquireLoadTag) const {
  return TaggedField<T, 0, TrustedSpaceCompressionScheme>::Acquire_Load(this,
                                                                        offset);
}

void TrustedObject::WriteProtectedPointerField(int offset,
                                               Tagged<TrustedObject> value) {
  TaggedField<TrustedObject, 0, TrustedSpaceCompressionScheme>::store(
      this, offset, value);
}

void TrustedObject::WriteProtectedPointerField(int offset,
                                               Tagged<TrustedObject> value,
                                               ReleaseStoreTag) {
  TaggedField<TrustedObject, 0, TrustedSpaceCompressionScheme>::Release_Store(
      this, offset, value);
}

bool TrustedObject::IsProtectedPointerFieldEmpty(int offset) const {
  return TaggedField<Object, 0, TrustedSpaceCompressionScheme>::load(
             this, offset) == Smi::zero();
}

bool TrustedObject::IsProtectedPointerFieldEmpty(int offset,
                                                 AcquireLoadTag) const {
  return TaggedField<Object, 0, TrustedSpaceCompressionScheme>::Acquire_Load(
             this, offset) == Smi::zero();
}

void TrustedObject::ClearProtectedPointerField(int offset) {
  TaggedField<Object, 0, TrustedSpaceCompressionScheme>::store(this, offset,
                                                               Smi::zero());
}

void TrustedObject::ClearProtectedPointerField(int offset, ReleaseStoreTag) {
  TaggedField<Object, 0, TrustedSpaceCompressionScheme>::Release_Store(
      this, offset, Smi::zero());
}

ProtectedPointerSlot TrustedObject::RawProtectedPointerField(
    int byte_offset) const {
  return ProtectedPointerSlot(field_address(byte_offset));
}

ProtectedMaybeObjectSlot TrustedObject::RawProtectedMaybeObjectField(
    int byte_offset) const {
  return ProtectedMaybeObjectSlot(field_address(byte_offset));
}

#ifdef VERIFY_HEAP
void TrustedObject::VerifyProtectedPointerField(Isolate* isolate, int offset) {
  Object::VerifyPointer(isolate, ReadProtectedPointerField(offset));
}
#endif

void ExposedTrustedObject::InitAndPublish(Isolate* isolate) {
#ifdef V8_ENABLE_SANDBOX
  InitSelfIndirectPointerField(&self_indirect_pointer_, isolate,
                               isolate->trusted_pointer_publishing_scope());
#endif
}

void ExposedTrustedObject::InitAndPublish(LocalIsolate* isolate) {
#ifdef V8_ENABLE_SANDBOX
  // Background threads using LocalIsolates don't use
  // TrustedPointerPublishingScopes.
  InitSelfIndirectPointerField(&self_indirect_pointer_, isolate, nullptr);
#endif
}

void ExposedTrustedObject::InitDontPublish(Isolate* isolate) {
#ifdef V8_ENABLE_SANDBOX
  i::InitSelfIndirectPointerField(
      reinterpret_cast<Address>(&self_indirect_pointer_), isolate, this,
      kUnpublishedIndirectPointerTag, nullptr);
#endif
}

void ExposedTrustedObject::InitDontPublish(LocalIsolate* isolate) {
#ifdef V8_ENABLE_SANDBOX
  i::InitSelfIndirectPointerField(
      reinterpret_cast<Address>(&self_indirect_pointer_), isolate, this,
      kUnpublishedIndirectPointerTag, nullptr);
#endif
}

void ExposedTrustedObject::Publish(IsolateForSandbox isolate) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK(!IsPublished(isolate));
  // Currently only non-shared objects can be unpublished. We could change that
  // in the future, which would probably require a new shared+unpublished tag.
  DCHECK(!HeapLayout::InAnySharedSpace(this));

  InstanceType instance_type = map()->instance_type();
  IndirectPointerTag tag =
      IndirectPointerTagFromInstanceType(instance_type, SharedFlag::kNo);
  IndirectPointerHandle handle =
      self_indirect_pointer_.load(std::memory_order::acquire);
  TrustedPointerTable& table = isolate.GetTrustedPointerTableFor(tag);
  return table.Publish(handle, tag);
#endif
}

void ExposedTrustedObject::Unpublish(IsolateForSandbox isolate) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK(IsPublished(isolate));
  // Currently only non-shared objects can be unpublished. We could change that
  // in the future, which would probably require a new shared+unpublished tag.
  DCHECK(!HeapLayout::InAnySharedSpace(this));

  InstanceType instance_type = map()->instance_type();
  IndirectPointerTag tag =
      IndirectPointerTagFromInstanceType(instance_type, SharedFlag::kNo);
  IndirectPointerHandle handle =
      self_indirect_pointer_.load(std::memory_order::acquire);
  TrustedPointerTable& table = isolate.GetTrustedPointerTableFor(tag);
  table.Unpublish(handle);
#endif  // V8_ENABLE_SANDBOX
}

bool ExposedTrustedObject::IsPublished(IsolateForSandbox isolate) const {
#ifdef V8_ENABLE_SANDBOX
  InstanceType instance_type = map()->instance_type();
  IndirectPointerTag tag =
      IndirectPointerTagFromInstanceType(instance_type, SharedFlag::kNo);
  return !TrustedPointerField::IsTrustedPointerFieldUnpublished(
      this, offsetof(ExposedTrustedObject, self_indirect_pointer_), tag,
      isolate);
#else
  return true;
#endif
}

IndirectPointerHandle ExposedTrustedObject::self_indirect_pointer_handle()
    const {
#ifdef V8_ENABLE_SANDBOX
  return self_indirect_pointer_.load(std::memory_order::relaxed);
#else
  UNREACHABLE();
#endif
}

#if V8_ENABLE_SANDBOX
void ExposedTrustedObject::InitSelfIndirectPointerField(
    std::atomic<IndirectPointerHandle>* field_ptr, IsolateForSandbox isolate,
    TrustedPointerPublishingScope* opt_publishing_scope) {
  InstanceType instance_type = map()->instance_type();
  SharedFlag shared = SharedFlag(HeapLayout::InAnySharedSpace(this));
  IndirectPointerTag tag =
      IndirectPointerTagFromInstanceType(instance_type, shared);
  i::InitSelfIndirectPointerField(reinterpret_cast<Address>(field_ptr), isolate,
                                  this, tag, opt_publishing_scope);
}
#endif  // V8_ENABLE_SANDBOX

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TRUSTED_OBJECT_INL_H_
