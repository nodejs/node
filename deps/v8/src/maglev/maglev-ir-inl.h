// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_IR_INL_H_
#define V8_MAGLEV_MAGLEV_IR_INL_H_

#include <type_traits>

#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace detail {

// A little bit of template magic to allow DeepForEachInput to either take a
// `const DeoptInfo*` or a `DeoptInfo*`, depending on whether the Function
// processes read-only `ValueNode*` or read-write `ValueNode*&`.
template <typename F, typename Ret, typename A, typename... Rest>
A first_argument_helper(Ret (F::*)(A, Rest...));

template <typename F, typename Ret, typename A, typename... Rest>
A first_argument_helper(Ret (F::*)(A, Rest...) const);

template <typename Function>
using first_argument = decltype(first_argument_helper(
    &std::remove_reference_t<Function>::operator()));

template <typename T, typename Function>
using const_if_function_first_arg_not_reference =
    std::conditional_t<std::is_reference_v<first_argument<Function>>, T,
                       const T>;

template <typename Function>
void DeepForEachInputSingleFrameImpl(
    const_if_function_first_arg_not_reference<DeoptFrame, Function>& frame,
    InputLocation*& input_location, Function&& f,
    std::function<bool(interpreter::Register)> is_result_register) {
  switch (frame.type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      f(frame.as_interpreted().closure(), input_location);
      frame.as_interpreted().frame_state()->ForEachValue(
          frame.as_interpreted().unit(),
          [&](first_argument<Function> node, interpreter::Register reg) {
            // Skip over the result location for lazy deopts, since it is
            // irrelevant for lazy deopts (unoptimized code will recreate the
            // result).
            if (is_result_register(reg)) return;
            f(node, input_location);
          });
      break;
    case DeoptFrame::FrameType::kInlinedArgumentsFrame: {
      // The inlined arguments frame can never be the top frame.
      f(frame.as_inlined_arguments().closure(), input_location);
      for (first_argument<Function> node :
           frame.as_inlined_arguments().arguments()) {
        f(node, input_location);
      }
      break;
    }
    case DeoptFrame::FrameType::kConstructInvokeStubFrame: {
      f(frame.as_construct_stub().receiver(), input_location);
      f(frame.as_construct_stub().context(), input_location);
      break;
    }
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      for (first_argument<Function> node :
           frame.as_builtin_continuation().parameters()) {
        f(node, input_location);
      }
      f(frame.as_builtin_continuation().context(), input_location);
      break;
  }
}

template <typename Function>
void DeepForCapturedAllocation(const CapturedAllocation& alloc,
                               InputLocation*& input_location, Function&& f) {
  if (alloc.type != CapturedAllocation::kObject) return;
  for (CapturedValue& value : alloc.object) {
    DCHECK_NE(value.type, CapturedValue::kCapturedObject);
    DCHECK_NE(value.type, CapturedValue::kFixedDoubleArray);
    if (value.type == CapturedValue::kRuntimeValue) {
      f(value.runtime_value, input_location, f);
    }
  }
}

template <typename Function>
void DeepForEachInputAndDeoptObject(
    const_if_function_first_arg_not_reference<DeoptFrame, Function>& frame,
    InputLocation*& input_location, Function&& f,
    std::function<bool(interpreter::Register)> is_result_register =
        [](interpreter::Register) { return false; }) {
  auto update_node = [&f](first_argument<Function> node,
                          InputLocation*& input_location) {
    auto lambda = [&f](first_argument<Function> node,
                       InputLocation*& input_location, const auto& lambda) {
      size_t input_locations_to_advance = 1;
      if (const_if_function_first_arg_not_reference<InlinedAllocation,
                                                    Function>* alloc =
              node->template TryCast<InlinedAllocation>()) {
        if (alloc->HasBeenAnalysed() && alloc->HasBeenElided()) {
          input_location++;  // Reserved for the inlined allocation.
          return DeepForCapturedAllocation(alloc->captured_allocation(),
                                           input_location, lambda);
        }
        input_locations_to_advance +=
            alloc->captured_allocation().InputLocationSizeNeeded();
      }
      f(node, input_location);
      input_location += input_locations_to_advance;
    };
    lambda(node, input_location, lambda);
  };
  DeepForEachInputSingleFrameImpl(frame, input_location, update_node,
                                  is_result_register);
}

