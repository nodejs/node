// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/heap-state.h"

#include "src/heap/cppgc/heap-base.h"

namespace cppgc {
namespace subtle {

// static
bool HeapState::IsMarking(const HeapHandle& heap_handle) {
  const internal::MarkerBase* marker =
      internal::HeapBase::From(heap_handle).marker();
  return marker && marker->IsMarking();
}

// static
bool HeapState::IsSweeping(const HeapHandle& heap_handle) {
  return internal::HeapBase::From(heap_handle).sweeper().IsSweepingInProgress();
}

// static
bool HeapState::IsSweepingOnOwningThread(const HeapHandle& heap_handle) {
  return internal::HeapBase::From(heap_handle)
      .sweeper()
      .IsSweepingOnMutatorThread();
}

// static
bool HeapState::IsInAtomicPause(const HeapHandle& heap_handle) {
  return internal::HeapBase::From(heap_handle).in_atomic_pause();
}

// static
bool HeapState::PreviousGCWasConservative(const HeapHandle& heap_handle) {
  return internal::HeapBase::From(heap_handle).stack_state_of_prev_gc() ==
         EmbedderStackState::kMayContainHeapPointers;
}

}  // namespace subtle
}  // namespace cppgc
