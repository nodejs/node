// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_ISOLATE_INL_H_
#define V8_SANDBOX_ISOLATE_INL_H_

#include "src/sandbox/isolate.h"
// Include the non-inl header before the rest of the headers.

#include "src/execution/isolate-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/objects/heap-object.h"
#include "src/sandbox/external-pointer-table-inl.h"
#include "src/sandbox/indirect-pointer-tag.h"

namespace v8::internal {

#ifdef V8_ENABLE_SANDBOX

ExternalPointerTable& IsolateForSandbox::GetExternalPointerTableFor(
    ExternalPointerTagRange tag_range) {
  IsolateForPointerCompression isolate(isolate_);
  return isolate.GetExternalPointerTableFor(tag_range);
}

ExternalPointerTable::Space* IsolateForSandbox::GetExternalPointerTableSpaceFor(
    ExternalPointerTagRange tag_range, Address host) {
  IsolateForPointerCompression isolate(isolate_);
  return isolate.GetExternalPointerTableSpaceFor(tag_range, host);
}

CodePointerTable::Space* IsolateForSandbox::GetCodePointerTableSpaceFor(
    Address owning_slot) {
  return ReadOnlyHeap::Contains(owning_slot)
             ? isolate_->read_only_heap()->code_pointer_space()
             : isolate_->heap()->code_pointer_space();
}

TrustedPointerTable& IsolateForSandbox::GetTrustedPointerTableFor(
    IndirectPointerTag tag) {
  return IsSharedTrustedPointerType(tag)
             ? isolate_->shared_trusted_pointer_table()
             : isolate_->trusted_pointer_table();
}

TrustedPointerTable::Space* IsolateForSandbox::GetTrustedPointerTableSpaceFor(
    IndirectPointerTag tag) {
  return IsSharedTrustedPointerType(tag)
             ? isolate_->shared_trusted_pointer_space()
             : isolate_->heap()->trusted_pointer_space();
}

ExternalPointerTag IsolateForSandbox::GetExternalPointerTableTagFor(
    Tagged<HeapObject> witness, ExternalPointerHandle handle) {
  DCHECK(!HeapLayout::InWritableSharedSpace(witness));
  return isolate_->external_pointer_table().GetTag(handle);
}

bool IsolateForSandbox::SharesPointerTablesWith(IsolateForSandbox other) const {
  return &isolate_->shared_trusted_pointer_table() ==
             &other.isolate_->shared_trusted_pointer_table() &&
         &isolate_->shared_external_pointer_table() ==
             &other.isolate_->shared_external_pointer_table() &&
         isolate_->shared_external_pointer_space() ==
             other.isolate_->shared_external_pointer_space();
}

V8_INLINE IsolateForSandbox GetIsolateForSandbox(Tagged<HeapObject> object) {
  // This method can be used on shared objects as opposed to
  // GetHeapFromWritableObject because it only returns IsolateForSandbox instead
  // of the Isolate. This is because shared objects will go to shared external
  // pointer table which is the same for main and all worker isolates.
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  Isolate* isolate = Isolate::FromHeap(chunk->GetHeap());
  // Until we replace all GetIsolateForSandbox calls by
  // GetCurrentIsolateForSandbox, do check that both return the same (or
  // compatible, in case of shared isolate) isolates.
  // See https://crbug.com/396607238 for context.
  bool allow_isolate_sharing = V8_UNLIKELY(v8_flags.shared_heap);
  if (V8_UNLIKELY(HeapLayout::InWritableSharedSpace(object))) {
    // The only shared objects so far are shared strings, if enabled (off by
    // default). Rethink this check once we have more shared objects.
    CHECK(v8_flags.shared_string_table);
    CHECK(IsString(object));
    CHECK(isolate->is_shared_space_isolate());
    allow_isolate_sharing = true;
  }
  if (allow_isolate_sharing) {
    // See the TODO above: The isolate returned here must match TLS.
    CHECK(IsolateForSandbox{isolate}.SharesPointerTablesWith(
        Isolate::TryGetCurrent()));
  } else {
    // See the TODO above: The isolate returned here must match TLS.
    CHECK_EQ(isolate, Isolate::TryGetCurrent());
  }
  return isolate;
}

V8_INLINE IsolateForSandbox GetCurrentIsolateForSandbox() {
  return Isolate::Current();
}

#endif  // V8_ENABLE_SANDBOX

#ifdef V8_COMPRESS_POINTERS

ExternalPointerTable& IsolateForPointerCompression::GetExternalPointerTableFor(
    ExternalPointerTagRange tag_range) {
  DCHECK(!tag_range.IsEmpty());
  return IsSharedExternalPointerType(tag_range)
             ? isolate_->shared_external_pointer_table()
             : isolate_->external_pointer_table();
}

ExternalPointerTable::Space*
IsolateForPointerCompression::GetExternalPointerTableSpaceFor(
    ExternalPointerTagRange tag_range, Address host) {
  DCHECK(!tag_range.IsEmpty());

  if (V8_UNLIKELY(IsSharedExternalPointerType(tag_range))) {
    DCHECK(!ReadOnlyHeap::Contains(host));
    return isolate_->shared_external_pointer_space();
  }

  if (V8_UNLIKELY(IsMaybeReadOnlyExternalPointerType(tag_range) &&
                  ReadOnlyHeap::Contains(host))) {
    return isolate_->heap()->read_only_external_pointer_space();
  }

  if (HeapLayout::InYoungGeneration(HeapObject::FromAddress(host))) {
    return isolate_->heap()->young_external_pointer_space();
  }

  return isolate_->heap()->old_external_pointer_space();
}

CppHeapPointerTable& IsolateForPointerCompression::GetCppHeapPointerTable() {
  return isolate_->cpp_heap_pointer_table();
}

CppHeapPointerTable::Space*
IsolateForPointerCompression::GetCppHeapPointerTableSpace() {
  return isolate_->heap()->cpp_heap_pointer_space();
}

#endif  // V8_COMPRESS_POINTERS

}  // namespace v8::internal

#endif  // V8_SANDBOX_ISOLATE_INL_H_
