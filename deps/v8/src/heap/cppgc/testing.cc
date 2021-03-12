// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/testing.h"

#include "src/base/logging.h"
#include "src/heap/cppgc/heap-base.h"

namespace cppgc {
namespace testing {

OverrideEmbedderStackStateScope::OverrideEmbedderStackStateScope(
    HeapHandle& heap_handle, EmbedderStackState state)
    : heap_handle_(heap_handle) {
  auto& heap = internal::HeapBase::From(heap_handle_);
  CHECK_NULL(heap.override_stack_state_.get());
  heap.override_stack_state_ = std::make_unique<EmbedderStackState>(state);
}

OverrideEmbedderStackStateScope::~OverrideEmbedderStackStateScope() {
  auto& heap = internal::HeapBase::From(heap_handle_);
  heap.override_stack_state_.reset();
}

}  // namespace testing
}  // namespace cppgc
