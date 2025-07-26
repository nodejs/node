// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_IR_INL_H_
#define V8_MAGLEV_MAGLEV_IR_INL_H_

#include "src/maglev/maglev-ir.h"
// Include the non-inl header before the rest of the headers.

#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-deopt-frame-visitor.h"
#include "src/sandbox/js-dispatch-table-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

#ifdef DEBUG
inline RegList GetGeneralRegistersUsedAsInputs(
    const EagerDeoptInfo* deopt_info) {
  RegList regs;
  InputLocation* input = deopt_info->input_locations();
  deopt_info->ForEachInput([&regs, &input](ValueNode* value) {
    if (input->IsGeneralRegister()) {
      regs.set(input->AssignedGeneralRegister());
    }
    input++;
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

CallKnownJSFunction::CallKnownJSFunction(
    uint64_t bitfield,
#ifdef V8_ENABLE_LEAPTIERING
    JSDispatchHandle dispatch_handle,
#endif
    compiler::SharedFunctionInfoRef shared_function_info, ValueNode* closure,
    ValueNode* context, ValueNode* receiver, ValueNode* new_target)
    : Base(bitfield),
#ifdef V8_ENABLE_LEAPTIERING
      dispatch_handle_(dispatch_handle),
#endif
      shared_function_info_(shared_function_info),
      expected_parameter_count_(
#ifdef V8_ENABLE_LEAPTIERING
          IsolateGroup::current()->js_dispatch_table()->GetParameterCount(
              dispatch_handle)
#else
          shared_function_info
              .internal_formal_parameter_count_with_receiver_deprecated()
#endif
      ) {
  set_input(kClosureIndex, closure);
  set_input(kContextIndex, context);
  set_input(kReceiverIndex, receiver);
  set_input(kNewTargetIndex, new_target);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_INL_H_
