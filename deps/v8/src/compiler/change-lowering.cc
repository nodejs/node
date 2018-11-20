// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/change-lowering.h"

#include "src/code-factory.h"
#include "src/compiler/diamond.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

ChangeLowering::~ChangeLowering() {}


Reduction ChangeLowering::Reduce(Node* node) {
  Node* control = graph()->start();
  switch (node->opcode()) {
    case IrOpcode::kChangeBitToBool:
      return ChangeBitToBool(node->InputAt(0), control);
    case IrOpcode::kChangeBoolToBit:
      return ChangeBoolToBit(node->InputAt(0));
    case IrOpcode::kChangeFloat64ToTagged:
      return ChangeFloat64ToTagged(node->InputAt(0), control);
    case IrOpcode::kChangeInt32ToTagged:
      return ChangeInt32ToTagged(node->InputAt(0), control);
    case IrOpcode::kChangeTaggedToFloat64:
      return ChangeTaggedToFloat64(node->InputAt(0), control);
    case IrOpcode::kChangeTaggedToInt32:
      return ChangeTaggedToUI32(node->InputAt(0), control, kSigned);
    case IrOpcode::kChangeTaggedToUint32:
      return ChangeTaggedToUI32(node->InputAt(0), control, kUnsigned);
    case IrOpcode::kChangeUint32ToTagged:
      return ChangeUint32ToTagged(node->InputAt(0), control);
    default:
      return NoChange();
  }
  UNREACHABLE();
  return NoChange();
}


Node* ChangeLowering::HeapNumberValueIndexConstant() {
  STATIC_ASSERT(HeapNumber::kValueOffset % kPointerSize == 0);
  const int heap_number_value_offset =
      ((HeapNumber::kValueOffset / kPointerSize) * (machine()->Is64() ? 8 : 4));
  return jsgraph()->IntPtrConstant(heap_number_value_offset - kHeapObjectTag);
}


Node* ChangeLowering::SmiMaxValueConstant() {
  const int smi_value_size = machine()->Is32() ? SmiTagging<4>::SmiValueSize()
                                               : SmiTagging<8>::SmiValueSize();
  return jsgraph()->Int32Constant(
      -(static_cast<int>(0xffffffffu << (smi_value_size - 1)) + 1));
}


Node* ChangeLowering::SmiShiftBitsConstant() {
  const int smi_shift_size = machine()->Is32() ? SmiTagging<4>::SmiShiftSize()
                                               : SmiTagging<8>::SmiShiftSize();
  return jsgraph()->IntPtrConstant(smi_shift_size + kSmiTagSize);
}


Node* ChangeLowering::AllocateHeapNumberWithValue(Node* value, Node* control) {
  // The AllocateHeapNumberStub does not use the context, so we can safely pass
  // in Smi zero here.
  Callable callable = CodeFactory::AllocateHeapNumber(isolate());
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), jsgraph()->zone(), callable.descriptor(), 0,
      CallDescriptor::kNoFlags);
  Node* target = jsgraph()->HeapConstant(callable.code());
  Node* context = jsgraph()->NoContextConstant();
  Node* effect = graph()->NewNode(common()->ValueEffect(1), value);
  Node* heap_number = graph()->NewNode(common()->Call(descriptor), target,
                                       context, effect, control);
  Node* store = graph()->NewNode(
      machine()->Store(StoreRepresentation(kMachFloat64, kNoWriteBarrier)),
      heap_number, HeapNumberValueIndexConstant(), value, heap_number, control);
  return graph()->NewNode(common()->Finish(1), heap_number, store);
}


Node* ChangeLowering::ChangeInt32ToFloat64(Node* value) {
  return graph()->NewNode(machine()->ChangeInt32ToFloat64(), value);
}


Node* ChangeLowering::ChangeSmiToFloat64(Node* value) {
  return ChangeInt32ToFloat64(ChangeSmiToInt32(value));
}


Node* ChangeLowering::ChangeSmiToInt32(Node* value) {
  value = graph()->NewNode(machine()->WordSar(), value, SmiShiftBitsConstant());
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->TruncateInt64ToInt32(), value);
  }
  return value;
}


Node* ChangeLowering::ChangeUint32ToFloat64(Node* value) {
  return graph()->NewNode(machine()->ChangeUint32ToFloat64(), value);
}


Node* ChangeLowering::ChangeUint32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->ChangeUint32ToUint64(), value);
  }
  return graph()->NewNode(machine()->WordShl(), value, SmiShiftBitsConstant());
}


Node* ChangeLowering::LoadHeapNumberValue(Node* value, Node* control) {
  return graph()->NewNode(machine()->Load(kMachFloat64), value,
                          HeapNumberValueIndexConstant(), graph()->start(),
                          control);
}


Node* ChangeLowering::TestNotSmi(Node* value) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);
  return graph()->NewNode(machine()->WordAnd(), value,
                          jsgraph()->IntPtrConstant(kSmiTagMask));
}


Node* ChangeLowering::Uint32LessThanOrEqual(Node* lhs, Node* rhs) {
  return graph()->NewNode(machine()->Uint32LessThanOrEqual(), lhs, rhs);
}


Reduction ChangeLowering::ChangeBitToBool(Node* val, Node* control) {
  MachineType const type = static_cast<MachineType>(kTypeBool | kRepTagged);
  return Replace(graph()->NewNode(common()->Select(type), val,
                                  jsgraph()->TrueConstant(),
                                  jsgraph()->FalseConstant()));
}


