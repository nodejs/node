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

inline void UseRegister(Input input) {
  input.location()->SetUnallocated(
      compiler::UnallocatedOperand::MUST_HAVE_REGISTER,
      compiler::UnallocatedOperand::USED_AT_END, kNoVreg);
}
inline void UseAndClobberRegister(Input input) {
  input.location()->SetUnallocated(
      compiler::UnallocatedOperand::MUST_HAVE_REGISTER,
      compiler::UnallocatedOperand::USED_AT_START, kNoVreg);
}
inline void UseAny(Input input) {
  input.location()->SetUnallocated(
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
      compiler::UnallocatedOperand::USED_AT_END, kNoVreg);
}
inline void UseFixed(Input input, Register reg) {
  input.location()->SetUnallocated(compiler::UnallocatedOperand::FIXED_REGISTER,
                                   reg.code(), kNoVreg);
  input.node()->SetHint(input.operand());
}
inline void UseFixed(Input input, DoubleRegister reg) {
  input.location()->SetUnallocated(
      compiler::UnallocatedOperand::FIXED_FP_REGISTER, reg.code(), kNoVreg);
  input.node()->SetHint(input.operand());
}

CallKnownJSFunction::CallKnownJSFunction(
    uint64_t bitfield,
    JSDispatchHandle dispatch_handle,
    compiler::SharedFunctionInfoRef shared_function_info, ValueNode* closure,
    ValueNode* context, ValueNode* receiver, ValueNode* new_target,
    const compiler::FeedbackSource& feedback_source)
    : Base(bitfield),
      dispatch_handle_(dispatch_handle),
      shared_function_info_(shared_function_info),
      expected_parameter_count_(
          IsolateGroup::current()->js_dispatch_table()->GetParameterCount(
              dispatch_handle)
              ),
      feedback_source_(feedback_source) {
  set_input(kTargetIndex, closure);
  set_input(kContextIndex, context);
  set_input(kReceiverIndex, receiver);
  set_input(kNewTargetIndex, new_target);
}

void NodeBase::UnwrapDeoptFrames() {
  // Unwrap (and remove uses of its inputs) of Identity and ReturnedValue.
  if (properties().can_eager_deopt() || properties().is_deopt_checkpoint()) {
    eager_deopt_info()->Unwrap();
  }
  if (properties().can_lazy_deopt()) {
    lazy_deopt_info()->Unwrap();
  }
}

void NodeBase::ClearInputs() {
  for (Input input : inputs()) {
    input.clear();
  }
}

void NodeBase::OverwriteWith(Opcode new_opcode,
                             std::optional<OpProperties> maybe_new_properties) {
  OpProperties new_properties = maybe_new_properties.has_value()
                                    ? maybe_new_properties.value()
                                    : StaticPropertiesForOpcode(new_opcode);
#ifdef DEBUG
  CheckCanOverwriteWith(new_opcode, new_properties);
#endif
  set_opcode(new_opcode);
  set_properties(new_properties);
  if (new_opcode == Opcode::kDead) return;
  int new_input_count = StaticInputCountForOpcode(new_opcode);
  if (input_count() != new_input_count) {
    bitfield_ = InputCountField::update(bitfield_, new_input_count);
  }
}

template <typename NodeT, typename... Args>
NodeT* NodeBase::OverwriteWith(Args&&... args) {
#ifdef DEBUG
  CheckCanOverwriteWith(opcode_of<NodeT>, NodeT::kProperties);
#endif
  uint64_t bitfield = OpcodeField::encode(opcode_of<NodeT>) |
                      OpPropertiesField::encode(NodeT::kProperties) |
                      InputCountField::encode(NodeT::kInputCount);
  return new (this) NodeT(bitfield, std::forward<Args>(args)...);
}

void NodeBase::OverwriteWithIdentityTo(ValueNode* node) {
  // OverwriteWith() checks if the node we're overwriting to has the same
  // input count and the same properties. Here we don't need to do that, since
  // overwriting with a node with property pure, we only need to check if
  // there is at least 1 input. Since the first input is always the one
  // closest to the input_base().
  DCHECK_GE(input_count(), 1);
  // Remove use of all inputs first.
  ClearInputs();
  // Unfortunately we cannot remove uses from deopt frames, since these could be
  // shared with other nodes. But we can remove uses from Identity and
  // ReturnedValue nodes.
  UnwrapDeoptFrames();

  set_opcode(Opcode::kIdentity);
  set_properties(StaticPropertiesForOpcode(Opcode::kIdentity));
  bitfield_ = InputCountField::update(bitfield_, 1);
  set_input(0, node);
}

void NodeBase::OverwriteWithReturnValue(ValueNode* node) {
  DCHECK_EQ(opcode(), Opcode::kCallKnownJSFunction);
  // This node might eventually be overwritten by conversion nodes which need
  // a register snapshot.
  DCHECK(properties().needs_register_snapshot());
  if (node->is_tagged()) {
    return OverwriteWithIdentityTo(node);
  }

  DCHECK_GE(input_count(), 1);
  // Remove use of all inputs first.
  ClearInputs();
  // Unfortunately we cannot remove uses from deopt frames, since these could be
  // shared with other nodes. But we can remove uses from Identity and
  // ReturnedValue nodes.
  UnwrapDeoptFrames();

  RegisterSnapshot registers = register_snapshot();
  set_opcode(Opcode::kReturnedValue);
  set_properties(StaticPropertiesForOpcode(Opcode::kReturnedValue));
  bitfield_ = InputCountField::update(bitfield_, 1);
  // After updating the input count, the possition of the register snapshot is
  // wrong. We simply write a copy to the new location.
  set_register_snapshot(registers);
  set_input(0, node);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_INL_H_
