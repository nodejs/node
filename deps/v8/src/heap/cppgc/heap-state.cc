// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/heap-state.h"

#include "src/heap/cppgc/heap-base.h"

namespace cppgc {
namespace subtle {

// static
bool HeapState::IsMarking(const HeapHandle& heap_handle) {
  const auto& heap_base = internal::HeapBase::From(heap_handle);
  const internal::MarkerBase* marker = heap_base.marker();
  return marker && marker->IsMarking();
}

// static
bool HeapState::IsSweeping(const HeapHandle& heap_handle) {
  const auto& heap_base = internal::HeapBase::From(heap_handle);
  return heap_base.sweeper().IsSweepingInProgress();
}

// static
bool HeapState::IsInAtomicPause(const HeapHandle& heap_handle) {
  const auto& heap_base = internal::HeapBase::From(heap_handle);
  return heap_base.in_atomic_pause();
}

}  // namespace subtle
}  // namespace cppgc