Reduction ChangeLowering::ChangeBoolToBit(Node* val) {
  return Replace(
      graph()->NewNode(machine()->WordEqual(), val, jsgraph()->TrueConstant()));
}


Reduction ChangeLowering::ChangeFloat64ToTagged(Node* val, Node* control) {
  return Replace(AllocateHeapNumberWithValue(val, control));
}


Reduction ChangeLowering::ChangeInt32ToTagged(Node* value, Node* control) {
  if (machine()->Is64()) {
    return Replace(graph()->NewNode(
        machine()->Word64Shl(),
        graph()->NewNode(machine()->ChangeInt32ToInt64(), value),
        SmiShiftBitsConstant()));
  } else if (NodeProperties::GetBounds(value).upper->Is(Type::Signed31())) {
    return Replace(
        graph()->NewNode(machine()->WordShl(), value, SmiShiftBitsConstant()));
  }

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), value, value);
  Node* ovf = graph()->NewNode(common()->Projection(1), add);

  Diamond d(graph(), common(), ovf, BranchHint::kFalse);
  d.Chain(control);
  return Replace(
      d.Phi(kMachAnyTagged,
            AllocateHeapNumberWithValue(ChangeInt32ToFloat64(value), d.if_true),
            graph()->NewNode(common()->Projection(0), add)));
}


Reduction ChangeLowering::ChangeTaggedToUI32(Node* value, Node* control,
                                             Signedness signedness) {
  const MachineType type = (signedness == kSigned) ? kMachInt32 : kMachUint32;
  const Operator* op = (signedness == kSigned)
                           ? machine()->ChangeFloat64ToInt32()
                           : machine()->ChangeFloat64ToUint32();
  Diamond d(graph(), common(), TestNotSmi(value), BranchHint::kFalse);
  d.Chain(control);
  return Replace(
      d.Phi(type, graph()->NewNode(op, LoadHeapNumberValue(value, d.if_true)),
            ChangeSmiToInt32(value)));
}


namespace {

bool CanCover(Node* value, IrOpcode::Value opcode) {
  if (value->opcode() != opcode) return false;
  bool first = true;
  for (Edge const edge : value->use_edges()) {
    if (NodeProperties::IsEffectEdge(edge)) continue;
    DCHECK(NodeProperties::IsValueEdge(edge));
    if (!first) return false;
    first = false;
  }
  return true;
}

}  // namespace


Reduction ChangeLowering::ChangeTaggedToFloat64(Node* value, Node* control) {
  if (CanCover(value, IrOpcode::kJSToNumber)) {
    // ChangeTaggedToFloat64(JSToNumber(x)) =>
    //   if IsSmi(x) then ChangeSmiToFloat64(x)
    //   else let y = JSToNumber(x) in
    //     if IsSmi(y) then ChangeSmiToFloat64(y)
    //     else LoadHeapNumberValue(y)
    Node* const object = NodeProperties::GetValueInput(value, 0);
    Node* const context = NodeProperties::GetContextInput(value);
    Node* const effect = NodeProperties::GetEffectInput(value);
    Node* const control = NodeProperties::GetControlInput(value);

    Diamond d1(graph(), common(), TestNotSmi(object), BranchHint::kFalse);
    d1.Chain(control);

    Node* number =
        OperatorProperties::HasFrameStateInput(value->op())
            ? graph()->NewNode(value->op(), object, context,
                               NodeProperties::GetFrameStateInput(value),
                               effect, d1.if_true)
            : graph()->NewNode(value->op(), object, context, effect,
                               d1.if_true);
    Diamond d2(graph(), common(), TestNotSmi(number));
    d2.Nest(d1, true);
    Node* phi2 = d2.Phi(kMachFloat64, LoadHeapNumberValue(number, d2.if_true),
                        ChangeSmiToFloat64(number));

    Node* phi1 = d1.Phi(kMachFloat64, phi2, ChangeSmiToFloat64(object));
    Node* ephi1 = d1.EffectPhi(number, effect);

    for (Edge edge : value->use_edges()) {
      if (NodeProperties::IsEffectEdge(edge)) {
        edge.UpdateTo(ephi1);
      }
    }
    return Replace(phi1);
  }

  Diamond d(graph(), common(), TestNotSmi(value), BranchHint::kFalse);
  d.Chain(control);
  Node* load = LoadHeapNumberValue(value, d.if_true);
  Node* number = ChangeSmiToFloat64(value);
  return Replace(d.Phi(kMachFloat64, load, number));
}


Reduction ChangeLowering::ChangeUint32ToTagged(Node* value, Node* control) {
  Diamond d(graph(), common(),
            Uint32LessThanOrEqual(value, SmiMaxValueConstant()),
            BranchHint::kTrue);
  d.Chain(control);
  return Replace(d.Phi(
      kMachAnyTagged, ChangeUint32ToSmi(value),
      AllocateHeapNumberWithValue(ChangeUint32ToFloat64(value), d.if_false)));
}


Isolate* ChangeLowering::isolate() const { return jsgraph()->isolate(); }


Graph* ChangeLowering::graph() const { return jsgraph()->graph(); }


CommonOperatorBuilder* ChangeLowering::common() const {
  return jsgraph()->common();
}


MachineOperatorBuilder* ChangeLowering::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
