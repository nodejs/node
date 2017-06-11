// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-assembler.h"

#include "src/code-factory.h"
#include "src/compiler/linkage.h"
#include "src/objects-inl.h"

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
  if (machine()->Float64RoundDown().IsSupported()) {
    return graph()->NewNode(machine()->Float64RoundDown().op(), value);
  }
  return nullptr;
}

Node* GraphAssembler::Projection(int index, Node* value) {
  return graph()->NewNode(common()->Projection(index), value, current_control_);
}

Node* GraphAssembler::Allocate(PretenureFlag pretenure, Node* size) {
  return current_effect_ =
             graph()->NewNode(simplified()->Allocate(Type::Any(), NOT_TENURED),
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

Node* GraphAssembler::DeoptimizeIf(DeoptimizeReason reason, Node* condition,
                                   Node* frame_state) {
  return current_control_ = current_effect_ = graph()->NewNode(
             common()->DeoptimizeIf(DeoptimizeKind::kEager, reason), condition,
             frame_state, current_effect_, current_control_);
}

Node* GraphAssembler::DeoptimizeUnless(DeoptimizeKind kind,
                                       DeoptimizeReason reason, Node* condition,
                                       Node* frame_state) {
  return current_control_ = current_effect_ = graph()->NewNode(
             common()->DeoptimizeUnless(kind, reason), condition, frame_state,
             current_effect_, current_control_);
}

Node* GraphAssembler::DeoptimizeUnless(DeoptimizeReason reason, Node* condition,
                                       Node* frame_state) {
  return DeoptimizeUnless(DeoptimizeKind::kEager, reason, condition,
                          frame_state);
}

void GraphAssembler::Branch(Node* condition,
                            GraphAssemblerStaticLabel<1>* if_true,
                            GraphAssemblerStaticLabel<1>* if_false) {
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
    Callable callable = CodeFactory::ToNumber(jsgraph()->isolate());
    CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        jsgraph()->isolate(), graph()->zone(), callable.descriptor(), 0, flags,
        Operator::kEliminatable);
    to_number_operator_.set(common()->Call(desc));
  }
  return to_number_operator_.get();
}

Node* GraphAssemblerLabel::PhiAt(size_t index) {
  DCHECK(IsBound());
  return GetBindingsPtrFor(index)[0];
}

GraphAssemblerLabel::GraphAssemblerLabel(GraphAssemblerLabelType is_deferred,
                                         size_t merge_count, size_t var_count,
                                         MachineRepresentation* representations,
                                         Zone* zone)
    : is_deferred_(is_deferred == GraphAssemblerLabelType::kDeferred),
      max_merge_count_(merge_count),
      var_count_(var_count) {
  effects_ = zone->NewArray<Node*>(MaxMergeCount() + 1);
  for (size_t i = 0; i < MaxMergeCount() + 1; i++) {
    effects_[i] = nullptr;
  }

  controls_ = zone->NewArray<Node*>(MaxMergeCount());
  for (size_t i = 0; i < MaxMergeCount(); i++) {
    controls_[i] = nullptr;
  }

  size_t num_bindings = (MaxMergeCount() + 1) * PhiCount() + 1;
  bindings_ = zone->NewArray<Node*>(num_bindings);
  for (size_t i = 0; i < num_bindings; i++) {
    bindings_[i] = nullptr;
  }

  representations_ = zone->NewArray<MachineRepresentation>(PhiCount() + 1);
  for (size_t i = 0; i < PhiCount(); i++) {
    representations_[i] = representations[i];
  }
}

GraphAssemblerLabel::~GraphAssemblerLabel() {
  DCHECK(IsBound() || MergedCount() == 0);
}

Node** GraphAssemblerLabel::GetBindingsPtrFor(size_t phi_index) {
  DCHECK_LT(phi_index, PhiCount());
  return &bindings_[phi_index * (MaxMergeCount() + 1)];
}

void GraphAssemblerLabel::SetBinding(size_t phi_index, size_t merge_index,
                                     Node* binding) {
  DCHECK_LT(phi_index, PhiCount());
  DCHECK_LT(merge_index, MaxMergeCount());
  bindings_[phi_index * (MaxMergeCount() + 1) + merge_index] = binding;
}

MachineRepresentation GraphAssemblerLabel::GetRepresentationFor(
    size_t phi_index) {
  DCHECK_LT(phi_index, PhiCount());
  return representations_[phi_index];
}

Node** GraphAssemblerLabel::GetControlsPtr() { return controls_; }

Node** GraphAssemblerLabel::GetEffectsPtr() { return effects_; }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