template <typename Function>
void DeepForEachInputImpl(
    const_if_function_first_arg_not_reference<DeoptFrame, Function>& frame,
    InputLocation*& input_location, Function&& f) {
  if (frame.parent()) {
    DeepForEachInputImpl(*frame.parent(), input_location, f);
  }
  DeepForEachInputAndDeoptObject(frame, input_location, f);
}

template <typename Function>
void DeepForEachInput(const_if_function_first_arg_not_reference<
                          EagerDeoptInfo, Function>* deopt_info,
                      Function&& f) {
  InputLocation* input_location = deopt_info->input_locations();
  DeepForEachInputImpl(deopt_info->top_frame(), input_location,
                       std::forward<Function>(f));
}

template <typename Function>
void DeepForEachInput(const_if_function_first_arg_not_reference<
                          LazyDeoptInfo, Function>* deopt_info,
                      Function&& f) {
  InputLocation* input_location = deopt_info->input_locations();
  auto& top_frame = deopt_info->top_frame();
  if (top_frame.parent()) {
    DeepForEachInputImpl(*top_frame.parent(), input_location, f);
  }
  DeepForEachInputAndDeoptObject(top_frame, input_location, f,
                                 [deopt_info](interpreter::Register reg) {
                                   return deopt_info->IsResultRegister(reg);
                                 });
}

}  // namespace detail

#ifdef DEBUG
inline RegList GetGeneralRegistersUsedAsInputs(
    const EagerDeoptInfo* deopt_info) {
  RegList regs;
  detail::DeepForEachInput(deopt_info,
                           [&regs](ValueNode* value, InputLocation* input) {
                             if (input->IsGeneralRegister()) {
                               regs.set(input->AssignedGeneralRegister());
                             }
                           });
  return regs;
}
#endif  // DEBUG

// Helper macro for checking that a reglist is empty which prints the contents
// when non-empty.
#define DCHECK_REGLIST_EMPTY(...) DCHECK_EQ((__VA_ARGS__), RegList{})

// ---
// Value location constraint setting helpers.
// ---

static constexpr int kNoVreg = -1;

inline void DefineAsRegister(Node* node) {
  node->result().SetUnallocated(
      compiler::UnallocatedOperand::MUST_HAVE_REGISTER, kNoVreg);
}
inline void DefineAsConstant(Node* node) {
  node->result().SetUnallocated(compiler::UnallocatedOperand::NONE, kNoVreg);
}

inline void DefineAsFixed(Node* node, Register reg) {
  node->result().SetUnallocated(compiler::UnallocatedOperand::FIXED_REGISTER,
                                reg.code(), kNoVreg);
}

// TODO(v8:7700): Create generic DefineSameAs(..., int input).
inline void DefineSameAsFirst(Node* node) {
  node->result().SetUnallocated(kNoVreg, 0);
}

inline void UseRegister(Input& input) {
  input.SetUnallocated(compiler::UnallocatedOperand::MUST_HAVE_REGISTER,
                       compiler::UnallocatedOperand::USED_AT_END, kNoVreg);
}
inline void UseAndClobberRegister(Input& input) {
  input.SetUnallocated(compiler::UnallocatedOperand::MUST_HAVE_REGISTER,
                       compiler::UnallocatedOperand::USED_AT_START, kNoVreg);
}
inline void UseAny(Input& input) {
  input.SetUnallocated(
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
      compiler::UnallocatedOperand::USED_AT_END, kNoVreg);
}
inline void UseFixed(Input& input, Register reg) {
  input.SetUnallocated(compiler::UnallocatedOperand::FIXED_REGISTER, reg.code(),
                       kNoVreg);
  input.node()->SetHint(input.operand());
}
inline void UseFixed(Input& input, DoubleRegister reg) {
  input.SetUnallocated(compiler::UnallocatedOperand::FIXED_FP_REGISTER,
                       reg.code(), kNoVreg);
  input.node()->SetHint(input.operand());
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_INL_H_
