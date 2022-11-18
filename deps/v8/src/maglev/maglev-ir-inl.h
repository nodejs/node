// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_IR_INL_H_
#define V8_MAGLEV_MAGLEV_IR_INL_H_

#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace detail {

template <typename Function>
void DeepForEachInputImpl(const MaglevCompilationUnit& unit,
                          const CheckpointedInterpreterState* state,
                          InputLocation* input_locations, int& index,
                          Function&& f) {
  if (state->parent) {
    DeepForEachInputImpl(*unit.caller(), state->parent, input_locations, index,
                         f);
  }
  state->register_frame->ForEachValue(
      unit, [&](ValueNode* node, interpreter::Register reg) {
        f(node, reg, &input_locations[index++]);
      });
}

template <typename Function>
void DeepForEachInput(const EagerDeoptInfo* deopt_info, Function&& f) {
  int index = 0;
  DeepForEachInputImpl(deopt_info->unit, &deopt_info->state,
                       deopt_info->input_locations, index, f);
}

template <typename Function>
void DeepForEachInput(const LazyDeoptInfo* deopt_info, Function&& f) {
  int index = 0;
  if (deopt_info->state.parent) {
    DeepForEachInputImpl(*deopt_info->unit.caller(), deopt_info->state.parent,
                         deopt_info->input_locations, index, f);
  }
  // Handle the top-of-frame info separately, since we have to skip the result
  // location.
  deopt_info->state.register_frame->ForEachValue(
      deopt_info->unit, [&](ValueNode* node, interpreter::Register reg) {
        // Skip over the result location since it is irrelevant for lazy deopts
        // (unoptimized code will recreate the result).
        if (deopt_info->IsResultRegister(reg)) return;
        f(node, reg, &deopt_info->input_locations[index++]);
      });
}

}  // namespace detail

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_INL_H_
