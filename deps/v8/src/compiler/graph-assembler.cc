// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-assembler.h"

#include "src/code-factory.h"
#include "src/compiler/linkage.h"

namespace v8 {
namespace internal {
namespace compiler {

GraphAssembler::GraphAssembler(JSGraph* jsgraph, Node* effect, Node* control,
                               Zone* zone)
    : temp_zone_(zone),
      jsgraph_(jsgraph),
      current_effect_(effect),
      current_control_(control) {}

Node* GraphAssembler::IntPtrConstant(intptr_t value) {
  return jsgraph()->IntPtrConstant(value);
}

Node* GraphAssembler::Int32Constant(int32_t value) {
  return jsgraph()->Int32Constant(value);
}

Node* GraphAssembler::UniqueInt32Constant(int32_t value) {
  return graph()->NewNode(common()->Int32Constant(value));
}

Node* GraphAssembler::SmiConstant(int32_t value) {
  return jsgraph()->SmiConstant(value);
}

Node* GraphAssembler::Uint32Constant(int32_t value) {
  return jsgraph()->Uint32Constant(value);
}

Node* GraphAssembler::Float64Constant(double value) {
  return jsgraph()->Float64Constant(value);
}

Node* GraphAssembler::HeapConstant(Handle<HeapObject> object) {
  return jsgraph()->HeapConstant(object);
}


Node* GraphAssembler::ExternalConstant(ExternalReference ref) {
  return jsgraph()->ExternalConstant(ref);
}

Node* GraphAssembler::CEntryStubConstant(int result_size) {
  return jsgraph()->CEntryStubConstant(result_size);
}

Node* GraphAssembler::LoadFramePointer() {
  return graph()->NewNode(machine()->LoadFramePointer());
}

#define SINGLETON_CONST_DEF(Name) \
  Node* GraphAssembler::Name() { return jsgraph()->Name(); }
JSGRAPH_SINGLETON_CONSTANT_LIST(SINGLETON_CONST_DEF)
#undef SINGLETON_CONST_DEF

#define PURE_UNOP_DEF(Name)                            \
  Node* GraphAssembler::Name(Node* input) {            \
    return graph()->NewNode(machine()->Name(), input); \
  }
PURE_ASSEMBLER_MACH_UNOP_LIST(PURE_UNOP_DEF)
#undef PURE_UNOP_DEF

#define PURE_BINOP_DEF(Name)                                 \
  Node* GraphAssembler::Name(Node* left, Node* right) {      \
    return graph()->NewNode(machine()->Name(), left, right); \
  }
PURE_ASSEMBLER_MACH_BINOP_LIST(PURE_BINOP_DEF)
#undef PURE_BINOP_DEF

#define CHECKED_BINOP_DEF(Name)                                                \
  Node* GraphAssembler::Name(Node* left, Node* right) {                        \
    return graph()->NewNode(machine()->Name(), left, right, current_control_); \
  }
CHECKED_ASSEMBLER_MACH_BINOP_LIST(CHECKED_BINOP_DEF)
#undef CHECKED_BINOP_DEF

Node* GraphAssembler::Float64RoundDown(Node* value) {
  CHECK(machine()->Float64RoundDown().IsSupported());
  return graph()->NewNode(machine()->Float64RoundDown().op(), value);
}

Node* GraphAssembler::Float64RoundTruncate(Node* value) {
  CHECK(machine()->Float64RoundTruncate().IsSupported());
  return graph()->NewNode(machine()->Float64RoundTruncate().op(), value);
}

Node* GraphAssembler::Projection(int index, Node* value) {
  return graph()->NewNode(common()->Projection(index), value, current_control_);
}

Node* GraphAssembler::Allocate(PretenureFlag pretenure, Node* size) {
  return current_control_ = current_effect_ =
             graph()->NewNode(simplified()->AllocateRaw(Type::Any(), pretenure),
                              size, current_effect_, current_control_);
}

Node* GraphAssembler::LoadField(FieldAccess const& access, Node* object) {
  return current_effect_ =
             graph()->NewNode(simplified()->LoadField(access), object,
                              current_effect_, current_control_);
}

Node* GraphAssembler::LoadElement(ElementAccess const& access, Node* object,
                                  Node* index) {
  return current_effect_ =
             graph()->NewNode(simplified()->LoadElement(access), object, index,
                              current_effect_, current_control_);
}

Node* GraphAssembler::StoreField(FieldAccess const& access, Node* object,
                                 Node* value) {
  return current_effect_ =
             graph()->NewNode(simplified()->StoreField(access), object, value,
                              current_effect_, current_control_);
}

Node* GraphAssembler::StoreElement(ElementAccess const& access, Node* object,
                                   Node* index, Node* value) {
  return current_effect_ =
             graph()->NewNode(simplified()->StoreElement(access), object, index,
                              value, current_effect_, current_control_);
}

Node* GraphAssembler::DebugBreak() {
  return current_effect_ = graph()->NewNode(machine()->DebugBreak(),
                                            current_effect_, current_control_);
}

Node* GraphAssembler::Unreachable() {
  return current_effect_ = graph()->NewNode(common()->Unreachable(),
                                            current_effect_, current_control_);
}

Node* GraphAssembler::Store(StoreRepresentation rep, Node* object, Node* offset,
                            Node* value) {
  return current_effect_ =
             graph()->NewNode(machine()->Store(rep), object, offset, value,
                              current_effect_, current_control_);
}

Node* GraphAssembler::Load(MachineType rep, Node* object, Node* offset) {
  return current_effect_ =
             graph()->NewNode(machine()->Load(rep), object, offset,
                              current_effect_, current_control_);
}

Node* GraphAssembler::Retain(Node* buffer) {
  return current_effect_ =
             graph()->NewNode(common()->Retain(), buffer, current_effect_);
}

Node* GraphAssembler::UnsafePointerAdd(Node* base, Node* external) {
  return current_effect_ =
             graph()->NewNode(machine()->UnsafePointerAdd(), base, external,
                              current_effect_, current_control_);
}

Node* GraphAssembler::ToNumber(Node* value) {
  return current_effect_ =
             graph()->NewNode(ToNumberOperator(), ToNumberBuiltinConstant(),
                              value, NoContextConstant(), current_effect_);
}

Node* GraphAssembler::BitcastWordToTagged(Node* value) {
  return current_effect_ =
             graph()->NewNode(machine()->BitcastWordToTagged(), value,
                              current_effect_, current_control_);
}

Node* GraphAssembler::DeoptimizeIf(DeoptimizeReason reason,
                                   VectorSlotPair const& feedback,
                                   Node* condition, Node* frame_state) {
  return current_control_ = current_effect_ = graph()->NewNode(
             common()->DeoptimizeIf(DeoptimizeKind::kEager, reason, feedback),
             condition, frame_state, current_effect_, current_control_);
}

Node* GraphAssembler::DeoptimizeIfNot(DeoptimizeKind kind,
                                      DeoptimizeReason reason,
                                      VectorSlotPair const& feedback,
                                      Node* condition, Node* frame_state) {
  return current_control_ = current_effect_ = graph()->NewNode(
             common()->DeoptimizeUnless(kind, reason, feedback), condition,
             frame_state, current_effect_, current_control_);
}

Node* GraphAssembler::DeoptimizeIfNot(DeoptimizeReason reason,
                                      VectorSlotPair const& feedback,
                                      Node* condition, Node* frame_state) {
  return DeoptimizeIfNot(DeoptimizeKind::kEager, reason, feedback, condition,
                         frame_state);
}

void GraphAssembler::Branch(Node* condition, GraphAssemblerLabel<0u>* if_true,
                            GraphAssemblerLabel<0u>* if_false) {
  DCHECK_NOT_NULL(current_control_);

  BranchHint hint = BranchHint::kNone;
  if (if_true->IsDeferred() != if_false->IsDeferred()) {
    hint = if_false->IsDeferred() ? BranchHint::kTrue : BranchHint::kFalse;
  }

  Node* branch =
      graph()->NewNode(common()->Branch(hint), condition, current_control_);

  current_control_ = graph()->NewNode(common()->IfTrue(), branch);
  MergeState(if_true);

  current_control_ = graph()->NewNode(common()->IfFalse(), branch);
  MergeState(if_false);

  current_control_ = nullptr;
  current_effect_ = nullptr;
}

// Extractors (should be only used when destructing the assembler.
Node* GraphAssembler::ExtractCurrentControl() {
  Node* result = current_control_;
  current_control_ = nullptr;
  return result;
}

Node* GraphAssembler::ExtractCurrentEffect() {
  Node* result = current_effect_;
  current_effect_ = nullptr;
  return result;
}

void GraphAssembler::Reset(Node* effect, Node* control) {
  current_effect_ = effect;
  current_control_ = control;
}

Operator const* GraphAssembler::ToNumberOperator() {
  if (!to_number_operator_.is_set()) {
    Callable callable =
        Builtins::CallableFor(jsgraph()->isolate(), Builtins::kToNumber);
    CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        jsgraph()->isolate(), graph()->zone(), callable.descriptor(), 0, flags,
        Operator::kEliminatable);
    to_number_operator_.set(common()->Call(call_descriptor));
  }
  return to_number_operator_.get();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
