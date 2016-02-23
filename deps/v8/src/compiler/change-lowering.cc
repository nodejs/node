// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/change-lowering.h"

#include "src/code-factory.h"
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
  return jsgraph()->IntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag);
}


Node* ChangeLowering::SmiMaxValueConstant() {
  return jsgraph()->Int32Constant(Smi::kMaxValue);
}


Node* ChangeLowering::SmiShiftBitsConstant() {
  return jsgraph()->IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}


Node* ChangeLowering::AllocateHeapNumberWithValue(Node* value, Node* control) {
  // The AllocateHeapNumberStub does not use the context, so we can safely pass
  // in Smi zero here.
  Callable callable = CodeFactory::AllocateHeapNumber(isolate());
  Node* target = jsgraph()->HeapConstant(callable.code());
  Node* context = jsgraph()->NoContextConstant();
  Node* effect = graph()->NewNode(common()->BeginRegion(), graph()->start());
  if (!allocate_heap_number_operator_.is_set()) {
    CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
        isolate(), jsgraph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNoFlags, Operator::kNoThrow);
    allocate_heap_number_operator_.set(common()->Call(descriptor));
  }
  Node* heap_number = graph()->NewNode(allocate_heap_number_operator_.get(),
                                       target, context, effect, control);
  Node* store = graph()->NewNode(
      machine()->Store(StoreRepresentation(kMachFloat64, kNoWriteBarrier)),
      heap_number, HeapNumberValueIndexConstant(), value, heap_number, control);
  return graph()->NewNode(common()->FinishRegion(), heap_number, store);
}


Node* ChangeLowering::ChangeInt32ToFloat64(Node* value) {
  return graph()->NewNode(machine()->ChangeInt32ToFloat64(), value);
}


Node* ChangeLowering::ChangeInt32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->ChangeInt32ToInt64(), value);
  }
  return graph()->NewNode(machine()->WordShl(), value, SmiShiftBitsConstant());
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


Reduction ChangeLowering::ChangeBitToBool(Node* value, Node* control) {
  return Replace(graph()->NewNode(common()->Select(kMachAnyTagged), value,
                                  jsgraph()->TrueConstant(),
                                  jsgraph()->FalseConstant()));
}


Reduction ChangeLowering::ChangeBoolToBit(Node* value) {
  return Replace(graph()->NewNode(machine()->WordEqual(), value,
                                  jsgraph()->TrueConstant()));
}


Reduction ChangeLowering::ChangeFloat64ToTagged(Node* value, Node* control) {
  Type* const value_type = NodeProperties::GetType(value);
  Node* const value32 = graph()->NewNode(
      machine()->TruncateFloat64ToInt32(TruncationMode::kRoundToZero), value);
  // TODO(bmeurer): This fast case must be disabled until we kill the asm.js
  // support in the generic JavaScript pipeline, because LoadBuffer is lying
  // about its result.
  // if (value_type->Is(Type::Signed32())) {
  //   return ChangeInt32ToTagged(value32, control);
  // }
  Node* check_same = graph()->NewNode(
      machine()->Float64Equal(), value,
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), value32));
  Node* branch_same = graph()->NewNode(common()->Branch(), check_same, control);

  Node* if_smi = graph()->NewNode(common()->IfTrue(), branch_same);
  Node* vsmi;
  Node* if_box = graph()->NewNode(common()->IfFalse(), branch_same);
  Node* vbox;

  // We only need to check for -0 if the {value} can potentially contain -0.
  if (value_type->Maybe(Type::MinusZero())) {
    Node* check_zero = graph()->NewNode(machine()->Word32Equal(), value32,
                                        jsgraph()->Int32Constant(0));
    Node* branch_zero = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                         check_zero, if_smi);

    Node* if_zero = graph()->NewNode(common()->IfTrue(), branch_zero);
    Node* if_notzero = graph()->NewNode(common()->IfFalse(), branch_zero);

    // In case of 0, we need to check the high bits for the IEEE -0 pattern.
    Node* check_negative = graph()->NewNode(
        machine()->Int32LessThan(),
        graph()->NewNode(machine()->Float64ExtractHighWord32(), value),
        jsgraph()->Int32Constant(0));
    Node* branch_negative = graph()->NewNode(
        common()->Branch(BranchHint::kFalse), check_negative, if_zero);

    Node* if_negative = graph()->NewNode(common()->IfTrue(), branch_negative);
    Node* if_notnegative =
        graph()->NewNode(common()->IfFalse(), branch_negative);

    // We need to create a box for negative 0.
    if_smi = graph()->NewNode(common()->Merge(2), if_notzero, if_notnegative);
    if_box = graph()->NewNode(common()->Merge(2), if_box, if_negative);
  }

  // On 64-bit machines we can just wrap the 32-bit integer in a smi, for 32-bit
  // machines we need to deal with potential overflow and fallback to boxing.
  if (machine()->Is64() || value_type->Is(Type::SignedSmall())) {
    vsmi = ChangeInt32ToSmi(value32);
  } else {
    Node* smi_tag =
        graph()->NewNode(machine()->Int32AddWithOverflow(), value32, value32);

    Node* check_ovf = graph()->NewNode(common()->Projection(1), smi_tag);
    Node* branch_ovf = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                        check_ovf, if_smi);

    Node* if_ovf = graph()->NewNode(common()->IfTrue(), branch_ovf);
    if_box = graph()->NewNode(common()->Merge(2), if_ovf, if_box);

    if_smi = graph()->NewNode(common()->IfFalse(), branch_ovf);
    vsmi = graph()->NewNode(common()->Projection(0), smi_tag);
  }

  // Allocate the box for the {value}.
  vbox = AllocateHeapNumberWithValue(value, if_box);

  control = graph()->NewNode(common()->Merge(2), if_smi, if_box);
  value =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), vsmi, vbox, control);
  return Replace(value);
}


