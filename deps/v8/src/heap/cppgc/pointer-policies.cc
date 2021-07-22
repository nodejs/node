// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/pointer-policies.h"

#include "include/cppgc/internal/caged-heap-local-data.h"
#include "include/cppgc/internal/persistent-node.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/page-memory.h"
#include "src/heap/cppgc/process-heap.h"

namespace cppgc {
namespace internal {

namespace {

#if defined(DEBUG)
bool IsOnStack(const void* address) {
  return v8::base::Stack::GetCurrentStackPosition() <= address &&
         address < v8::base::Stack::GetStackStart();
}
#endif  // defined(DEBUG)

}  // namespace

void EnabledCheckingPolicy::CheckPointerImpl(const void* ptr,
                                             bool points_to_payload) {
  // `ptr` must not reside on stack.
  DCHECK(!IsOnStack(ptr));
  auto* base_page = BasePage::FromPayload(ptr);
  // Large objects do not support mixins. This also means that `base_page` is
  // valid for large objects.
  DCHECK_IMPLIES(base_page->is_large(), points_to_payload);

  // References cannot change their heap association which means that state is
  // immutable once it is set.
  if (!heap_) {
    heap_ = base_page->heap();
    if (!heap_->page_backend()->Lookup(reinterpret_cast<Address>(this))) {
      // If `this` is not contained within the heap of `ptr`, we must deal with
      // an on-stack or off-heap reference. For both cases there should be no
      // heap registered.
      CHECK(!HeapRegistry::TryFromManagedPointer(this));
    }
  }

  // Member references should never mix heaps.
  DCHECK_EQ(heap_, base_page->heap());

  // Header checks.
  const HeapObjectHeader* header = nullptr;
  if (points_to_payload) {
    header = &HeapObjectHeader::FromObject(ptr);
  } else if (!heap_->sweeper().IsSweepingInProgress()) {
    // Mixin case.
    header = &base_page->ObjectHeaderFromInnerAddress(ptr);
    DCHECK_LE(header->ObjectStart(), ptr);
    DCHECK_GT(header->ObjectEnd(), ptr);
  }
  if (header) {
    DCHECK(!header->IsFree());
  }

  // TODO(v8:11749): Check mark bits when during pre-finalizer phase.
}

PersistentRegion& StrongPersistentPolicy::GetPersistentRegion(
    const void* object) {
  auto* heap = BasePage::FromPayload(object)->heap();
  return heap->GetStrongPersistentRegion();
}

PersistentRegion& WeakPersistentPolicy::GetPersistentRegion(
    const void* object) {
  auto* heap = BasePage::FromPayload(object)->heap();
  return heap->GetWeakPersistentRegion();
}

CrossThreadPersistentRegion&
StrongCrossThreadPersistentPolicy::GetPersistentRegion(const void* object) {
  auto* heap = BasePage::FromPayload(object)->heap();
  return heap->GetStrongCrossThreadPersistentRegion();
}

CrossThreadPersistentRegion&
WeakCrossThreadPersistentPolicy::GetPersistentRegion(const void* object) {
  auto* heap = BasePage::FromPayload(object)->heap();
  return heap->GetWeakCrossThreadPersistentRegion();
}

}  // namespace internal
}  // namespace cppgc
