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
void DeepForEachInputImpl(const DeoptFrame& frame,
                          InputLocation* input_locations, int& index,
                          Function&& f) {
  if (frame.parent()) {
    DeepForEachInputImpl(*frame.parent(), input_locations, index, f);
  }
  switch (frame.type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      frame.as_interpreted().frame_state()->ForEachValue(
          frame.as_interpreted().unit(),
          [&](ValueNode* node, interpreter::Register reg) {
            f(node, &input_locations[index++]);
          });
      break;
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      for (ValueNode* node : frame.as_builtin_continuation().parameters()) {
        f(node, &input_locations[index++]);
      }
      f(frame.as_builtin_continuation().context(), &input_locations[index++]);
      UNREACHABLE();
  }
}

template <typename Function>
void DeepForEachInput(const EagerDeoptInfo* deopt_info, Function&& f) {
  int index = 0;
  DeepForEachInputImpl(deopt_info->top_frame(), deopt_info->input_locations(),
                       index, f);
}

template <typename Function>
void DeepForEachInput(const LazyDeoptInfo* deopt_info, Function&& f) {
  int index = 0;
  InputLocation* input_locations = deopt_info->input_locations();
  const DeoptFrame& top_frame = deopt_info->top_frame();
  if (top_frame.parent()) {
    DeepForEachInputImpl(*top_frame.parent(), input_locations, index, f);
  }
  // Handle the top-of-frame info separately, since we have to skip the result
  // location.
  switch (top_frame.type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      top_frame.as_interpreted().frame_state()->ForEachValue(
          top_frame.as_interpreted().unit(),
          [&](ValueNode* node, interpreter::Register reg) {
            // Skip over the result location since it is irrelevant for lazy
            // deopts (unoptimized code will recreate the result).
            if (deopt_info->IsResultRegister(reg)) return;
            f(node, &input_locations[index++]);
          });
      break;
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      for (ValueNode* node : top_frame.as_builtin_continuation().parameters()) {
        f(node, &input_locations[index++]);
      };
      f(top_frame.as_builtin_continuation().context(),
        &input_locations[index++]);
  }
}

}  // namespace detail

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_INL_H_
