// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bigint/bigint-internal.h"
#include "src/bigint/vector-arithmetic.h"

namespace v8 {
namespace bigint {

// The classic algorithm: for every part, multiply the accumulator with
// the appropriate multiplier, and add the part. O(n²) overall.
void ProcessorImpl::FromStringClassic(RWDigits Z,
                                      FromStringAccumulator* accumulator) {
  // We always have at least one part to process.
  DCHECK(accumulator->stack_parts_used_ > 0);  // NOLINT(readability/check)
  Z[0] = accumulator->stack_parts_[0];
  RWDigits already_set(Z, 0, 1);
  for (int i = 1; i < Z.len(); i++) Z[i] = 0;

  // The {FromStringAccumulator} uses stack-allocated storage for the first
  // few parts; if heap storage is used at all then all parts are copied there.
  int num_stack_parts = accumulator->stack_parts_used_;
  if (num_stack_parts == 1) return;
  const std::vector<digit_t>& heap_parts = accumulator->heap_parts_;
  int num_heap_parts = static_cast<int>(heap_parts.size());
  // All multipliers are the same, except possibly for the last.
  const digit_t max_multiplier = accumulator->max_multiplier_;

  if (num_heap_parts == 0) {
    for (int i = 1; i < num_stack_parts - 1; i++) {
      MultiplySingle(Z, already_set, max_multiplier);
      Add(Z, accumulator->stack_parts_[i]);
      already_set.set_len(already_set.len() + 1);
    }
    MultiplySingle(Z, already_set, accumulator->last_multiplier_);
    Add(Z, accumulator->stack_parts_[num_stack_parts - 1]);
    return;
  }
  // Parts are stored on the heap.
  for (int i = 1; i < num_heap_parts - 1; i++) {
    MultiplySingle(Z, already_set, max_multiplier);
    if (should_terminate()) return;
    Add(Z, accumulator->heap_parts_[i]);
    already_set.set_len(already_set.len() + 1);
  }
  MultiplySingle(Z, already_set, accumulator->last_multiplier_);
  Add(Z, accumulator->heap_parts_.back());
}

void ProcessorImpl::FromString(RWDigits Z, FromStringAccumulator* accumulator) {
  if (accumulator->inline_everything_) {
    int i = 0;
    for (; i < accumulator->stack_parts_used_; i++) {
      Z[i] = accumulator->stack_parts_[i];
    }
    for (; i < Z.len(); i++) Z[i] = 0;
  } else if (accumulator->stack_parts_used_ == 0) {
    for (int i = 0; i < Z.len(); i++) Z[i] = 0;
  } else {
    FromStringClassic(Z, accumulator);
  }
}

Status Processor::FromString(RWDigits Z, FromStringAccumulator* accumulator) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  impl->FromString(Z, accumulator);
  return impl->get_and_clear_status();
}

}  // namespace bigint
}  // namespace v8
