// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_OBJECT_FIELD_INL_H_
#define V8_OBJECTS_HEAP_OBJECT_FIELD_INL_H_

#include "src/objects/heap-object.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/slots-inl.h"
#include "src/sandbox/bounded-size-inl.h"
#include "src/sandbox/cppheap-pointer-inl.h"
#include "src/sandbox/external-pointer-inl.h"
#include "src/sandbox/indirect-pointer-inl.h"
#include "src/sandbox/sandboxed-pointer-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Address HeapObject::ReadSandboxedPointerField(
    size_t offset, PtrComprCageBase cage_base) const {
  return i::ReadSandboxedPointerField(field_address(offset), cage_base);
}

void HeapObject::WriteSandboxedPointerField(size_t offset,
                                            PtrComprCageBase cage_base,
                                            Address value) {
  i::WriteSandboxedPointerField(field_address(offset), cage_base, value);
}

void HeapObject::WriteSandboxedPointerField(size_t offset, Isolate* isolate,
                                            Address value) {
  i::WriteSandboxedPointerField(field_address(offset),
                                PtrComprCageBase(isolate), value);
}

size_t HeapObject::ReadBoundedSizeField(size_t offset) const {
  return i::ReadBoundedSizeField(field_address(offset));
}

void HeapObject::WriteBoundedSizeField(size_t offset, size_t value) {
  i::WriteBoundedSizeField(field_address(offset), value);
}

template <ExternalPointerTag tag>
void HeapObject::InitExternalPointerField(size_t offset,
                                          IsolateForSandbox isolate,
                                          Address value,
                                          WriteBarrierMode mode) {
  i::InitExternalPointerField<tag>(address(), field_address(offset), isolate,
                                   value);
  CONDITIONAL_EXTERNAL_POINTER_WRITE_BARRIER(this, static_cast<int>(offset),
                                             tag, mode);
}

void HeapObject::InitExternalPointerField(size_t offset,
                                          IsolateForSandbox isolate,
                                          ExternalPointerTag tag, Address value,
                                          WriteBarrierMode mode) {
  i::InitExternalPointerField(address(), field_address(offset), isolate, tag,
                              value);
  CONDITIONAL_EXTERNAL_POINTER_WRITE_BARRIER(this, static_cast<int>(offset),
                                             tag, mode);
}

template <ExternalPointerTagRange tag_range>
Address HeapObject::ReadExternalPointerField(size_t offset,
                                             IsolateForSandbox isolate) const {
  return i::ReadExternalPointerField<tag_range>(field_address(offset), isolate);
}

Address HeapObject::ReadExternalPointerField(
    size_t offset, IsolateForSandbox isolate,
    ExternalPointerTagRange tag_range) const {
  return i::ReadExternalPointerField(field_address(offset), isolate, tag_range);
}

template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
Address HeapObject::ReadCppHeapPointerField(
    size_t offset, IsolateForPointerCompression isolate) const {
  return i::ReadCppHeapPointerField<lower_bound, upper_bound>(
      field_address(offset), isolate);
}

Address HeapObject::ReadCppHeapPointerField(
    size_t offset, IsolateForPointerCompression isolate,
    CppHeapPointerTagRange tag_range) const {
  return i::ReadCppHeapPointerField(field_address(offset), isolate, tag_range);
}

template <ExternalPointerTag tag>
void HeapObject::WriteExternalPointerField(size_t offset,
                                           IsolateForSandbox isolate,
                                           Address value) {
  i::WriteExternalPointerField<tag>(field_address(offset), isolate, value);
}

void HeapObject::WriteExternalPointerField(size_t offset,
                                           IsolateForSandbox isolate,
                                           ExternalPointerTag tag,
                                           Address value) {
  i::WriteExternalPointerField(field_address(offset), isolate, tag, value);
}

void HeapObject::SetupLazilyInitializedExternalPointerField(size_t offset) {
#ifdef V8_ENABLE_SANDBOX
  auto location =
      reinterpret_cast<ExternalPointerHandle*>(field_address(offset));
  base::AsAtomic32::Release_Store(location, kNullExternalPointerHandle);
#else
  WriteMaybeUnalignedValue<Address>(field_address(offset), kNullAddress);
#endif  // V8_ENABLE_SANDBOX
}

bool HeapObject::IsLazilyInitializedExternalPointerFieldInitialized(
    size_t offset) const {
#ifdef V8_ENABLE_SANDBOX
  auto location =
      reinterpret_cast<ExternalPointerHandle*>(field_address(offset));
  ExternalPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  return handle != kNullExternalPointerHandle;
#else
  return ReadMaybeUnalignedValue<Address>(field_address(offset)) !=
         kNullAddress;
#endif  // V8_ENABLE_SANDBOX
}

template <ExternalPointerTag tag>
void HeapObject::WriteLazilyInitializedExternalPointerField(
    size_t offset, IsolateForSandbox isolate, Address value) {
  WriteLazilyInitializedExternalPointerField(offset, isolate, value, tag);
}

void HeapObject::WriteLazilyInitializedExternalPointerField(
    size_t offset, IsolateForSandbox isolate, Address value,
    ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kExternalPointerNullTag);
  ExternalPointerTable& table = isolate.GetExternalPointerTableFor(tag);
  auto location =
      reinterpret_cast<ExternalPointerHandle*>(field_address(offset));
  ExternalPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  if (handle == kNullExternalPointerHandle) {
    // Field has not been initialized yet.
    handle = table.AllocateAndInitializeEntry(
        isolate.GetExternalPointerTableSpaceFor(tag, address()), value, tag);
    base::AsAtomic32::Release_Store(location, handle);
    // In this case, we're adding a reference from an existing object to a new
    // table entry, so we always require a write barrier.
    EXTERNAL_POINTER_WRITE_BARRIER(this, static_cast<int>(offset), tag);
  } else {
    table.Set(handle, value, tag);
  }
#else
  WriteMaybeUnalignedValue<Address>(field_address(offset), value);
#endif  // V8_ENABLE_SANDBOX
}

void HeapObject::SetupLazilyInitializedCppHeapPointerField(size_t offset) {
  CppHeapPointerSlot(field_address(offset)).init();
}

void HeapObject::WriteLazilyInitializedCppHeapPointerField(
    size_t offset, IsolateForPointerCompression isolate, Address value,
    CppHeapPointerTag tag) {
  i::WriteLazilyInitializedCppHeapPointerField(field_address(offset), isolate,
                                               value, tag);
}

ObjectSlot HeapObject::RawField(int byte_offset) const {
  return ObjectSlot(field_address(byte_offset));
}

MaybeObjectSlot HeapObject::RawMaybeWeakField(int byte_offset) const {
  return MaybeObjectSlot(field_address(byte_offset));
}

InstructionStreamSlot HeapObject::RawInstructionStreamField(
    int byte_offset) const {
  return InstructionStreamSlot(field_address(byte_offset));
}

ExternalPointerSlot HeapObject::RawExternalPointerField(
    int byte_offset, ExternalPointerTagRange tag_range) const {
  return ExternalPointerSlot(field_address(byte_offset), tag_range);
}

CppHeapPointerSlot HeapObject::RawCppHeapPointerField(int byte_offset) const {
  return CppHeapPointerSlot(field_address(byte_offset));
}

IndirectPointerSlot HeapObject::RawIndirectPointerField(
    int byte_offset, IndirectPointerTagRange tag_range) const {
  return IndirectPointerSlot(field_address(byte_offset), tag_range);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_OBJECT_FIELD_INL_H_