Reduction ChangeLowering::ChangeInt32ToTagged(Node* value, Node* control) {
  if (machine()->Is64() ||
      NodeProperties::GetType(value)->Is(Type::SignedSmall())) {
    return Replace(ChangeInt32ToSmi(value));
  }

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), value, value);

  Node* ovf = graph()->NewNode(common()->Projection(1), add);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), ovf, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue =
      AllocateHeapNumberWithValue(ChangeInt32ToFloat64(value), if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = graph()->NewNode(common()->Projection(0), add);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), vtrue, vfalse, merge);

  return Replace(phi);
}


Reduction ChangeLowering::ChangeTaggedToUI32(Node* value, Node* control,
                                             Signedness signedness) {
  if (NodeProperties::GetType(value)->Is(Type::TaggedSigned())) {
    return Replace(ChangeSmiToInt32(value));
  }

  const MachineType type = (signedness == kSigned) ? kMachInt32 : kMachUint32;
  const Operator* op = (signedness == kSigned)
                           ? machine()->ChangeFloat64ToInt32()
                           : machine()->ChangeFloat64ToUint32();

  if (NodeProperties::GetType(value)->Is(Type::TaggedPointer())) {
    return Replace(graph()->NewNode(op, LoadHeapNumberValue(value, control)));
  }

  Node* check = TestNotSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = graph()->NewNode(op, LoadHeapNumberValue(value, if_true));

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = ChangeSmiToInt32(value);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(type, 2), vtrue, vfalse, merge);

  return Replace(phi);
}


namespace {

bool CanCover(Node* value, IrOpcode::Value opcode) {
  if (value->opcode() != opcode) return false;
  bool first = true;
  for (Edge const edge : value->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) continue;
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
    Node* const frame_state = NodeProperties::GetFrameStateInput(value, 0);
    Node* const effect = NodeProperties::GetEffectInput(value);
    Node* const control = NodeProperties::GetControlInput(value);

    const Operator* merge_op = common()->Merge(2);
    const Operator* ephi_op = common()->EffectPhi(2);
    const Operator* phi_op = common()->Phi(kMachFloat64, 2);

    Node* check1 = TestNotSmi(object);
    Node* branch1 =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check1, control);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = graph()->NewNode(value->op(), object, context, frame_state,
                                    effect, if_true1);
    Node* etrue1 = vtrue1;

    Node* check2 = TestNotSmi(vtrue1);
    Node* branch2 = graph()->NewNode(common()->Branch(), check2, if_true1);

    Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
    Node* vtrue2 = LoadHeapNumberValue(vtrue1, if_true2);

    Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
    Node* vfalse2 = ChangeSmiToFloat64(vtrue1);

    if_true1 = graph()->NewNode(merge_op, if_true2, if_false2);
    vtrue1 = graph()->NewNode(phi_op, vtrue2, vfalse2, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1 = ChangeSmiToFloat64(object);
    Node* efalse1 = effect;

    Node* merge1 = graph()->NewNode(merge_op, if_true1, if_false1);
    Node* ephi1 = graph()->NewNode(ephi_op, etrue1, efalse1, merge1);
    Node* phi1 = graph()->NewNode(phi_op, vtrue1, vfalse1, merge1);

    // Wire the new diamond into the graph, {JSToNumber} can still throw.
    NodeProperties::ReplaceUses(value, phi1, ephi1, etrue1, etrue1);

    // TODO(mstarzinger): This iteration cuts out the IfSuccess projection from
    // the node and places it inside the diamond. Come up with a helper method!
    for (Node* use : etrue1->uses()) {
      if (use->opcode() == IrOpcode::kIfSuccess) {
        use->ReplaceUses(merge1);
        NodeProperties::ReplaceControlInput(branch2, use);
      }
    }

    return Replace(phi1);
  }

  Node* check = TestNotSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = LoadHeapNumberValue(value, if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = ChangeSmiToFloat64(value);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi =
      graph()->NewNode(common()->Phi(kMachFloat64, 2), vtrue, vfalse, merge);

  return Replace(phi);
}


Reduction ChangeLowering::ChangeUint32ToTagged(Node* value, Node* control) {
  if (NodeProperties::GetType(value)->Is(Type::UnsignedSmall())) {
    return Replace(ChangeUint32ToSmi(value));
  }

  Node* check = graph()->NewNode(machine()->Uint32LessThanOrEqual(), value,
                                 SmiMaxValueConstant());
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = ChangeUint32ToSmi(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse =
      AllocateHeapNumberWithValue(ChangeUint32ToFloat64(value), if_false);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), vtrue, vfalse, merge);

  return Replace(phi);
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
