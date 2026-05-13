// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SORT_STATE_H_
#define V8_OBJECTS_SORT_STATE_H_

#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-objects.h"
#include "src/objects/map.h"
#include "src/objects/oddball.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/third_party/v8/builtins/array-sort-tq.inc"

// SortState's fields are accessed exclusively from Torque-generated code
// (third_party/v8/builtins/array-sort.tq and src/builtins/array-to-sorted.tq);
// no C++ accessors are needed.
V8_OBJECT class SortState : public HeapObject {
 public:
  DECL_PRINTER(SortState)
  DECL_VERIFIER(SortState)

  class BodyDescriptor;

  static constexpr int SizeFor() { return sizeof(SortState); }

  TaggedMember<JSReceiver> receiver_;
  TaggedMember<Map> initial_receiver_map_;
  TaggedMember<Number> initial_receiver_length_;
  TaggedMember<Object> user_cmp_fn_;  // Undefined|Callable
  TaggedMember<Boolean> is_reset_to_generic_;
  TaggedMember<Smi> min_gallop_;
  TaggedMember<Smi> pending_runs_size_;
  TaggedMember<FixedArray> pending_runs_;
  TaggedMember<FixedArray> pending_powers_;
  TaggedMember<FixedArray> work_array_;
  TaggedMember<FixedArray> temp_array_;
  TaggedMember<Smi> sort_length_;
  TaggedMember<Smi> number_of_undefined_;
} V8_OBJECT_END;

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SORT_STATE_H_
