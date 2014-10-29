// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/change-lowering.h"
#include "src/compiler/machine-operator.h"

#include "src/compiler/js-graph.h"

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
  return jsgraph()->Int32Constant(heap_number_value_offset - kHeapObjectTag);
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
  return jsgraph()->Int32Constant(smi_shift_size + kSmiTagSize);
}


Node* ChangeLowering::AllocateHeapNumberWithValue(Node* value, Node* control) {
  // The AllocateHeapNumber() runtime function does not use the context, so we
  // can safely pass in Smi zero here.
  Node* context = jsgraph()->ZeroConstant();
  Node* effect = graph()->NewNode(common()->ValueEffect(1), value);
  const Runtime::Function* function =
      Runtime::FunctionForId(Runtime::kAllocateHeapNumber);
  DCHECK_EQ(0, function->nargs);
  CallDescriptor* desc = linkage()->GetRuntimeCallDescriptor(
      function->function_id, 0, Operator::kNoProperties);
  Node* heap_number = graph()->NewNode(
      common()->Call(desc), jsgraph()->CEntryStubConstant(),
      jsgraph()->ExternalConstant(ExternalReference(function, isolate())),
      jsgraph()->Int32Constant(function->nargs), context, effect, control);
  Node* store = graph()->NewNode(
      machine()->Store(StoreRepresentation(kMachFloat64, kNoWriteBarrier)),
      heap_number, HeapNumberValueIndexConstant(), value, heap_number, control);
  return graph()->NewNode(common()->Finish(1), heap_number, store);
}


Node* ChangeLowering::ChangeSmiToInt32(Node* value) {
  value = graph()->NewNode(machine()->WordSar(), value, SmiShiftBitsConstant());
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->TruncateInt64ToInt32(), value);
  }
  return value;
}


Node* ChangeLowering::LoadHeapNumberValue(Node* value, Node* control) {
  return graph()->NewNode(machine()->Load(kMachFloat64), value,
                          HeapNumberValueIndexConstant(),
                          graph()->NewNode(common()->ControlEffect(), control));
}


Reduction ChangeLowering::ChangeBitToBool(Node* val, Node* control) {
  Node* branch = graph()->NewNode(common()->Branch(), val, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* true_value = jsgraph()->TrueConstant();

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* false_value = jsgraph()->FalseConstant();

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(
      common()->Phi(static_cast<MachineType>(kTypeBool | kRepTagged), 2),
      true_value, false_value, merge);

  return Replace(phi);
}


Reduction ChangeLowering::ChangeBoolToBit(Node* val) {
  return Replace(
      graph()->NewNode(machine()->WordEqual(), val, jsgraph()->TrueConstant()));
}


Reduction ChangeLowering::ChangeFloat64ToTagged(Node* val, Node* control) {
  return Replace(AllocateHeapNumberWithValue(val, control));
}


Reduction ChangeLowering::ChangeInt32ToTagged(Node* val, Node* control) {
  if (machine()->Is64()) {
    return Replace(
        graph()->NewNode(machine()->Word64Shl(),
                         graph()->NewNode(machine()->ChangeInt32ToInt64(), val),
                         SmiShiftBitsConstant()));
  }

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), val, val);
  Node* ovf = graph()->NewNode(common()->Projection(1), add);

  Node* branch = graph()->NewNode(common()->Branch(), ovf, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* heap_number = AllocateHeapNumberWithValue(
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), val), if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* smi = graph()->NewNode(common()->Projection(0), add);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), heap_number,
                               smi, merge);

  return Replace(phi);
}


Reduction ChangeLowering::ChangeTaggedToUI32(Node* val, Node* control,
                                             Signedness signedness) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);

  Node* tag = graph()->NewNode(machine()->WordAnd(), val,
                               jsgraph()->Int32Constant(kSmiTagMask));
  Node* branch = graph()->NewNode(common()->Branch(), tag, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  const Operator* op = (signedness == kSigned)
                           ? machine()->ChangeFloat64ToInt32()
                           : machine()->ChangeFloat64ToUint32();
  Node* change = graph()->NewNode(op, LoadHeapNumberValue(val, if_true));

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* number = ChangeSmiToInt32(val);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(
      common()->Phi((signedness == kSigned) ? kMachInt32 : kMachUint32, 2),
      change, number, merge);

  return Replace(phi);
}


Reduction ChangeLowering::ChangeTaggedToFloat64(Node* val, Node* control) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);

  Node* tag = graph()->NewNode(machine()->WordAnd(), val,
                               jsgraph()->Int32Constant(kSmiTagMask));
  Node* branch = graph()->NewNode(common()->Branch(), tag, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* load = LoadHeapNumberValue(val, if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* number = graph()->NewNode(machine()->ChangeInt32ToFloat64(),
                                  ChangeSmiToInt32(val));

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi =
      graph()->NewNode(common()->Phi(kMachFloat64, 2), load, number, merge);

  return Replace(phi);
}


Reduction ChangeLowering::ChangeUint32ToTagged(Node* val, Node* control) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);

  Node* cmp = graph()->NewNode(machine()->Uint32LessThanOrEqual(), val,
                               SmiMaxValueConstant());
  Node* branch = graph()->NewNode(common()->Branch(), cmp, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* smi = graph()->NewNode(
      machine()->WordShl(),
      machine()->Is64()
          ? graph()->NewNode(machine()->ChangeUint32ToUint64(), val)
          : val,
      SmiShiftBitsConstant());

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* heap_number = AllocateHeapNumberWithValue(
      graph()->NewNode(machine()->ChangeUint32ToFloat64(), val), if_false);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), smi,
                               heap_number, merge);

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
