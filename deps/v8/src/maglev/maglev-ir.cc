// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-ir.h"

#include <limits>

#include "src/base/bounds.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/heap-refs.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

const char* OpcodeToString(Opcode opcode) {
#define DEF_NAME(Name) #Name,
  static constexpr const char* const names[] = {NODE_BASE_LIST(DEF_NAME)};
#undef DEF_NAME
  return names[static_cast<int>(opcode)];
}

BasicBlock* Phi::predecessor_at(int i) {
  return merge_state_->predecessor_at(i);
}

namespace {

// Prevent people from accidentally using kScratchRegister here and having their
// code break in arm64.
struct Do_not_use_kScratchRegister_in_arch_independent_code {
} kScratchRegister;
struct Do_not_use_kScratchDoubleRegister_in_arch_independent_code {
} kScratchDoubleRegister;
static_assert(!std::is_same_v<decltype(kScratchRegister), Register>);
static_assert(
    !std::is_same_v<decltype(kScratchDoubleRegister), DoubleRegister>);

}  // namespace

#ifdef DEBUG
namespace {

template <size_t InputCount, typename Base, typename Derived>
int StaticInputCount(FixedInputNodeTMixin<InputCount, Base, Derived>*) {
  return InputCount;
}

int StaticInputCount(NodeBase*) { UNREACHABLE(); }

}  // namespace

void NodeBase::CheckCanOverwriteWith(Opcode new_opcode,
                                     OpProperties new_properties) {
  DCHECK_IMPLIES(new_properties.can_eager_deopt(),
                 properties().can_eager_deopt());
  DCHECK_IMPLIES(new_properties.can_lazy_deopt(),
                 properties().can_lazy_deopt());
  DCHECK_IMPLIES(new_properties.needs_register_snapshot(),
                 properties().needs_register_snapshot());

  int old_input_count = input_count();
  size_t old_sizeof = -1;
  switch (opcode()) {
#define CASE(op)             \
  case Opcode::k##op:        \
    old_sizeof = sizeof(op); \
    break;
    NODE_BASE_LIST(CASE);
#undef CASE
  }

  switch (new_opcode) {
#define CASE(op)                                                          \
  case Opcode::k##op: {                                                   \
    DCHECK_EQ(old_input_count, StaticInputCount(static_cast<op*>(this))); \
    DCHECK_EQ(sizeof(op), old_sizeof);                                    \
    break;                                                                \
  }
    NODE_BASE_LIST(CASE)
#undef CASE
  }
}

#endif  // DEBUG

namespace {

// ---
// Print
// ---

void PrintInputs(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                 const NodeBase* node) {
  if (!node->has_inputs()) return;

  os << " [";
  for (int i = 0; i < node->input_count(); i++) {
    if (i != 0) os << ", ";
    graph_labeller->PrintInput(os, node->input(i));
  }
  os << "]";
}

void PrintResult(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                 const NodeBase* node) {}

void PrintResult(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                 const ValueNode* node) {
  os << " → " << node->result().operand();
  if (node->has_valid_live_range()) {
    os << ", live range: [" << node->live_range().start << "-"
       << node->live_range().end << "]";
  }
}

void PrintTargets(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                  const NodeBase* node) {}

void PrintTargets(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                  const UnconditionalControlNode* node) {
  os << " b" << graph_labeller->BlockId(node->target());
}

void PrintTargets(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                  const BranchControlNode* node) {
  os << " b" << graph_labeller->BlockId(node->if_true()) << " b"
     << graph_labeller->BlockId(node->if_false());
}

void PrintTargets(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                  const Switch* node) {
  for (int i = 0; i < node->size(); i++) {
    const BasicBlockRef& target = node->Cast<Switch>()->targets()[i];
    os << " b" << graph_labeller->BlockId(target.block_ptr());
  }
  if (node->Cast<Switch>()->has_fallthrough()) {
    BasicBlock* fallthrough_target = node->Cast<Switch>()->fallthrough();
    os << " b" << graph_labeller->BlockId(fallthrough_target);
  }
}

class MaybeUnparkForPrint {
 public:
  MaybeUnparkForPrint() {
    LocalHeap* local_heap = LocalHeap::Current();
    if (!local_heap) {
      local_heap = Isolate::Current()->main_thread_local_heap();
    }
    DCHECK_NOT_NULL(local_heap);
    if (local_heap->IsParked()) {
      scope_.emplace(local_heap);
    }
  }

 private:
  base::Optional<UnparkedScope> scope_;
};

template <typename NodeT>
void PrintImpl(std::ostream& os, MaglevGraphLabeller* graph_labeller,
               const NodeT* node, bool skip_targets) {
  MaybeUnparkForPrint unpark;
  os << node->opcode();
  node->PrintParams(os, graph_labeller);
  PrintInputs(os, graph_labeller, node);
  PrintResult(os, graph_labeller, node);
  if (!skip_targets) {
    PrintTargets(os, graph_labeller, node);
  }
}

size_t GetInputLocationsArraySize(const DeoptFrame& top_frame) {
  size_t size = 0;
  const DeoptFrame* frame = &top_frame;
  do {
    switch (frame->type()) {
      case DeoptFrame::FrameType::kInterpretedFrame:
        size += frame->as_interpreted().frame_state()->size(
            frame->as_interpreted().unit());
        break;
      case DeoptFrame::FrameType::kInlinedArgumentsFrame:
        size += frame->as_inlined_arguments().arguments().size();
        break;
      case DeoptFrame::FrameType::kBuiltinContinuationFrame:
        size += frame->as_builtin_continuation().parameters().size() + 1;
        break;
    }
    frame = frame->parent();
  } while (frame != nullptr);
  return size;
}

bool RootToBoolean(RootIndex index) {
  switch (index) {
    case RootIndex::kFalseValue:
    case RootIndex::kNullValue:
    case RootIndex::kUndefinedValue:
    case RootIndex::kNanValue:
    case RootIndex::kHoleNanValue:
    case RootIndex::kMinusZeroValue:
    case RootIndex::kempty_string:
#ifdef V8_ENABLE_WEBASSEMBLY
    case RootIndex::kWasmNull:
#endif
      return false;
    default:
      return true;
  }
}

#ifdef DEBUG
// For all RO roots, check that RootToBoolean returns the same value as
// BooleanValue on that root.
bool CheckToBooleanOnAllRoots(LocalIsolate* local_isolate) {
  ReadOnlyRoots roots(local_isolate);
  // Use the READ_ONLY_ROOT_LIST macro list rather than a for loop to get nicer
  // error messages if there is a failure.
#define DO_CHECK(type, name, CamelName)                                   \
  /* Ignore 'undefined' roots that are not the undefined value itself. */ \
  if (roots.name() != roots.undefined_value() ||                          \
      RootIndex::k##CamelName == RootIndex::kUndefinedValue) {            \
    DCHECK_EQ(roots.name().BooleanValue(local_isolate),                   \
              RootToBoolean(RootIndex::k##CamelName));                    \
  }
  READ_ONLY_ROOT_LIST(DO_CHECK)
#undef DO_CHECK
  return true;
}
#endif

}  // namespace

bool RootConstant::ToBoolean(LocalIsolate* local_isolate) const {
#ifdef DEBUG
  // (Ab)use static locals to call CheckToBooleanOnAllRoots once, on first
  // call to this function.
  static bool check_once = CheckToBooleanOnAllRoots(local_isolate);
  DCHECK(check_once);
#endif
  // ToBoolean is only supported for RO roots.
  DCHECK(RootsTable::IsReadOnly(index_));
  return RootToBoolean(index_);
}

bool FromConstantToBool(LocalIsolate* local_isolate, ValueNode* node) {
  DCHECK(IsConstantNode(node->opcode()));
  switch (node->opcode()) {
#define CASE(Name)                                       \
  case Opcode::k##Name: {                                \
    return node->Cast<Name>()->ToBoolean(local_isolate); \
  }
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

bool FromConstantToBool(MaglevAssembler* masm, ValueNode* node) {
  // TODO(leszeks): Getting the main thread local isolate is not what we
  // actually want here, but it's all we have, and it happens to work because
  // really all we're using it for is ReadOnlyRoots. We should change ToBoolean
  // to be able to pass ReadOnlyRoots in directly.
  return FromConstantToBool(masm->isolate()->AsLocalIsolate(), node);
}

DeoptInfo::DeoptInfo(Zone* zone, const DeoptFrame top_frame,
                     compiler::FeedbackSource feedback_to_update)
    : top_frame_(top_frame),
      feedback_to_update_(feedback_to_update),
      input_locations_(zone->NewArray<InputLocation>(
          GetInputLocationsArraySize(top_frame))) {
  // Initialise InputLocations so that they correctly don't have a next use id.
  for (size_t i = 0; i < GetInputLocationsArraySize(top_frame); ++i) {
    new (&input_locations_[i]) InputLocation();
  }
}

bool LazyDeoptInfo::IsResultRegister(interpreter::Register reg) const {
  if (V8_LIKELY(result_size() == 1)) {
    return reg == result_location_;
  }
  if (result_size() == 0) {
    return false;
  }
  DCHECK_EQ(result_size(), 2);
  return reg == result_location_ ||
         reg == interpreter::Register(result_location_.index() + 1);
}

void NodeBase::Print(std::ostream& os, MaglevGraphLabeller* graph_labeller,
                     bool skip_targets) const {
  switch (opcode()) {
#define V(Name)         \
  case Opcode::k##Name: \
    return PrintImpl(os, graph_labeller, this->Cast<Name>(), skip_targets);
    NODE_BASE_LIST(V)
#undef V
  }
  UNREACHABLE();
}

void NodeBase::Print() const {
  MaglevGraphLabeller labeller;
  Print(std::cout, &labeller);
  std::cout << std::endl;
}

void ValueNode::SetHint(compiler::InstructionOperand hint) {
  if (!hint_.IsInvalid()) return;
  hint_ = hint;
  if (result_.operand().IsUnallocated()) {
    auto operand = compiler::UnallocatedOperand::cast(result_.operand());
    if (operand.HasSameAsInputPolicy()) {
      input(operand.input_index()).node()->SetHint(hint);
    }
  }
  if (this->Is<Phi>()) {
    for (Input& input : *this) {
      if (input.node()->has_id() && input.node()->id() < this->id()) {
        input.node()->SetHint(hint);
      }
    }
  }
}

void ValueNode::SetNoSpill() {
  DCHECK(!IsConstantNode(opcode()));
#ifdef DEBUG
  state_ = kSpill;
#endif  // DEBUG
  spill_ = compiler::InstructionOperand();
}

void ValueNode::SetConstantLocation() {
  DCHECK(IsConstantNode(opcode()));
#ifdef DEBUG
  state_ = kSpill;
#endif  // DEBUG
  spill_ = compiler::ConstantOperand(
      compiler::UnallocatedOperand::cast(result().operand())
          .virtual_register());
}

// ---
// Check input value representation
// ---

ValueRepresentation ToValueRepresentation(MachineType type) {
  switch (type.representation()) {
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
      return ValueRepresentation::kTagged;
    case MachineRepresentation::kFloat64:
      return ValueRepresentation::kFloat64;
    case MachineRepresentation::kWord64:
      return ValueRepresentation::kWord64;
    default:
      return ValueRepresentation::kInt32;
  }
}

void CheckValueInputIs(const NodeBase* node, int i,
                       ValueRepresentation expected,
                       MaglevGraphLabeller* graph_labeller) {
  ValueNode* input = node->input(i).node();
  DCHECK(!input->Is<Identity>());
  ValueRepresentation got = input->properties().value_representation();
  if (got != expected) {
    std::ostringstream str;
    str << "Type representation error: node ";
    if (graph_labeller) {
      str << "#" << graph_labeller->NodeId(node) << " : ";
    }
    str << node->opcode() << " (input @" << i << " = " << input->opcode()
        << ") type " << got << " is not " << expected;
    FATAL("%s", str.str().c_str());
  }
}

void CheckValueInputIs(const NodeBase* node, int i, Opcode expected,
                       MaglevGraphLabeller* graph_labeller) {
  ValueNode* input = node->input(i).node();
  Opcode got = input->opcode();
  if (got != expected) {
    std::ostringstream str;
    str << "Opcode error: node ";
    if (graph_labeller) {
      str << "#" << graph_labeller->NodeId(node) << " : ";
    }
    str << node->opcode() << " (input @" << i << " = " << input->opcode()
        << ") opcode " << got << " is not " << expected;
    FATAL("%s", str.str().c_str());
  }
}

void CheckValueInputIsWord32(const NodeBase* node, int i,
                             MaglevGraphLabeller* graph_labeller) {
  ValueNode* input = node->input(i).node();
  DCHECK(!input->Is<Identity>());
  ValueRepresentation got = input->properties().value_representation();
  if (got != ValueRepresentation::kInt32 &&
      got != ValueRepresentation::kUint32) {
    std::ostringstream str;
    str << "Type representation error: node ";
    if (graph_labeller) {
      str << "#" << graph_labeller->NodeId(node) << " : ";
    }
    str << node->opcode() << " (input @" << i << " = " << input->opcode()
        << ") type " << got << " is not Word32 (Int32 or Uint32)";
    FATAL("%s", str.str().c_str());
  }
}

void GeneratorStore::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void UnsafeSmiTag::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  DCHECK_EQ(input_count(), 1);
  CheckValueInputIsWord32(this, 0, graph_labeller);
}

void Phi::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  switch (value_representation()) {
#define CASE_REPR(repr)                                        \
  case ValueRepresentation::k##repr:                           \
    for (int i = 0; i < input_count(); i++) {                  \
      CheckValueInputIs(this, i, ValueRepresentation::k##repr, \
                        graph_labeller);                       \
    }                                                          \
    break;

    CASE_REPR(Tagged)
    CASE_REPR(Int32)
    CASE_REPR(Uint32)
    CASE_REPR(Float64)
#undef CASE_REPR
    case ValueRepresentation::kWord64:
      UNREACHABLE();
  }
}

void Call::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void Call::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void CallWithArrayLike::VerifyInputs(
    MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallWithArrayLike::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void CallWithSpread::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallWithSpread::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void CallSelf::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallSelf::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void CallKnownJSFunction::VerifyInputs(
    MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallKnownJSFunction::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void Construct::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void Construct::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void ConstructWithSpread::VerifyInputs(
    MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void ConstructWithSpread::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void CallBuiltin::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  int count = input_count();
  // Verify context.
  if (descriptor.HasContextParameter()) {
    CheckValueInputIs(this, count - 1, ValueRepresentation::kTagged,
                      graph_labeller);
    count--;
  }

// {all_input_count} includes the feedback slot and vector.
#ifdef DEBUG
  int all_input_count = count + (has_feedback() ? 2 : 0);
  if (descriptor.AllowVarArgs()) {
    DCHECK_GE(all_input_count, descriptor.GetParameterCount());
  } else {
    DCHECK_EQ(all_input_count, descriptor.GetParameterCount());
  }
#endif
  int i = 0;
  // Check the rest of inputs.
  for (; i < count; ++i) {
    MachineType type = i < descriptor.GetParameterCount()
                           ? descriptor.GetParameterType(i)
                           : MachineType::AnyTagged();
    CheckValueInputIs(this, i, ToValueRepresentation(type), graph_labeller);
  }
}

void CallBuiltin::MarkTaggedInputsAsDecompressing() {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  int count = input_count();
  // Set context.
  if (descriptor.HasContextParameter()) {
    input(count - 1).node()->SetTaggedResultNeedsDecompress();
    count--;
  }
  int i = 0;
  // Set the rest of the tagged inputs.
  for (; i < count; ++i) {
    MachineType type = i < descriptor.GetParameterCount()
                           ? descriptor.GetParameterType(i)
                           : MachineType::AnyTagged();
    if (type.IsTagged() && !type.IsTaggedSigned()) {
      input(i).node()->SetTaggedResultNeedsDecompress();
    }
  }
}

void CallRuntime::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallRuntime::MarkTaggedInputsAsDecompressing() {
  for (int i = 0; i < input_count(); i++) {
    input(i).node()->SetTaggedResultNeedsDecompress();
  }
}

void FoldedAllocation::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  Base::VerifyInputs(graph_labeller);
  CheckValueInputIs(this, 0, Opcode::kAllocateRaw, graph_labeller);
}

// ---
// Reify constants
// ---

Handle<Object> ValueNode::Reify(LocalIsolate* isolate) {
  switch (opcode()) {
#define V(Name)         \
  case Opcode::k##Name: \
    return this->Cast<Name>()->DoReify(isolate);
    CONSTANT_VALUE_NODE_LIST(V)
#undef V
    default:
      UNREACHABLE();
  }
}

Handle<Object> ExternalConstant::DoReify(LocalIsolate* isolate) {
  UNREACHABLE();
}

Handle<Object> SmiConstant::DoReify(LocalIsolate* isolate) {
  return handle(value_, isolate);
}

Handle<Object> Int32Constant::DoReify(LocalIsolate* isolate) {
  return isolate->factory()->NewNumber<AllocationType::kOld>(value());
}

Handle<Object> Float64Constant::DoReify(LocalIsolate* isolate) {
  return isolate->factory()->NewNumber<AllocationType::kOld>(
      value_.get_scalar());
}

Handle<Object> Constant::DoReify(LocalIsolate* isolate) {
  return object_.object();
}

Handle<Object> RootConstant::DoReify(LocalIsolate* isolate) {
  return isolate->root_handle(index());
}

// ---
// Load node to registers
// ---

namespace {
template <typename NodeT>
void LoadToRegisterHelper(NodeT* node, MaglevAssembler* masm, Register reg) {
  if constexpr (NodeT::kProperties.value_representation() !=
                ValueRepresentation::kFloat64) {
    return node->DoLoadToRegister(masm, reg);
  } else {
    UNREACHABLE();
  }
}
template <typename NodeT>
void LoadToRegisterHelper(NodeT* node, MaglevAssembler* masm,
                          DoubleRegister reg) {
  if constexpr (NodeT::kProperties.value_representation() ==
                ValueRepresentation::kFloat64) {
    return node->DoLoadToRegister(masm, reg);
  } else {
    UNREACHABLE();
  }
}
}  // namespace

void ValueNode::LoadToRegister(MaglevAssembler* masm, Register reg) {
  switch (opcode()) {
#define V(Name)         \
  case Opcode::k##Name: \
    return LoadToRegisterHelper(this->Cast<Name>(), masm, reg);
    VALUE_NODE_LIST(V)
#undef V
    default:
      UNREACHABLE();
  }
}
void ValueNode::LoadToRegister(MaglevAssembler* masm, DoubleRegister reg) {
  switch (opcode()) {
#define V(Name)         \
  case Opcode::k##Name: \
    return LoadToRegisterHelper(this->Cast<Name>(), masm, reg);
    VALUE_NODE_LIST(V)
#undef V
    default:
      UNREACHABLE();
  }
}

void ValueNode::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  DCHECK(is_spilled());
  DCHECK(!use_double_register());
  __ Move(reg,
          masm->GetStackSlot(compiler::AllocatedOperand::cast(spill_slot())));
}

void ValueNode::DoLoadToRegister(MaglevAssembler* masm, DoubleRegister reg) {
  DCHECK(is_spilled());
  DCHECK(use_double_register());
  __ Move(reg,
          masm->GetStackSlot(compiler::AllocatedOperand::cast(spill_slot())));
}

void ExternalConstant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ Move(reg, reference());
}

void SmiConstant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ Move(reg, value());
}

void Int32Constant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ Move(reg, value());
}

void Float64Constant::DoLoadToRegister(MaglevAssembler* masm,
                                       DoubleRegister reg) {
  __ Move(reg, value());
}

void Constant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ Move(reg, object_.object());
}

void RootConstant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ LoadRoot(reg, index());
}

// ---
// Arch agnostic nodes
// ---

void ExternalConstant::SetValueLocationConstraints() { DefineAsConstant(this); }
void ExternalConstant::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {}

void SmiConstant::SetValueLocationConstraints() { DefineAsConstant(this); }
void SmiConstant::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {}

void Int32Constant::SetValueLocationConstraints() { DefineAsConstant(this); }
void Int32Constant::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {}

void Float64Constant::SetValueLocationConstraints() { DefineAsConstant(this); }
void Float64Constant::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {}

void Constant::SetValueLocationConstraints() { DefineAsConstant(this); }
void Constant::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {}

void RootConstant::SetValueLocationConstraints() { DefineAsConstant(this); }
void RootConstant::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {}

void InitialValue::SetValueLocationConstraints() {
  // TODO(leszeks): Make this nicer.
  result().SetUnallocated(compiler::UnallocatedOperand::FIXED_SLOT,
                          (StandardFrameConstants::kExpressionsOffset -
                           UnoptimizedFrameConstants::kRegisterFileFromFp) /
                                  kSystemPointerSize +
                              source().index(),
                          kNoVreg);
}
void InitialValue::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  // No-op, the value is already in the appropriate slot.
}

void RegisterInput::SetValueLocationConstraints() {
  DefineAsFixed(this, input());
}
void RegisterInput::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  // Nothing to be done, the value is already in the register.
}

void GetSecondReturnedValue::SetValueLocationConstraints() {
  DefineAsFixed(this, kReturnRegister1);
}
void GetSecondReturnedValue::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  // No-op. This is just a hack that binds kReturnRegister1 to a value node.
  // kReturnRegister1 is guaranteed to be free in the register allocator, since
  // previous node in the basic block is a call.
#ifdef DEBUG
  // Check if the previous node is call.
  Node* previous = nullptr;
  for (Node* node : state.block()->nodes()) {
    if (node == this) {
      break;
    }
    previous = node;
  }
  DCHECK_NE(previous, nullptr);
  DCHECK(previous->properties().is_call());
#endif  // DEBUG
}

void Deopt::SetValueLocationConstraints() {}
void Deopt::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  __ EmitEagerDeopt(this, reason());
}

void Phi::SetValueLocationConstraints() {
  for (Input& input : *this) {
    UseAny(input);
  }

  // We have to pass a policy for the result, but it is ignored during register
  // allocation. See StraightForwardRegisterAllocator::AllocateRegisters which
  // has special handling for Phis.
  static const compiler::UnallocatedOperand::ExtendedPolicy kIgnoredPolicy =
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT;

  result().SetUnallocated(kIgnoredPolicy, kNoVreg);
}
void Phi::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {}

namespace {

constexpr Builtin BuiltinFor(Operation operation) {
  switch (operation) {
#define CASE(name)         \
  case Operation::k##name: \
    return Builtin::k##name##_WithFeedback;
    OPERATION_LIST(CASE)
#undef CASE
  }
}

}  // namespace

template <class Derived, Operation kOperation>
void UnaryWithFeedbackNode<Derived, kOperation>::SetValueLocationConstraints() {
  using D = UnaryOp_WithFeedbackDescriptor;
  UseFixed(operand_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(this, kReturnRegister0);
}

template <class Derived, Operation kOperation>
void UnaryWithFeedbackNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  using D = UnaryOp_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(operand_input()), D::GetRegisterParameter(D::kValue));
  __ Move(kContextRegister, masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(BuiltinFor(kOperation));
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

template <class Derived, Operation kOperation>
void BinaryWithFeedbackNode<Derived,
                            kOperation>::SetValueLocationConstraints() {
  using D = BinaryOp_WithFeedbackDescriptor;
  UseFixed(left_input(), D::GetRegisterParameter(D::kLeft));
  UseFixed(right_input(), D::GetRegisterParameter(D::kRight));
  DefineAsFixed(this, kReturnRegister0);
}

template <class Derived, Operation kOperation>
void BinaryWithFeedbackNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  using D = BinaryOp_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(left_input()), D::GetRegisterParameter(D::kLeft));
  DCHECK_EQ(ToRegister(right_input()), D::GetRegisterParameter(D::kRight));
  __ Move(kContextRegister, masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(BuiltinFor(kOperation));
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
GENERIC_OPERATIONS_NODE_LIST(DEF_OPERATION)
#undef DEF_OPERATION

void ConstantGapMove::SetValueLocationConstraints() { UNREACHABLE(); }

namespace {
template <typename T>
struct GetRegister;
template <>
struct GetRegister<Register> {
  static Register Get(compiler::AllocatedOperand target) {
    return target.GetRegister();
  }
};
template <>
struct GetRegister<DoubleRegister> {
  static DoubleRegister Get(compiler::AllocatedOperand target) {
    return target.GetDoubleRegister();
  }
};
}  // namespace

void ConstantGapMove::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  switch (node_->opcode()) {
#define CASE(Name)                                \
  case Opcode::k##Name:                           \
    return node_->Cast<Name>()->DoLoadToRegister( \
        masm, GetRegister<Name::OutputRegister>::Get(target()));
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

void GapMove::SetValueLocationConstraints() { UNREACHABLE(); }
void GapMove::GenerateCode(MaglevAssembler* masm,
                           const ProcessingState& state) {
  DCHECK_EQ(source().representation(), target().representation());
  MachineRepresentation repr = source().representation();
  if (source().IsRegister()) {
    Register source_reg = ToRegister(source());
    if (target().IsAnyRegister()) {
      DCHECK(target().IsRegister());
      __ MoveRepr(repr, ToRegister(target()), source_reg);
    } else {
      __ MoveRepr(repr, masm->ToMemOperand(target()), source_reg);
    }
  } else if (source().IsDoubleRegister()) {
    DoubleRegister source_reg = ToDoubleRegister(source());
    if (target().IsAnyRegister()) {
      DCHECK(target().IsDoubleRegister());
      __ Move(ToDoubleRegister(target()), source_reg);
    } else {
      __ Move(masm->ToMemOperand(target()), source_reg);
    }
  } else {
    DCHECK(source().IsAnyStackSlot());
    MemOperand source_op = masm->ToMemOperand(source());
    if (target().IsRegister()) {
      __ MoveRepr(repr, ToRegister(target()), source_op);
    } else if (target().IsDoubleRegister()) {
      __ Move(ToDoubleRegister(target()), source_op);
    } else {
      DCHECK(target().IsAnyStackSlot());
      __ MoveRepr(repr, masm->ToMemOperand(target()), source_op);
    }
  }
}

void AssertInt32::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void AssertInt32::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  __ CompareInt32(ToRegister(left_input()), ToRegister(right_input()));
  __ Check(ToCondition(condition_), reason_);
}

void CheckUint32IsSmi::SetValueLocationConstraints() { UseRegister(input()); }
void CheckUint32IsSmi::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register reg = ToRegister(input());
  // Perform an unsigned comparison against Smi::kMaxValue.
  __ Cmp(reg, Smi::kMaxValue);
  __ EmitEagerDeoptIf(kUnsignedGreaterThan, DeoptimizeReason::kNotASmi, this);
}

void CheckedSmiUntag::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}

void CheckedSmiUntag::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(input());
  // TODO(leszeks): Consider optimizing away this test and using the carry bit
  // of the `sarl` for cases where the deopt uses the value from a different
  // register.
  Condition is_smi = __ CheckSmi(value);
  __ EmitEagerDeoptIf(NegateCondition(is_smi), DeoptimizeReason::kNotASmi,
                      this);
  __ SmiToInt32(value);
}

void UnsafeSmiUntag::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}

void UnsafeSmiUntag::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(input());
  __ AssertSmi(value);
  __ SmiToInt32(value);
}

int DeleteProperty::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kDeleteProperty>::type;
  return D::GetStackParameterCount();
}
void DeleteProperty::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kDeleteProperty>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object(), D::GetRegisterParameter(D::kObject));
  UseFixed(key(), D::GetRegisterParameter(D::kKey));
  DefineAsFixed(this, kReturnRegister0);
}
void DeleteProperty::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kDeleteProperty>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object()), D::GetRegisterParameter(D::kObject));
  DCHECK_EQ(ToRegister(key()), D::GetRegisterParameter(D::kKey));
  __ Move(D::GetRegisterParameter(D::kLanguageMode),
          Smi::FromInt(static_cast<int>(mode())));
  __ CallBuiltin(Builtin::kDeleteProperty);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int ForInPrepare::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kForInPrepare>::type;
  return D::GetStackParameterCount();
}
void ForInPrepare::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kForInPrepare>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(enumerator(), D::GetRegisterParameter(D::kEnumerator));
  DefineAsFixed(this, kReturnRegister0);
}
void ForInPrepare::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kForInPrepare>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(enumerator()), D::GetRegisterParameter(D::kEnumerator));
  __ Move(D::GetRegisterParameter(D::kVectorIndex),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(Builtin::kForInPrepare);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int ForInNext::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kForInNext>::type;
  return D::GetStackParameterCount();
}
void ForInNext::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kForInNext>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(receiver(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(cache_array(), D::GetRegisterParameter(D::kCacheArray));
  UseFixed(cache_type(), D::GetRegisterParameter(D::kCacheType));
  UseFixed(cache_index(), D::GetRegisterParameter(D::kCacheIndex));
  DefineAsFixed(this, kReturnRegister0);
}
void ForInNext::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kForInNext>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(receiver()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(cache_array()), D::GetRegisterParameter(D::kCacheArray));
  DCHECK_EQ(ToRegister(cache_type()), D::GetRegisterParameter(D::kCacheType));
  DCHECK_EQ(ToRegister(cache_index()), D::GetRegisterParameter(D::kCacheIndex));
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
  // Feedback vector is pushed into the stack.
  static_assert(D::GetStackParameterIndex(D::kFeedbackVector) == 0);
  static_assert(D::GetStackParameterCount() == 1);
  __ Push(feedback().vector);
  __ CallBuiltin(Builtin::kForInNext);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int GetIterator::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kGetIteratorWithFeedback>::type;
  return D::GetStackParameterCount();
}
void GetIterator::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kGetIteratorWithFeedback>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(receiver(), D::GetRegisterParameter(D::kReceiver));
  DefineAsFixed(this, kReturnRegister0);
}
void GetIterator::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kGetIteratorWithFeedback>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(receiver()), D::GetRegisterParameter(D::kReceiver));
  __ Move(D::GetRegisterParameter(D::kLoadSlot),
          TaggedIndex::FromIntptr(load_slot()));
  __ Move(D::GetRegisterParameter(D::kCallSlot),
          TaggedIndex::FromIntptr(call_slot()));
  __ Move(D::GetRegisterParameter(D::kMaybeFeedbackVector), feedback());
  __ CallBuiltin(Builtin::kGetIteratorWithFeedback);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register result = ToRegister(this->result());
  Label is_true, end;
  __ CompareInt32AndJumpIf(ToRegister(left_input()), ToRegister(right_input()),
                           ConditionFor(kOperation), &is_true,
                           Label::Distance::kNear);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kFalseValue);
  __ jmp(&end);
  {
    __ bind(&is_true);
    __ LoadRoot(result, RootIndex::kTrueValue);
  }
  __ bind(&end);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
DEF_OPERATION(Int32Equal)
DEF_OPERATION(Int32StrictEqual)
DEF_OPERATION(Int32LessThan)
DEF_OPERATION(Int32LessThanOrEqual)
DEF_OPERATION(Int32GreaterThan)
DEF_OPERATION(Int32GreaterThanOrEqual)
#undef DEF_OPERATION

void LoadDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void LoadDoubleField::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register tmp = temps.Acquire();
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressTagged(tmp, FieldMemOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ LoadHeapNumberValue(ToDoubleRegister(result()), tmp);
}

void LoadTaggedField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void LoadTaggedField::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  if (this->decompresses_tagged_result()) {
    __ LoadTaggedField(ToRegister(result()), object, offset());
  } else {
    __ LoadTaggedFieldWithoutDecompressing(ToRegister(result()), object,
                                           offset());
  }
}

void LoadTaggedFieldByFieldIndex::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
  set_double_temporaries_needed(1);
}
void LoadTaggedFieldByFieldIndex::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  __ AssertNotSmi(object);
  __ AssertSmi(index);

  ZoneLabelRef done(masm);

  // For in-object properties, the index is encoded as:
  //
  //      index = actual_index | is_double_bit | smi_tag_bit
  //            = actual_index << 2 | is_double_bit << 1
  //
  // The value we want is at the field offset:
  //
  //      (actual_index << kTaggedSizeLog2) + JSObject::kHeaderSize
  //
  // We could get index from actual_index by shifting away the double and smi
  // bits. But, note that `kTaggedSizeLog2 == 2` and `index` encodes
  // `actual_index` with a two bit shift. So, we can do some rearranging
  // to get the offset without shifting:
  //
  //      ((index >> 2) << kTaggedSizeLog2 + JSObject::kHeaderSize
  //
  //    [Expand definitions of index and kTaggedSizeLog2]
  //    = (((actual_index << 2 | is_double_bit << 1) >> 2) << 2)
  //           + JSObject::kHeaderSize
  //
  //    [Cancel out shift down and shift up, clear is_double bit by subtracting]
  //    = (actual_index << 2 | is_double_bit << 1) - (is_double_bit << 1)
  //           + JSObject::kHeaderSize
  //
  //    [Fold together the constants, and collapse definition of index]
  //    = index + (JSObject::kHeaderSize - (is_double_bit << 1))
  //
  //
  // For out-of-object properties, the encoding is:
  //
  //     index = (-1 - actual_index) | is_double_bit | smi_tag_bit
  //           = (-1 - actual_index) << 2 | is_double_bit << 1
  //           = (-1 - actual_index) * 4 + (is_double_bit ? 2 : 0)
  //           = -(actual_index * 4) + (is_double_bit ? 2 : 0) - 4
  //           = -(actual_index << 2) + (is_double_bit ? 2 : 0) - 4
  //
  // The value we want is in the property array at offset:
  //
  //      (actual_index << kTaggedSizeLog2) + FixedArray::kHeaderSize
  //
  //    [Expand definition of kTaggedSizeLog2]
  //    = (actual_index << 2) + FixedArray::kHeaderSize
  //
  //    [Substitute in index]
  //    = (-index + (is_double_bit ? 2 : 0) - 4) + FixedArray::kHeaderSize
  //
  //    [Fold together the constants]
  //    = -index + (FixedArray::kHeaderSize + (is_double_bit ? 2 : 0) - 4))
  //
  // This allows us to simply negate the index register and do a load with
  // otherwise constant offset.

  // Check if field is a mutable double field.
  static constexpr int32_t kIsDoubleBitMask = 1 << kSmiTagSize;
  __ TestInt32AndJumpIfAnySet(
      index, kIsDoubleBitMask,
      __ MakeDeferredCode(
          [](MaglevAssembler* masm, Register object, Register index,
             Register result_reg, RegisterSnapshot register_snapshot,
             ZoneLabelRef done) {
            // The field is a Double field, a.k.a. a mutable HeapNumber.
            static const int kIsDoubleBit = 1;

            // Check if field is in-object or out-of-object. The is_double bit
            // value doesn't matter, since negative values will stay negative.
            Label if_outofobject, loaded_field;
            __ CompareInt32AndJumpIf(index, 0, kLessThan, &if_outofobject);

            // The field is located in the {object} itself.
            {
              // See giant comment above.
              static_assert(kTaggedSizeLog2 == 2);
              static_assert(kSmiTagSize == 1);
              // We haven't untagged, so we need to sign extend.
              __ SignExtend32To64Bits(index, index);
              __ LoadTaggedFieldByIndex(
                  result_reg, object, index, 1,
                  JSObject::kHeaderSize - (kIsDoubleBit << kSmiTagSize));
              __ Jump(&loaded_field);
            }

            __ bind(&if_outofobject);
            {
              MaglevAssembler::ScratchRegisterScope temps(masm);
              Register property_array = temps.Acquire();
              // Load the property array.
              __ LoadTaggedField(
                  property_array,
                  FieldMemOperand(object, JSObject::kPropertiesOrHashOffset));

              // See giant comment above.
              static_assert(kSmiTagSize == 1);
              __ NegateInt32(index);
              __ LoadTaggedFieldByIndex(
                  result_reg, property_array, index, 1,
                  FixedArray::kHeaderSize + (kIsDoubleBit << kSmiTagSize) - 4);
              __ Jump(&loaded_field);
            }

            __ bind(&loaded_field);
            // We may have transitioned in-place away from double, so check that
            // this is a HeapNumber -- otherwise the load is fine and we don't
            // need to copy anything anyway.
            __ JumpIfSmi(result_reg, *done);
            MaglevAssembler::ScratchRegisterScope temps(masm);
            Register map = temps.Acquire();
            // Hack: The temporary allocated for `map` might alias the result
            // register. If it does, use the index register as a temporary
            // instead (since it's clobbered anyway).
            // TODO(leszeks): Extend the result register's lifetime to overlap
            // the temporaries, so that this alias isn't possible.
            if (map == result_reg) {
              DCHECK_NE(map, index);
              map = index;
            }
            __ LoadMap(map, result_reg);
            __ JumpIfNotRoot(map, RootIndex::kHeapNumberMap, *done);
            DoubleRegister double_value = temps.AcquireDouble();
            __ LoadHeapNumberValue(double_value, result_reg);
            __ AllocateHeapNumber(register_snapshot, result_reg, double_value);
            __ Jump(*done);
          },
          object, index, result_reg, register_snapshot(), done));

  // The field is a proper Tagged field on {object}. The {index} is shifted
  // to the left by one in the code below.
  {
    static const int kIsDoubleBit = 0;

    // Check if field is in-object or out-of-object. The is_double bit value
    // doesn't matter, since negative values will stay negative.
    Label if_outofobject;
    __ CompareInt32AndJumpIf(index, 0, kLessThan, &if_outofobject);

    // The field is located in the {object} itself.
    {
      // See giant comment above.
      static_assert(kTaggedSizeLog2 == 2);
      static_assert(kSmiTagSize == 1);
      // We haven't untagged, so we need to sign extend.
      __ SignExtend32To64Bits(index, index);
      __ LoadTaggedFieldByIndex(
          result_reg, object, index, 1,
          JSObject::kHeaderSize - (kIsDoubleBit << kSmiTagSize));
      __ Jump(*done);
    }

    __ bind(&if_outofobject);
    {
      MaglevAssembler::ScratchRegisterScope temps(masm);
      Register property_array = temps.Acquire();
      // Load the property array.
      __ LoadTaggedField(
          property_array,
          FieldMemOperand(object, JSObject::kPropertiesOrHashOffset));

      // See giant comment above.
      static_assert(kSmiTagSize == 1);
      __ NegateInt32(index);
      __ LoadTaggedFieldByIndex(
          result_reg, property_array, index, 1,
          FixedArray::kHeaderSize + (kIsDoubleBit << kSmiTagSize) - 4);
      // Fallthrough to `done`.
    }
  }

  __ bind(*done);
}

int StoreMap::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void StoreMap::SetValueLocationConstraints() {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
  set_temporaries_needed(1);
}
void StoreMap::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));
  Register value = temps.Acquire();
  __ Move(value, map_.object());

  __ StoreTaggedFieldWithWriteBarrier(object, HeapObject::kMapOffset, value,
                                      register_snapshot(),
                                      MaglevAssembler::kValueIsDecompressed,
                                      MaglevAssembler::kValueCannotBeSmi);
}

int StoreTaggedFieldWithWriteBarrier::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void StoreTaggedFieldWithWriteBarrier::SetValueLocationConstraints() {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
  UseRegister(value_input());
}
void StoreTaggedFieldWithWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));
  Register value = ToRegister(value_input());

  __ StoreTaggedFieldWithWriteBarrier(
      object, offset(), value, register_snapshot(),
      value_input().node()->decompresses_tagged_result()
          ? MaglevAssembler::kValueIsDecompressed
          : MaglevAssembler::kValueIsCompressed,
      MaglevAssembler::kValueCanBeSmi);
}

namespace {

template <typename NodeT, typename Function, typename... Args>
void EmitPolymorphicAccesses(MaglevAssembler* masm, NodeT* node,
                             Register object, Function&& f, Args&&... args) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register object_map = temps.Acquire();

  Label done;
  Label is_number;

  Condition is_smi = __ CheckSmi(object);
  __ JumpIf(is_smi, &is_number);
  __ LoadMap(object_map, object);

  for (const PolymorphicAccessInfo& access_info : node->access_infos()) {
    Label next;
    Label map_found;
    auto& maps = access_info.maps();

    if (HasOnlyNumberMaps(base::VectorOf(maps))) {
      __ CompareRoot(object_map, RootIndex::kHeapNumberMap);
      __ JumpIf(kNotEqual, &next);
      // Fallthrough... to map_found.
      DCHECK(!is_number.is_bound());
      __ bind(&is_number);
    } else if (HasOnlyStringMaps(base::VectorOf(maps))) {
      __ CompareInstanceTypeRange(object_map, FIRST_STRING_TYPE,
                                  LAST_STRING_TYPE);
      __ JumpIf(kUnsignedGreaterThan, &next);
      // Fallthrough... to map_found.
    } else {
      for (auto it = maps.begin(); it != maps.end(); ++it) {
        __ CompareTagged(object_map, it->object());
        if (it == maps.end() - 1) {
          __ JumpIf(kNotEqual, &next);
          // Fallthrough... to map_found.
        } else {
          __ JumpIf(kEqual, &map_found);
        }
      }
    }

    __ bind(&map_found);
    f(masm, node, access_info, object, object_map, std::forward<Args>(args)...);
    __ Jump(&done);

    __ bind(&next);
  }

  // A HeapNumberMap was not found, we should eager deopt here in case of a
  // number.
  if (!is_number.is_bound()) {
    __ bind(&is_number);
  }

  // No map matched!
  __ EmitEagerDeopt(node, DeoptimizeReason::kWrongMap);
  __ bind(&done);
}

}  // namespace

void LoadPolymorphicTaggedField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
  set_double_temporaries_needed(1);
}
void LoadPolymorphicTaggedField::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register object = ToRegister(object_input());
  EmitPolymorphicAccesses(
      masm, this, object,
      [](MaglevAssembler* masm, LoadPolymorphicTaggedField* node,
         const PolymorphicAccessInfo& access_info, Register object,
         Register map, Register result) {
        switch (access_info.kind()) {
          case PolymorphicAccessInfo::kNotFound:
            __ LoadRoot(result, RootIndex::kUndefinedValue);
            break;
          case PolymorphicAccessInfo::kConstant: {
            Handle<Object> constant = access_info.constant();
            if (constant->IsSmi()) {
              __ Move(result, Smi::cast(*constant));
            } else {
              DCHECK(access_info.constant()->IsHeapObject());
              __ Move(result, Handle<HeapObject>::cast(constant));
            }
            break;
          }
          case PolymorphicAccessInfo::kModuleExport: {
            Register cell = map;  // Reuse scratch.
            __ Move(cell, access_info.cell());
            __ AssertNotSmi(cell);
            __ DecompressTagged(result,
                                FieldMemOperand(cell, Cell::kValueOffset));
            break;
          }
          case PolymorphicAccessInfo::kDataLoad: {
            MaglevAssembler::ScratchRegisterScope temps(masm);
            DoubleRegister double_scratch = temps.AcquireDouble();
            __ LoadDataField(access_info, result, object, map);
            if (access_info.field_index().is_double()) {
              __ LoadHeapNumberValue(double_scratch, result);
              __ AllocateHeapNumber(node->register_snapshot(), result,
                                    double_scratch);
            }
            break;
          }
          case PolymorphicAccessInfo::kStringLength:
            __ StringLength(result, object);
            __ SmiTag(result);
            break;
        }
      },
      ToRegister(result()));
}

void LoadPolymorphicDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void LoadPolymorphicDoubleField::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register object = ToRegister(object_input());
  EmitPolymorphicAccesses(
      masm, this, object,
      [](MaglevAssembler* masm, LoadPolymorphicDoubleField* node,
         const PolymorphicAccessInfo& access_info, Register object,
         Register map, DoubleRegister result) {
        Register scratch = map;
        switch (access_info.kind()) {
          case PolymorphicAccessInfo::kDataLoad:
            __ LoadDataField(access_info, scratch, object, map);
            switch (access_info.field_representation().kind()) {
              case Representation::kSmi:
                __ SmiToDouble(result, scratch);
                break;
              case Representation::kDouble:
                __ LoadHeapNumberValue(result, scratch);
                break;
              default:
                UNREACHABLE();
            }
            break;
          case PolymorphicAccessInfo::kConstant: {
            Handle<Object> constant = access_info.constant();
            if (constant->IsSmi()) {
              __ Move(scratch, Smi::cast(*constant));
              __ SmiToDouble(result, scratch);
            } else {
              DCHECK(constant->IsHeapNumber());
              __ Move(result, Handle<HeapNumber>::cast(constant)->value());
            }
            break;
          }
          case PolymorphicAccessInfo::kStringLength:
            __ StringLength(scratch, object);
            __ Int32ToDouble(result, scratch);
            break;
          default:
            UNREACHABLE();
        }
      },
      ToDoubleRegister(result()));
}

void LoadEnumCacheLength::SetValueLocationConstraints() {
  UseRegister(map_input());
  DefineAsRegister(this);
}
void LoadEnumCacheLength::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register map = ToRegister(map_input());
  Register result_reg = ToRegister(result());
  __ AssertMap(map);
  __ LoadBitField<Map::Bits3::EnumLengthBits>(
      result_reg, FieldMemOperand(map, Map::kBitField3Offset));
}

int LoadGlobal::MaxCallStackArgs() const {
  if (typeof_mode() == TypeofMode::kNotInside) {
    using D = CallInterfaceDescriptorFor<Builtin::kLoadGlobalIC>::type;
    return D::GetStackParameterCount();
  } else {
    using D =
        CallInterfaceDescriptorFor<Builtin::kLoadGlobalICInsideTypeof>::type;
    return D::GetStackParameterCount();
  }
}
void LoadGlobal::SetValueLocationConstraints() {
  UseFixed(context(), kContextRegister);
  DefineAsFixed(this, kReturnRegister0);
}
void LoadGlobal::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  // TODO(leszeks): Port the nice Sparkplug CallBuiltin helper.
  if (typeof_mode() == TypeofMode::kNotInside) {
    using D = CallInterfaceDescriptorFor<Builtin::kLoadGlobalIC>::type;
    DCHECK_EQ(ToRegister(context()), kContextRegister);
    __ Move(D::GetRegisterParameter(D::kName), name().object());
    __ Move(D::GetRegisterParameter(D::kSlot),
            TaggedIndex::FromIntptr(feedback().index()));
    __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);

    __ CallBuiltin(Builtin::kLoadGlobalIC);
  } else {
    DCHECK_EQ(typeof_mode(), TypeofMode::kInside);
    using D =
        CallInterfaceDescriptorFor<Builtin::kLoadGlobalICInsideTypeof>::type;
    DCHECK_EQ(ToRegister(context()), kContextRegister);
    __ Move(D::GetRegisterParameter(D::kName), name().object());
    __ Move(D::GetRegisterParameter(D::kSlot),
            TaggedIndex::FromIntptr(feedback().index()));
    __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);

    __ CallBuiltin(Builtin::kLoadGlobalICInsideTypeof);
  }

  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int StoreGlobal::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreGlobalIC>::type;
  return D::GetStackParameterCount();
}
void StoreGlobal::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreGlobalIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(this, kReturnRegister0);
}
void StoreGlobal::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreGlobalIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);

  __ CallBuiltin(Builtin::kStoreGlobalIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void CheckValue::SetValueLocationConstraints() { UseRegister(target_input()); }
void CheckValue::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register target = ToRegister(target_input());
  __ CompareTagged(target, value().object());
  __ EmitEagerDeoptIfNotEqual(DeoptimizeReason::kWrongValue, this);
}

void CheckValueEqualsString::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kStringEqual>::type;
  UseFixed(target_input(), D::GetRegisterParameter(D::kLeft));
  RequireSpecificTemporary(D::GetRegisterParameter(D::kLength));
}
void CheckValueEqualsString::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStringEqual>::type;

  ZoneLabelRef end(masm);
  DCHECK_EQ(D::GetRegisterParameter(D::kLeft), ToRegister(target_input()));
  Register target = D::GetRegisterParameter(D::kLeft);
  // Maybe the string is internalized already, do a fast reference check first.
  __ CompareTagged(target, value().object());
  __ JumpIf(kEqual, *end, Label::kNear);

  __ EmitEagerDeoptIf(__ CheckSmi(target), DeoptimizeReason::kWrongValue, this);
  __ CompareObjectTypeRange(target, FIRST_STRING_TYPE, LAST_STRING_TYPE);

  __ JumpToDeferredIf(
      kUnsignedLessThanEqual,
      [](MaglevAssembler* masm, CheckValueEqualsString* node,
         ZoneLabelRef end) {
        Register target = D::GetRegisterParameter(D::kLeft);
        Register string_length = D::GetRegisterParameter(D::kLength);
        __ StringLength(string_length, target);
        __ CompareInt32(string_length, node->value().length());
        __ EmitEagerDeoptIf(kNotEqual, DeoptimizeReason::kWrongValue, node);

        RegisterSnapshot snapshot = node->register_snapshot();
        AddDeoptRegistersToSnapshot(&snapshot, node->eager_deopt_info());
        {
          SaveRegisterStateForCall save_register_state(masm, snapshot);
          __ Move(kContextRegister, masm->native_context().object());
          __ Move(D::GetRegisterParameter(D::kRight), node->value().object());
          __ CallBuiltin(Builtin::kStringEqual);
          save_register_state.DefineSafepoint();
          // Compare before restoring registers, so that the deopt below has the
          // correct register set.
          __ CompareRoot(kReturnRegister0, RootIndex::kTrueValue);
        }
        __ JumpIf(kEqual, *end);
        __ EmitEagerDeopt(node, DeoptimizeReason::kWrongValue);
      },
      this, end);

  __ EmitEagerDeopt(this, DeoptimizeReason::kWrongValue);

  __ bind(*end);
}

void CheckDynamicValue::SetValueLocationConstraints() {
  UseRegister(first_input());
  UseRegister(second_input());
}
void CheckDynamicValue::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register first = ToRegister(first_input());
  Register second = ToRegister(second_input());
  __ CompareTagged(first, second);
  __ EmitEagerDeoptIfNotEqual(DeoptimizeReason::kWrongValue, this);
}

void CheckSmi::SetValueLocationConstraints() { UseRegister(receiver_input()); }
void CheckSmi::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  __ EmitEagerDeoptIf(NegateCondition(is_smi), DeoptimizeReason::kNotASmi,
                      this);
}

void CheckHeapObject::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckHeapObject::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kSmi, this);
}

void CheckSymbol::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckSymbol::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotASymbol, this);
  }
  __ IsObjectType(object, SYMBOL_TYPE);
  __ EmitEagerDeoptIf(kNotEqual, DeoptimizeReason::kNotASymbol, this);
}

void CheckInstanceType::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckInstanceType::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongInstanceType, this);
  }
  __ IsObjectType(object, instance_type());
  __ EmitEagerDeoptIf(kNotEqual, DeoptimizeReason::kWrongInstanceType, this);
}

void CheckFixedArrayNonEmpty::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  set_temporaries_needed(1);
}
void CheckFixedArrayNonEmpty::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  __ AssertNotSmi(object);

  if (v8_flags.debug_code) {
    Label ok;
    __ IsObjectType(object, FIXED_ARRAY_TYPE);
    __ JumpIf(kEqual, &ok);
    __ IsObjectType(object, FIXED_DOUBLE_ARRAY_TYPE);
    __ Assert(kEqual, AbortReason::kOperandIsNotAFixedArray);
    __ bind(&ok);
  }
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register length = temps.Acquire();
  __ LoadTaggedSignedField(length, object, FixedArrayBase::kLengthOffset);
  __ CompareSmiAndJumpIf(
      length, Smi::zero(), kEqual,
      __ GetDeoptLabel(this, DeoptimizeReason::kWrongEnumIndices));
}

void CheckInt32Condition::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void CheckInt32Condition::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  __ CompareInt32(ToRegister(left_input()), ToRegister(right_input()));
  __ EmitEagerDeoptIf(NegateCondition(ToCondition(condition_)), reason_, this);
}

void CheckString::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckString::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotAString, this);
  }
  __ CompareObjectTypeRange(object, FIRST_STRING_TYPE, LAST_STRING_TYPE);
  __ EmitEagerDeoptIf(kUnsignedGreaterThan, DeoptimizeReason::kNotAString,
                      this);
}

void ConvertHoleToUndefined::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineSameAsFirst(this);
}
void ConvertHoleToUndefined::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  Label done;
  DCHECK_EQ(ToRegister(object_input()), ToRegister(result()));
  __ JumpIfNotRoot(ToRegister(object_input()), RootIndex::kTheHoleValue, &done);
  __ LoadRoot(ToRegister(result()), RootIndex::kUndefinedValue);
  __ bind(&done);
}

int ConvertReceiver::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  return D::GetStackParameterCount();
}
void ConvertReceiver::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(receiver_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(this, kReturnRegister0);
}
void ConvertReceiver::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Label convert_to_object, done;
  Register receiver = ToRegister(receiver_input());
  __ JumpIfSmi(receiver, &convert_to_object, Label::Distance::kNear);
  __ JumpIfJSAnyIsNotPrimitive(receiver, &done);

  compiler::JSHeapBroker* broker = masm->compilation_info()->broker();
  if (mode_ != ConvertReceiverMode::kNotNullOrUndefined) {
    Label convert_global_proxy;
    __ JumpIfRoot(receiver, RootIndex::kUndefinedValue, &convert_global_proxy,
                  Label::Distance::kNear);
    __ JumpIfNotRoot(receiver, RootIndex::kNullValue, &convert_to_object,
                     Label::Distance::kNear);
    __ bind(&convert_global_proxy);
    // Patch receiver to global proxy.
    __ Move(
        ToRegister(result()),
        target_.native_context(broker).global_proxy_object(broker).object());
    __ Jump(&done);
  }

  __ bind(&convert_to_object);
  // ToObject needs to be ran with the target context installed.
  __ Move(kContextRegister, target_.context(broker).object());
  __ CallBuiltin(Builtin::kToObject);
  __ bind(&done);
}

int CreateEmptyArrayLiteral::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kCreateEmptyArrayLiteral>::type;
  return D::GetStackParameterCount();
}
void CreateEmptyArrayLiteral::SetValueLocationConstraints() {
  DefineAsFixed(this, kReturnRegister0);
}
void CreateEmptyArrayLiteral::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  using D = CreateEmptyArrayLiteralDescriptor;
  __ Move(kContextRegister, masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), Smi::FromInt(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(Builtin::kCreateEmptyArrayLiteral);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CreateObjectLiteral::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kCreateObjectLiteral)->nargs, 4);
  return 4;
}
void CreateObjectLiteral::SetValueLocationConstraints() {
  DefineAsFixed(this, kReturnRegister0);
}
void CreateObjectLiteral::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  __ Move(kContextRegister, masm->native_context().object());
  __ Push(feedback().vector, TaggedIndex::FromIntptr(feedback().index()),
          boilerplate_descriptor().object(), Smi::FromInt(flags()));
  __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CreateShallowArrayLiteral::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kCreateEmptyArrayLiteral>::type;
  return D::GetStackParameterCount();
}
void CreateShallowArrayLiteral::SetValueLocationConstraints() {
  DefineAsFixed(this, kReturnRegister0);
}
void CreateShallowArrayLiteral::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  using D = CreateShallowArrayLiteralDescriptor;
  __ Move(D::ContextRegister(), masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kMaybeFeedbackVector), feedback().vector);
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kConstantElements),
          constant_elements().object());
  __ Move(D::GetRegisterParameter(D::kFlags), Smi::FromInt(flags()));
  __ CallBuiltin(Builtin::kCreateShallowArrayLiteral);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CreateArrayLiteral::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kCreateArrayLiteral)->nargs, 4);
  return 4;
}
void CreateArrayLiteral::SetValueLocationConstraints() {
  DefineAsFixed(this, kReturnRegister0);
}
void CreateArrayLiteral::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  __ Move(kContextRegister, masm->native_context().object());
  __ Push(feedback().vector, TaggedIndex::FromIntptr(feedback().index()),
          constant_elements().object(), Smi::FromInt(flags()));
  __ CallRuntime(Runtime::kCreateArrayLiteral, 4);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CreateShallowObjectLiteral::MaxCallStackArgs() const {
  using D =
      CallInterfaceDescriptorFor<Builtin::kCreateShallowObjectLiteral>::type;
  return D::GetStackParameterCount();
}
void CreateShallowObjectLiteral::SetValueLocationConstraints() {
  DefineAsFixed(this, kReturnRegister0);
}
void CreateShallowObjectLiteral::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  using D = CreateShallowObjectLiteralDescriptor;
  __ Move(D::ContextRegister(), masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kMaybeFeedbackVector), feedback().vector);
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kDesc), boilerplate_descriptor().object());
  __ Move(D::GetRegisterParameter(D::kFlags), Smi::FromInt(flags()));
  __ CallBuiltin(Builtin::kCreateShallowObjectLiteral);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void AllocateRaw::SetValueLocationConstraints() { DefineAsRegister(this); }

void AllocateRaw::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  __ Allocate(register_snapshot(), ToRegister(result()), size(),
              allocation_type());
}

int CreateClosure::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(pretenured() ? Runtime::kNewClosure_Tenured
                                                : Runtime::kNewClosure)
                ->nargs,
            2);
  return 2;
}
void CreateClosure::SetValueLocationConstraints() {
  UseFixed(context(), kContextRegister);
  DefineAsFixed(this, kReturnRegister0);
}
void CreateClosure::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Runtime::FunctionId function_id =
      pretenured() ? Runtime::kNewClosure_Tenured : Runtime::kNewClosure;
  __ Push(shared_function_info().object(), feedback_cell().object());
  __ CallRuntime(function_id);
}

int FastCreateClosure::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kFastNewClosure>::type;
  return D::GetStackParameterCount();
}
void FastCreateClosure::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kFastNewClosure>::type;
  static_assert(D::HasContextParameter());
  UseFixed(context(), D::ContextRegister());
  DefineAsFixed(this, kReturnRegister0);
}
void FastCreateClosure::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kFastNewClosure>::type;

  DCHECK_EQ(ToRegister(context()), D::ContextRegister());
  __ Move(D::GetRegisterParameter(D::kSharedFunctionInfo),
          shared_function_info().object());
  __ Move(D::GetRegisterParameter(D::kFeedbackCell), feedback_cell().object());
  __ CallBuiltin(Builtin::kFastNewClosure);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CreateFunctionContext::MaxCallStackArgs() const {
  if (scope_type() == FUNCTION_SCOPE) {
    using D = CallInterfaceDescriptorFor<
        Builtin::kFastNewFunctionContextFunction>::type;
    return D::GetStackParameterCount();
  } else {
    using D =
        CallInterfaceDescriptorFor<Builtin::kFastNewFunctionContextEval>::type;
    return D::GetStackParameterCount();
  }
}
void CreateFunctionContext::SetValueLocationConstraints() {
  DCHECK_LE(slot_count(),
            static_cast<uint32_t>(
                ConstructorBuiltins::MaximumFunctionContextSlots()));
  if (scope_type() == FUNCTION_SCOPE) {
    using D = CallInterfaceDescriptorFor<
        Builtin::kFastNewFunctionContextFunction>::type;
    static_assert(D::HasContextParameter());
    UseFixed(context(), D::ContextRegister());
  } else {
    DCHECK_EQ(scope_type(), ScopeType::EVAL_SCOPE);
    using D =
        CallInterfaceDescriptorFor<Builtin::kFastNewFunctionContextEval>::type;
    static_assert(D::HasContextParameter());
    UseFixed(context(), D::ContextRegister());
  }
  DefineAsFixed(this, kReturnRegister0);
}
void CreateFunctionContext::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  if (scope_type() == FUNCTION_SCOPE) {
    using D = CallInterfaceDescriptorFor<
        Builtin::kFastNewFunctionContextFunction>::type;
    DCHECK_EQ(ToRegister(context()), D::ContextRegister());
    __ Move(D::GetRegisterParameter(D::kScopeInfo), scope_info().object());
    __ Move(D::GetRegisterParameter(D::kSlots), slot_count());
    // TODO(leszeks): Consider inlining this allocation.
    __ CallBuiltin(Builtin::kFastNewFunctionContextFunction);
  } else {
    DCHECK_EQ(scope_type(), ScopeType::EVAL_SCOPE);
    using D =
        CallInterfaceDescriptorFor<Builtin::kFastNewFunctionContextEval>::type;
    DCHECK_EQ(ToRegister(context()), D::ContextRegister());
    __ Move(D::GetRegisterParameter(D::kScopeInfo), scope_info().object());
    __ Move(D::GetRegisterParameter(D::kSlots), slot_count());
    __ CallBuiltin(Builtin::kFastNewFunctionContextEval);
  }
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CreateRegExpLiteral::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kCreateRegExpLiteral>::type;
  return D::GetStackParameterCount();
}
void CreateRegExpLiteral::SetValueLocationConstraints() {
  DefineAsFixed(this, kReturnRegister0);
}
void CreateRegExpLiteral::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  using D = CreateRegExpLiteralDescriptor;
  __ Move(D::ContextRegister(), masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kMaybeFeedbackVector), feedback().vector);
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kPattern), pattern().object());
  __ Move(D::GetRegisterParameter(D::kFlags), Smi::FromInt(flags()));
  __ CallBuiltin(Builtin::kCreateRegExpLiteral);
}

int GetTemplateObject::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kGetTemplateObject>::type;
  return D::GetStackParameterCount();
}
void GetTemplateObject::SetValueLocationConstraints() {
  using D = GetTemplateObjectDescriptor;
  UseFixed(description(), D::GetRegisterParameter(D::kDescription));
  DefineAsFixed(this, kReturnRegister0);
}
void GetTemplateObject::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  using D = GetTemplateObjectDescriptor;
  __ Move(D::ContextRegister(), masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kMaybeFeedbackVector), feedback().vector);
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().slot.ToInt());
  __ Move(D::GetRegisterParameter(D::kShared), shared_function_info_.object());
  __ CallBuiltin(Builtin::kGetTemplateObject);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void DebugBreak::SetValueLocationConstraints() {}
void DebugBreak::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  __ DebugBreak();
}

int Abort::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kAbort)->nargs, 1);
  return 1;
}
void Abort::SetValueLocationConstraints() {}
void Abort::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  __ Push(Smi::FromInt(static_cast<int>(reason())));
  __ CallRuntime(Runtime::kAbort, 1);
  __ Trap();
}

void LogicalNot::SetValueLocationConstraints() {
  UseAny(value());
  DefineAsRegister(this);
}
void LogicalNot::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  if (v8_flags.debug_code) {
    // LogicalNot expects either TrueValue or FalseValue.
    Label next;
    __ JumpIf(__ IsRootConstant(value(), RootIndex::kFalseValue), &next);
    __ JumpIf(__ IsRootConstant(value(), RootIndex::kTrueValue), &next);
    __ Abort(AbortReason::kUnexpectedValue);
    __ bind(&next);
  }

  Label return_false, done;
  __ JumpIf(__ IsRootConstant(value(), RootIndex::kTrueValue), &return_false);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ Jump(&done);

  __ bind(&return_false);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);

  __ bind(&done);
}

int LoadNamedGeneric::MaxCallStackArgs() const {
  return LoadWithVectorDescriptor::GetStackParameterCount();
}
void LoadNamedGeneric::SetValueLocationConstraints() {
  using D = LoadWithVectorDescriptor;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  DefineAsFixed(this, kReturnRegister0);
}
void LoadNamedGeneric::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  using D = LoadWithVectorDescriptor;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kLoadIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int LoadNamedFromSuperGeneric::MaxCallStackArgs() const {
  return LoadWithReceiverAndVectorDescriptor::GetStackParameterCount();
}
void LoadNamedFromSuperGeneric::SetValueLocationConstraints() {
  using D = LoadWithReceiverAndVectorDescriptor;
  UseFixed(context(), kContextRegister);
  UseFixed(receiver(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(lookup_start_object(),
           D::GetRegisterParameter(D::kLookupStartObject));
  DefineAsFixed(this, kReturnRegister0);
}
void LoadNamedFromSuperGeneric::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  using D = LoadWithReceiverAndVectorDescriptor;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(receiver()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(lookup_start_object()),
            D::GetRegisterParameter(D::kLookupStartObject));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kLoadSuperIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int SetNamedGeneric::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreIC>::type;
  return D::GetStackParameterCount();
}
void SetNamedGeneric::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(this, kReturnRegister0);
}
void SetNamedGeneric::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kStoreIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int DefineNamedOwnGeneric::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineNamedOwnIC>::type;
  return D::GetStackParameterCount();
}
void DefineNamedOwnGeneric::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineNamedOwnIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(this, kReturnRegister0);
}
void DefineNamedOwnGeneric::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineNamedOwnIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kDefineNamedOwnIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void EnsureWritableFastElements::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(object_input());
  set_temporaries_needed(1);
  DefineSameAsFirst(this);
}
void EnsureWritableFastElements::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register elements = ToRegister(elements_input());
  Register result_reg = ToRegister(result());
  DCHECK_EQ(elements, result_reg);
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ CompareMapWithRoot(elements, RootIndex::kFixedArrayMap, scratch);
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      kNotEqual,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         Register result_reg, RegisterSnapshot snapshot) {
        {
          using D = CallInterfaceDescriptorFor<
              Builtin::kCopyFastSmiOrObjectElements>::type;
          snapshot.live_registers.clear(result_reg);
          snapshot.live_tagged_registers.clear(result_reg);
          SaveRegisterStateForCall save_register_state(masm, snapshot);
          __ Move(D::GetRegisterParameter(D::kObject), object);
          __ CallBuiltin(Builtin::kCopyFastSmiOrObjectElements);
          save_register_state.DefineSafepoint();
          __ Move(result_reg, kReturnRegister0);
        }
        __ Jump(*done);
      },
      done, object, result_reg, register_snapshot());
  __ bind(*done);
}

int SetKeyedGeneric::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedStoreIC>::type;
  return D::GetStackParameterCount();
}
void SetKeyedGeneric::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedStoreIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(key_input(), D::GetRegisterParameter(D::kName));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(this, kReturnRegister0);
}
void SetKeyedGeneric::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedStoreIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(key_input()), D::GetRegisterParameter(D::kName));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kKeyedStoreIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int DefineKeyedOwnGeneric::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineKeyedOwnIC>::type;
  return D::GetStackParameterCount();
}
void DefineKeyedOwnGeneric::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineKeyedOwnIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(key_input(), D::GetRegisterParameter(D::kName));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  UseFixed(flags_input(), D::GetRegisterParameter(D::kFlags));
  DefineAsFixed(this, kReturnRegister0);
}
void DefineKeyedOwnGeneric::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineKeyedOwnIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(key_input()), D::GetRegisterParameter(D::kName));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  DCHECK_EQ(ToRegister(flags_input()), D::GetRegisterParameter(D::kFlags));
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Push(feedback().vector);
  __ CallBuiltin(Builtin::kDefineKeyedOwnIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int StoreInArrayLiteralGeneric::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreInArrayLiteralIC>::type;
  return D::GetStackParameterCount();
}
void StoreInArrayLiteralGeneric::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreInArrayLiteralIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(name_input(), D::GetRegisterParameter(D::kName));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(this, kReturnRegister0);
}
void StoreInArrayLiteralGeneric::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreInArrayLiteralIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  DCHECK_EQ(ToRegister(name_input()), D::GetRegisterParameter(D::kName));
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kStoreInArrayLiteralIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void GeneratorRestoreRegister::SetValueLocationConstraints() {
  UseRegister(array_input());
  UseRegister(stale_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void GeneratorRestoreRegister::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register temp = temps.Acquire();
  Register array = ToRegister(array_input());
  Register stale = ToRegister(stale_input());
  Register result_reg = ToRegister(result());

  // The input and the output can alias, if that happens we use a temporary
  // register and a move at the end.
  Register value = (array == result_reg ? temp : result_reg);

  // Loads the current value in the generator register file.
  __ DecompressTagged(
      value, FieldMemOperand(array, FixedArray::OffsetOfElementAt(index())));

  // And trashs it with StaleRegisterConstant.
  __ StoreTaggedField(
      FieldMemOperand(array, FixedArray::OffsetOfElementAt(index())), stale);

  if (value != result_reg) {
    __ Move(result_reg, value);
  }
}

int GeneratorStore::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void GeneratorStore::SetValueLocationConstraints() {
  UseAny(context_input());
  UseRegister(generator_input());
  for (int i = 0; i < num_parameters_and_registers(); i++) {
    UseAny(parameters_and_registers(i));
  }
  RequireSpecificTemporary(WriteBarrierDescriptor::ObjectRegister());
  RequireSpecificTemporary(WriteBarrierDescriptor::SlotAddressRegister());
}
void GeneratorStore::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register generator = ToRegister(generator_input());
  Register array = WriteBarrierDescriptor::ObjectRegister();
  __ LoadTaggedField(array, generator,
                     JSGeneratorObject::kParametersAndRegistersOffset);

  RegisterSnapshot register_snapshot_during_store = register_snapshot();
  // Include the array and generator registers in the register snapshot while
  // storing parameters and registers, to avoid the write barrier clobbering
  // them.
  register_snapshot_during_store.live_registers.set(array);
  register_snapshot_during_store.live_tagged_registers.set(array);
  register_snapshot_during_store.live_registers.set(generator);
  register_snapshot_during_store.live_tagged_registers.set(generator);
  for (int i = 0; i < num_parameters_and_registers(); i++) {
    // Use WriteBarrierDescriptor::SlotAddressRegister() as the temporary for
    // the value -- it'll be clobbered by StoreTaggedFieldWithWriteBarrier since
    // it's not in the register snapshot, but that's ok, and a clobberable value
    // register lets the write barrier emit slightly better code.
    Input value_input = parameters_and_registers(i);
    Register value = __ FromAnyToRegister(
        value_input, WriteBarrierDescriptor::SlotAddressRegister());
    // Include the value register in the live set, in case it is used by future
    // inputs.
    register_snapshot_during_store.live_registers.set(value);
    register_snapshot_during_store.live_tagged_registers.set(value);
    __ StoreTaggedFieldWithWriteBarrier(
        array, FixedArray::OffsetOfElementAt(i), value,
        register_snapshot_during_store,
        value_input.node()->decompresses_tagged_result()
            ? MaglevAssembler::kValueIsDecompressed
            : MaglevAssembler::kValueIsCompressed,
        MaglevAssembler::kValueCanBeSmi);
  }

  __ StoreTaggedSignedField(generator, JSGeneratorObject::kContinuationOffset,
                            Smi::FromInt(suspend_id()));
  __ StoreTaggedSignedField(generator,
                            JSGeneratorObject::kInputOrDebugPosOffset,
                            Smi::FromInt(bytecode_offset()));

  // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
  // register, see comment above. At this point we no longer need to preserve
  // the array or generator registers, so use the original register snapshot.
  Register context = __ FromAnyToRegister(
      context_input(), WriteBarrierDescriptor::SlotAddressRegister());
  __ StoreTaggedFieldWithWriteBarrier(
      generator, JSGeneratorObject::kContextOffset, context,
      register_snapshot(),
      context_input().node()->decompresses_tagged_result()
          ? MaglevAssembler::kValueIsDecompressed
          : MaglevAssembler::kValueIsCompressed,
      MaglevAssembler::kValueCannotBeSmi);
}

int GetKeyedGeneric::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedLoadIC>::type;
  return D::GetStackParameterCount();
}
void GetKeyedGeneric::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedLoadIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(key_input(), D::GetRegisterParameter(D::kName));
  DefineAsFixed(this, kReturnRegister0);
}
void GetKeyedGeneric::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedLoadIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(key_input()), D::GetRegisterParameter(D::kName));
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kKeyedLoadIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void Float64ToTagged::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64ToTagged::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  Register object = ToRegister(result());
  Label box, done;
  if (canonicalize_smi()) {
    __ TryTruncateDoubleToInt32(object, value, &box);
    __ SmiTagInt32(object, &box);
    __ jmp(&done);
    __ bind(&box);
  }
  __ AllocateHeapNumber(register_snapshot(), object, value);
  if (canonicalize_smi()) {
    __ bind(&done);
  }
}

void Float64Round::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
  if (kind_ == Kind::kNearest) {
    set_double_temporaries_needed(1);
  }
}
void HoleyFloat64Box::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void HoleyFloat64Box::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  ZoneLabelRef done(masm);
  DoubleRegister value = ToDoubleRegister(input());
  // Using return as scratch register.
  Register repr = ToRegister(result());
  Register object = ToRegister(result());
  __ DoubleToInt64Repr(repr, value);
  __ JumpToDeferredIf(
      __ IsInt64Constant(repr, kHoleNanInt64),
      [](MaglevAssembler* masm, Register object, ZoneLabelRef done) {
        __ LoadRoot(object, RootIndex::kUndefinedValue);
        __ Jump(*done);
      },
      object, done);
  __ AllocateHeapNumber(register_snapshot(), object, value);
  __ bind(*done);
}

void CheckedSmiTagFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedSmiTagFloat64::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  Register object = ToRegister(result());

  __ TryTruncateDoubleToInt32(
      object, value, __ GetDeoptLabel(this, DeoptimizeReason::kNotASmi));
  __ SmiTagInt32(object, __ GetDeoptLabel(this, DeoptimizeReason::kNotASmi));
}

void StoreFloat64::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreFloat64::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  DoubleRegister value = ToDoubleRegister(value_input());

  __ AssertNotSmi(object);
  __ Move(FieldMemOperand(object, offset()), value);
}

void CheckedStoreSmiField::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
}
void CheckedStoreSmiField::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register value = ToRegister(value_input());

  Condition is_smi = __ CheckSmi(value);
  __ EmitEagerDeoptIf(NegateCondition(is_smi), DeoptimizeReason::kNotASmi,
                      this);
  __ StoreTaggedField(FieldMemOperand(object, offset()), value);
}

void StoreTaggedFieldNoWriteBarrier::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreTaggedFieldNoWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  __ StoreTaggedField(FieldMemOperand(object, offset()), value);
}

int StringAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return std::max(2, AllocateDescriptor::GetStackParameterCount());
}
void StringAt::SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void StringAt::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register result_string = ToRegister(result());
  Register string = ToRegister(string_input());
  Register index = ToRegister(index_input());
  Register char_code = string;

  ZoneLabelRef done(masm);
  Label cached_one_byte_string;

  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeOrCodePointAt(
      BuiltinStringPrototypeCharCodeOrCodePointAt::kCharCodeAt, save_registers,
      char_code, string, index, scratch, &cached_one_byte_string);
  __ StringFromCharCode(save_registers, &cached_one_byte_string, result_string,
                        char_code, scratch);
}

void StringLength::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void StringLength::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  __ StringLength(ToRegister(result()), ToRegister(object_input()));
}

void TaggedEqual::SetValueLocationConstraints() {
  UseRegister(lhs());
  UseRegister(rhs());
  DefineAsRegister(this);
}
void TaggedEqual::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Label done, if_equal;
  __ CmpTagged(ToRegister(lhs()), ToRegister(rhs()));
  __ JumpIf(kEqual, &if_equal, Label::Distance::kNear);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);
  __ Jump(&done);
  __ bind(&if_equal);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ bind(&done);
}

void TaggedNotEqual::SetValueLocationConstraints() {
  UseRegister(lhs());
  UseRegister(rhs());
  DefineAsRegister(this);
}
void TaggedNotEqual::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Label done, if_equal;
  __ CmpTagged(ToRegister(lhs()), ToRegister(rhs()));
  __ JumpIf(kEqual, &if_equal, Label::Distance::kNear);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ Jump(&done);
  __ bind(&if_equal);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);
  __ bind(&done);
}

int TestInstanceOf::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kInstanceOf_WithFeedback>::type;
  return D::GetStackParameterCount();
}
void TestInstanceOf::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kInstanceOf_WithFeedback>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object(), D::GetRegisterParameter(D::kLeft));
  UseFixed(callable(), D::GetRegisterParameter(D::kRight));
  DefineAsFixed(this, kReturnRegister0);
}
void TestInstanceOf::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kInstanceOf_WithFeedback>::type;
#ifdef DEBUG
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object()), D::GetRegisterParameter(D::kLeft));
  DCHECK_EQ(ToRegister(callable()), D::GetRegisterParameter(D::kRight));
#endif
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
  __ CallBuiltin(Builtin::kInstanceOf_WithFeedback);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void TestTypeOf::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}
void TestTypeOf::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register object = ToRegister(value());
  // Use return register as temporary if needed. Be careful: {object} and
  // {scratch} could alias (which means that {object} should be considered dead
  // once {scratch} has been written to).
  MaglevAssembler::ScratchRegisterScope temps(masm);
  temps.Include(ToRegister(result()));

  Label is_true, is_false, done;
  __ TestTypeOf(object, literal_, &is_true, Label::Distance::kNear, true,
                &is_false, Label::Distance::kNear, false);
  // Fallthrough into true.
  __ bind(&is_true);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ Jump(&done, Label::Distance::kNear);
  __ bind(&is_false);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);
  __ bind(&done);
}

void ToBoolean::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}
void ToBoolean::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());
  Label done;
  ZoneLabelRef object_is_true(masm), object_is_false(masm);
  // TODO(leszeks): We're likely to be calling this on an existing boolean --
  // maybe that's a case we should fast-path here and re-use that boolean value?
  __ ToBoolean(object, object_is_true, object_is_false, true);
  __ bind(*object_is_true);
  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ Jump(&done);
  __ bind(*object_is_false);
  __ LoadRoot(return_value, RootIndex::kFalseValue);
  __ bind(&done);
}

void ToBooleanLogicalNot::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}
void ToBooleanLogicalNot::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());
  Label done;
  ZoneLabelRef object_is_true(masm), object_is_false(masm);
  __ ToBoolean(object, object_is_true, object_is_false, true);
  __ bind(*object_is_true);
  __ LoadRoot(return_value, RootIndex::kFalseValue);
  __ Jump(&done);
  __ bind(*object_is_false);
  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ bind(&done);
}

int ToName::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToName>::type;
  return D::GetStackParameterCount();
}
void ToName::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToName>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(this, kReturnRegister0);
}
void ToName::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kToName>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kInput));
#endif  // DEBUG
  __ CallBuiltin(Builtin::kToName);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int ToNumberOrNumeric::MaxCallStackArgs() const {
  return TypeConversionDescriptor::GetStackParameterCount();
}
void ToNumberOrNumeric::SetValueLocationConstraints() {
  using D = TypeConversionDescriptor;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kArgument));
  DefineAsFixed(this, kReturnRegister0);
}
void ToNumberOrNumeric::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  switch (mode()) {
    case Object::Conversion::kToNumber:
      __ CallBuiltin(Builtin::kToNumber);
      break;
    case Object::Conversion::kToNumeric:
      __ CallBuiltin(Builtin::kToNumeric);
      break;
  }
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int ToObject::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  return D::GetStackParameterCount();
}
void ToObject::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(this, kReturnRegister0);
}
void ToObject::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kInput));
#endif  // DEBUG
  Register value = ToRegister(value_input());
  Label call_builtin, done;
  // Avoid the builtin call if {value} is a JSReceiver.
  __ JumpIfSmi(value, &call_builtin, Label::Distance::kNear);
  __ JumpIfJSAnyIsNotPrimitive(value, &done, Label::Distance::kNear);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToObject);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

int ToString::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  return D::GetStackParameterCount();
}
void ToString::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kO));
  DefineAsFixed(this, kReturnRegister0);
}
void ToString::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kO));
#endif  // DEBUG
  Register value = ToRegister(value_input());
  Label call_builtin, done;
  // Avoid the builtin call if {value} is a string.
  __ JumpIfSmi(value, &call_builtin, Label::Distance::kNear);
  __ CompareObjectType(value, FIRST_NONSTRING_TYPE);
  __ JumpIf(kUnsignedLessThan, &done, Label::Distance::kNear);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToString);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

int ThrowReferenceErrorIfHole::MaxCallStackArgs() const { return 1; }
void ThrowReferenceErrorIfHole::SetValueLocationConstraints() {
  UseAny(value());
}
void ThrowReferenceErrorIfHole::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  __ JumpToDeferredIf(
      __ IsRootConstant(value(), RootIndex::kTheHoleValue),
      [](MaglevAssembler* masm, ThrowReferenceErrorIfHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ Push(node->name().object());
        __ CallRuntime(Runtime::kThrowAccessedUninitializedVariable, 1);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

int ThrowSuperNotCalledIfHole::MaxCallStackArgs() const { return 0; }
void ThrowSuperNotCalledIfHole::SetValueLocationConstraints() {
  UseAny(value());
}
void ThrowSuperNotCalledIfHole::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  __ JumpToDeferredIf(
      __ IsRootConstant(value(), RootIndex::kTheHoleValue),
      [](MaglevAssembler* masm, ThrowSuperNotCalledIfHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowSuperNotCalled, 0);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

int ThrowSuperAlreadyCalledIfNotHole::MaxCallStackArgs() const { return 0; }
void ThrowSuperAlreadyCalledIfNotHole::SetValueLocationConstraints() {
  UseAny(value());
}
void ThrowSuperAlreadyCalledIfNotHole::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  __ JumpToDeferredIf(
      NegateCondition(__ IsRootConstant(value(), RootIndex::kTheHoleValue)),
      [](MaglevAssembler* masm, ThrowSuperAlreadyCalledIfNotHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowSuperAlreadyCalledError, 0);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

void TruncateUint32ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void TruncateUint32ToInt32::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  // No code emitted -- as far as the machine is concerned, int32 is uint32.
  DCHECK_EQ(ToRegister(input()), ToRegister(result()));
}

void TruncateFloat64ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void TruncateFloat64ToInt32::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  __ TruncateDoubleToInt32(ToRegister(result()), ToDoubleRegister(input()));
}

void CheckedTruncateFloat64ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToInt32::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  __ TryTruncateDoubleToInt32(
      ToRegister(result()), ToDoubleRegister(input()),
      __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32));
}

void UnsafeTruncateFloat64ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void UnsafeTruncateFloat64ToInt32::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
#ifdef DEBUG
  Label fail, start;
  __ Jump(&start);
  __ bind(&fail);
  __ Abort(AbortReason::kFloat64IsNotAInt32);

  __ bind(&start);
  __ TryTruncateDoubleToInt32(ToRegister(result()), ToDoubleRegister(input()),
                              &fail);
#else
  // TODO(dmercadier): TruncateDoubleToInt32 does additional work when the
  // double doesn't fit in a 32-bit integer. This is not necessary for
  // UnsafeTruncateFloat64ToInt32 (since we statically know that it the double
  // fits in a 32-bit int) and could be instead just a Cvttsd2si (x64) or Fcvtzs
  // (arm64).
  __ TruncateDoubleToInt32(ToRegister(result()), ToDoubleRegister(input()));
#endif
}

void CheckedUint32ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedUint32ToInt32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(input());
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32);
  __ CompareInt32AndJumpIf(input_reg, 0, kLessThan, fail);
}

void UnsafeTruncateUint32ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void UnsafeTruncateUint32ToInt32::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
#ifdef DEBUG
  Register input_reg = ToRegister(input());
  Label success;
  __ CompareInt32AndJumpIf(input_reg, 0, kGreaterThanEqual, &success);
  __ Abort(AbortReason::kUint32IsNotAInt32);
  __ bind(&success);
#endif
  // No code emitted -- as far as the machine is concerned, int32 is uint32.
  DCHECK_EQ(ToRegister(input()), ToRegister(result()));
}

void Int32ToUint8Clamped::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void Int32ToUint8Clamped::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register value = ToRegister(input());
  Register result_reg = ToRegister(result());
  DCHECK_EQ(value, result_reg);
  Label min, done;
  __ CompareInt32(value, 0);
  __ JumpIf(kLessThanEqual, &min);
  __ CompareInt32(value, 255);
  __ JumpIf(kLessThanEqual, &done);
  __ Move(result_reg, 255);
  __ Jump(&done, Label::Distance::kNear);
  __ bind(&min);
  __ Move(result_reg, 0);
  __ bind(&done);
}

void Uint32ToUint8Clamped::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void Uint32ToUint8Clamped::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register value = ToRegister(input());
  DCHECK_EQ(value, ToRegister(result()));
  Label done;
  __ CompareInt32(value, 255);
  __ JumpIf(kUnsignedLessThanEqual, &done, Label::Distance::kNear);
  __ Move(value, 255);
  __ bind(&done);
}

void Float64ToUint8Clamped::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64ToUint8Clamped::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  Register result_reg = ToRegister(result());
  Label min, max, done;
  __ ToUint8Clamped(result_reg, value, &min, &max, &done);
  __ bind(&min);
  __ Move(result_reg, 0);
  __ Jump(&done, Label::Distance::kNear);
  __ bind(&max);
  __ Move(result_reg, 255);
  __ bind(&done);
}

void CheckedNumberToUint8Clamped::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
  set_temporaries_needed(1);
  set_double_temporaries_needed(1);
}
void CheckedNumberToUint8Clamped::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register value = ToRegister(input());
  Register result_reg = ToRegister(result());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  DoubleRegister double_value = temps.AcquireDouble();
  Label is_not_smi, min, max, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi);
  // If Smi, convert to Int32.
  __ SmiToInt32(value);
  // Clamp.
  __ CompareInt32(value, 0);
  __ JumpIf(kLessThanEqual, &min);
  __ CompareInt32(value, 255);
  __ JumpIf(kGreaterThanEqual, &max);
  __ Jump(&done);
  __ bind(&is_not_smi);
  // Check if HeapNumber, deopt otherwise.
  __ CompareMapWithRoot(value, RootIndex::kHeapNumberMap, scratch);
  __ EmitEagerDeoptIf(kNotEqual, DeoptimizeReason::kNotANumber, this);
  // If heap number, get double value.
  __ LoadHeapNumberValue(double_value, value);
  // Clamp.
  __ ToUint8Clamped(value, double_value, &min, &max, &done);
  __ bind(&min);
  __ Move(result_reg, 0);
  __ Jump(&done, Label::Distance::kNear);
  __ bind(&max);
  __ Move(result_reg, 255);
  __ bind(&done);
}

void StoreFixedArrayElementWithWriteBarrier::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(index_input());
  UseRegister(value_input());
  RequireSpecificTemporary(WriteBarrierDescriptor::ObjectRegister());
  RequireSpecificTemporary(WriteBarrierDescriptor::SlotAddressRegister());
}
void StoreFixedArrayElementWithWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register elements = ToRegister(elements_input());
  Register index = ToRegister(index_input());
  Register value = ToRegister(value_input());
  __ StoreFixedArrayElementWithWriteBarrier(elements, index, value,
                                            register_snapshot());
}

void StoreFixedArrayElementNoWriteBarrier::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(index_input());
  UseRegister(value_input());
}
void StoreFixedArrayElementNoWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register elements = ToRegister(elements_input());
  Register index = ToRegister(index_input());
  Register value = ToRegister(value_input());
  __ StoreFixedArrayElementNoWriteBarrier(elements, index, value);
}

void CheckedStoreFixedArraySmiElement::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(index_input());
  UseRegister(value_input());
}
void CheckedStoreFixedArraySmiElement::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register elements = ToRegister(elements_input());
  Register index = ToRegister(index_input());
  Register value = ToRegister(value_input());
  Condition is_smi = __ CheckSmi(value);
  __ EmitEagerDeoptIf(NegateCondition(is_smi), DeoptimizeReason::kNotASmi,
                      this);
  __ StoreFixedArrayElementNoWriteBarrier(elements, index, value);
}

// ---
// Arch agnostic call nodes
// ---

int Call::MaxCallStackArgs() const { return num_args(); }
void Call::SetValueLocationConstraints() {
  // TODO(leszeks): Consider splitting Call into with- and without-feedback
  // opcodes, rather than checking for feedback validity.
  if (feedback_.IsValid()) {
    using D = CallTrampoline_WithFeedbackDescriptor;
    UseFixed(function(), D::GetRegisterParameter(D::kFunction));
    UseFixed(arg(0), D::GetRegisterParameter(D::kReceiver));
  } else {
    using D = CallTrampolineDescriptor;
    UseFixed(function(), D::GetRegisterParameter(D::kFunction));
    UseAny(arg(0));
  }
  for (int i = 1; i < num_args(); i++) {
    UseAny(arg(i));
  }
  UseFixed(context(), kContextRegister);
  DefineAsFixed(this, kReturnRegister0);
}

void Call::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  // TODO(leszeks): Port the nice Sparkplug CallBuiltin helper.
#ifdef DEBUG
  if (feedback_.IsValid()) {
    using D = CallTrampoline_WithFeedbackDescriptor;
    DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kFunction));
    DCHECK_EQ(ToRegister(arg(0)), D::GetRegisterParameter(D::kReceiver));
  } else {
    using D = CallTrampolineDescriptor;
    DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kFunction));
  }
#endif
  DCHECK_EQ(ToRegister(context()), kContextRegister);

  __ PushReverse(base::make_iterator_range(args_begin(), args_end()));

  uint32_t arg_count = num_args();
  if (feedback_.IsValid()) {
    DCHECK_EQ(TargetType::kAny, target_type_);
    using D = CallTrampoline_WithFeedbackDescriptor;
    __ Move(D::GetRegisterParameter(D::kActualArgumentsCount), arg_count);
    __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
    __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());

    switch (receiver_mode_) {
      case ConvertReceiverMode::kNullOrUndefined:
        __ CallBuiltin(Builtin::kCall_ReceiverIsNullOrUndefined_WithFeedback);
        break;
      case ConvertReceiverMode::kNotNullOrUndefined:
        __ CallBuiltin(
            Builtin::kCall_ReceiverIsNotNullOrUndefined_WithFeedback);
        break;
      case ConvertReceiverMode::kAny:
        __ CallBuiltin(Builtin::kCall_ReceiverIsAny_WithFeedback);
        break;
    }
  } else if (target_type_ == TargetType::kAny) {
    using D = CallTrampolineDescriptor;
    __ Move(D::GetRegisterParameter(D::kActualArgumentsCount), arg_count);

    switch (receiver_mode_) {
      case ConvertReceiverMode::kNullOrUndefined:
        __ CallBuiltin(Builtin::kCall_ReceiverIsNullOrUndefined);
        break;
      case ConvertReceiverMode::kNotNullOrUndefined:
        __ CallBuiltin(Builtin::kCall_ReceiverIsNotNullOrUndefined);
        break;
      case ConvertReceiverMode::kAny:
        __ CallBuiltin(Builtin::kCall_ReceiverIsAny);
        break;
    }
  } else {
    DCHECK_EQ(TargetType::kJSFunction, target_type_);
    using D = CallTrampolineDescriptor;
    __ Move(D::GetRegisterParameter(D::kActualArgumentsCount), arg_count);

    switch (receiver_mode_) {
      case ConvertReceiverMode::kNullOrUndefined:
        __ CallBuiltin(Builtin::kCallFunction_ReceiverIsNullOrUndefined);
        break;
      case ConvertReceiverMode::kNotNullOrUndefined:
        __ CallBuiltin(Builtin::kCallFunction_ReceiverIsNotNullOrUndefined);
        break;
      case ConvertReceiverMode::kAny:
        __ CallBuiltin(Builtin::kCallFunction_ReceiverIsAny);
        break;
    }
  }

  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallSelf::MaxCallStackArgs() const {
  int actual_parameter_count = num_args() + 1;
  return std::max(expected_parameter_count_, actual_parameter_count);
}
void CallSelf::SetValueLocationConstraints() {
  UseAny(receiver());
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
  set_temporaries_needed(1);
}

void CallSelf::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  int actual_parameter_count = num_args() + 1;
  if (actual_parameter_count < expected_parameter_count_) {
    int number_of_undefineds =
        expected_parameter_count_ - actual_parameter_count;
    __ LoadRoot(scratch, RootIndex::kUndefinedValue);
    __ PushReverse(receiver(),
                   base::make_iterator_range(args_begin(), args_end()),
                   RepeatValue(scratch, number_of_undefineds));
  } else {
    __ PushReverse(receiver(),
                   base::make_iterator_range(args_begin(), args_end()));
  }
  compiler::JSHeapBroker* broker = masm->compilation_info()->broker();
  __ Move(kContextRegister, function_.context(broker).object());
  __ Move(kJavaScriptCallTargetRegister, function_.object());
  __ LoadRoot(kJavaScriptCallNewTargetRegister, RootIndex::kUndefinedValue);
  __ Move(kJavaScriptCallArgCountRegister, actual_parameter_count);
  DCHECK(!shared_function_info(broker).HasBuiltinId());
  __ CallSelf();
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallKnownJSFunction::MaxCallStackArgs() const {
  int actual_parameter_count = num_args() + 1;
  return std::max(expected_parameter_count_, actual_parameter_count);
}
void CallKnownJSFunction::SetValueLocationConstraints() {
  UseAny(receiver());
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
  set_temporaries_needed(1);
}

void CallKnownJSFunction::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  int actual_parameter_count = num_args() + 1;
  if (actual_parameter_count < expected_parameter_count_) {
    int number_of_undefineds =
        expected_parameter_count_ - actual_parameter_count;
    __ LoadRoot(scratch, RootIndex::kUndefinedValue);
    __ PushReverse(receiver(),
                   base::make_iterator_range(args_begin(), args_end()),
                   RepeatValue(scratch, number_of_undefineds));
  } else {
    __ PushReverse(receiver(),
                   base::make_iterator_range(args_begin(), args_end()));
  }
  compiler::JSHeapBroker* broker = masm->compilation_info()->broker();
  __ Move(kContextRegister, function_.context(broker).object());
  __ Move(kJavaScriptCallTargetRegister, function_.object());
  __ LoadRoot(kJavaScriptCallNewTargetRegister, RootIndex::kUndefinedValue);
  __ Move(kJavaScriptCallArgCountRegister, actual_parameter_count);
  if (shared_function_info(broker).HasBuiltinId()) {
    __ CallBuiltin(shared_function_info(broker).builtin_id());
  } else {
    __ AssertCallableFunction(kJavaScriptCallTargetRegister);
    __ LoadTaggedField(kJavaScriptCallCodeStartRegister,
                       FieldMemOperand(kJavaScriptCallTargetRegister,
                                       JSFunction::kCodeOffset));
    __ CallCodeObject(kJavaScriptCallCodeStartRegister);
  }
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallBuiltin::MaxCallStackArgs() const {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  if (!descriptor.AllowVarArgs()) {
    return descriptor.GetStackParameterCount();
  } else {
    int all_input_count = InputCountWithoutContext() + (has_feedback() ? 2 : 0);
    DCHECK_GE(all_input_count, descriptor.GetRegisterParameterCount());
    return all_input_count - descriptor.GetRegisterParameterCount();
  }
}

void CallBuiltin::SetValueLocationConstraints() {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  bool has_context = descriptor.HasContextParameter();
  int i = 0;
  for (; i < InputsInRegisterCount(); i++) {
    UseFixed(input(i), descriptor.GetRegisterParameter(i));
  }
  for (; i < InputCountWithoutContext(); i++) {
    UseAny(input(i));
  }
  if (has_context) {
    UseFixed(input(i), kContextRegister);
  }
  DefineAsFixed(this, kReturnRegister0);
}

template <typename... Args>
void CallBuiltin::PushArguments(MaglevAssembler* masm, Args... extra_args) {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  if (descriptor.GetStackArgumentOrder() == StackArgumentOrder::kDefault) {
    // In Default order we cannot have extra args (feedback).
    DCHECK_EQ(sizeof...(extra_args), 0);
    __ Push(base::make_iterator_range(stack_args_begin(), stack_args_end()));
  } else {
    DCHECK_EQ(descriptor.GetStackArgumentOrder(), StackArgumentOrder::kJS);
    __ PushReverse(extra_args..., base::make_iterator_range(stack_args_begin(),
                                                            stack_args_end()));
  }
}

void CallBuiltin::PassFeedbackSlotInRegister(MaglevAssembler* masm) {
  DCHECK(has_feedback());
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  int slot_index = InputCountWithoutContext();
  switch (slot_type()) {
    case kTaggedIndex:
      __ Move(descriptor.GetRegisterParameter(slot_index),
              TaggedIndex::FromIntptr(feedback().index()));
      break;
    case kSmi:
      __ Move(descriptor.GetRegisterParameter(slot_index),
              Smi::FromInt(feedback().index()));
      break;
  }
}

void CallBuiltin::PushFeedbackAndArguments(MaglevAssembler* masm) {
  DCHECK(has_feedback());

  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  int slot_index = InputCountWithoutContext();
  int vector_index = slot_index + 1;

  // There are three possibilities:
  // 1. Feedback slot and vector are in register.
  // 2. Feedback slot is in register and vector is on stack.
  // 3. Feedback slot and vector are on stack.
  if (vector_index < descriptor.GetRegisterParameterCount()) {
    PassFeedbackSlotInRegister(masm);
    __ Move(descriptor.GetRegisterParameter(vector_index), feedback().vector);
    PushArguments(masm);
  } else if (vector_index == descriptor.GetRegisterParameterCount()) {
    PassFeedbackSlotInRegister(masm);
    DCHECK_EQ(descriptor.GetStackArgumentOrder(), StackArgumentOrder::kJS);
    // Ensure that the builtin only expects the feedback vector on the stack and
    // potentional additional var args are passed through to another builtin.
    // This is required to align the stack correctly (e.g. on arm64).
    DCHECK_EQ(descriptor.GetStackParameterCount(), 1);
    PushArguments(masm);
    __ Push(feedback().vector);
  } else {
    int slot = feedback().index();
    Handle<FeedbackVector> vector = feedback().vector;
    switch (slot_type()) {
      case kTaggedIndex:
        PushArguments(masm, TaggedIndex::FromIntptr(slot), vector);
        break;
      case kSmi:
        PushArguments(masm, Smi::FromInt(slot), vector);
        break;
    }
  }
}

void CallBuiltin::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  if (has_feedback()) {
    PushFeedbackAndArguments(masm);
  } else {
    PushArguments(masm);
  }
  __ CallBuiltin(builtin());
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallRuntime::MaxCallStackArgs() const { return num_args(); }
void CallRuntime::SetValueLocationConstraints() {
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
}
void CallRuntime::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  __ Push(base::make_iterator_range(args_begin(), args_end()));
  __ CallRuntime(function_id(), num_args());
  // TODO(victorgomes): Not sure if this is needed for all runtime calls.
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallWithSpread::MaxCallStackArgs() const {
  int argc_no_spread = num_args() - 1;
  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    return argc_no_spread + D::GetStackParameterCount();
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    return argc_no_spread + D::GetStackParameterCount();
  }
}
void CallWithSpread::SetValueLocationConstraints() {
  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    UseFixed(function(), D::GetRegisterParameter(D::kTarget));
    UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    UseFixed(function(), D::GetRegisterParameter(D::kTarget));
    UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  }
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args() - 1; i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
}
void CallWithSpread::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
#ifdef DEBUG
  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
    DCHECK_EQ(ToRegister(spread()), D::GetRegisterParameter(D::kSpread));
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
    DCHECK_EQ(ToRegister(spread()), D::GetRegisterParameter(D::kSpread));
  }
  DCHECK_EQ(ToRegister(context()), kContextRegister);
#endif

  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    __ PushReverse(base::make_iterator_range(args_no_spread_begin(),
                                             args_no_spread_end()));
    // Receiver needs to be pushed (aligned) separately as it is consumed by
    // CallWithSpread_WithFeedback directly while the other arguments on the
    // stack are passed through to CallWithSpread.
    static_assert(D::GetStackParameterIndex(D::kReceiver) == 0);
    static_assert(D::GetStackParameterCount() == 1);
    __ Push(receiver());

    __ Move(D::GetRegisterParameter(D::kArgumentsCount), num_args_no_spread());
    __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
    __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
    __ CallBuiltin(Builtin::kCallWithSpread_WithFeedback);
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    __ PushReverse(base::make_iterator_range(args_no_spread_begin(),
                                             args_no_spread_end()));
    __ Move(D::GetRegisterParameter(D::kArgumentsCount), num_args_no_spread());
    __ CallBuiltin(Builtin::kCallWithSpread);
  }

  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallWithArrayLike::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithArrayLike>::type;
  return D::GetStackParameterCount();
}
void CallWithArrayLike::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithArrayLike>::type;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseAny(receiver());
  UseFixed(arguments_list(), D::GetRegisterParameter(D::kArgumentsList));
  UseFixed(context(), kContextRegister);
  DefineAsFixed(this, kReturnRegister0);
}
void CallWithArrayLike::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithArrayLike>::type;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(arguments_list()),
            D::GetRegisterParameter(D::kArgumentsList));
  DCHECK_EQ(ToRegister(context()), kContextRegister);
#endif  // DEBUG
  __ Push(receiver());
  __ CallBuiltin(Builtin::kCallWithArrayLike);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

// ---
// Arch agnostic construct nodes
// ---

int Construct::MaxCallStackArgs() const {
  using D = Construct_WithFeedbackDescriptor;
  return num_args() + D::GetStackParameterCount();
}
void Construct::SetValueLocationConstraints() {
  using D = Construct_WithFeedbackDescriptor;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseFixed(new_target(), D::GetRegisterParameter(D::kNewTarget));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
}
void Construct::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  using D = Construct_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(new_target()), D::GetRegisterParameter(D::kNewTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);

  __ PushReverse(base::make_iterator_range(args_begin(), args_end()));
  // Feedback needs to be pushed (aligned) separately as it is consumed by
  // Construct_WithFeedback directly while the other arguments on the stack
  // are passed through to Construct.
  static_assert(D::GetStackParameterIndex(D::kFeedbackVector) == 0);
  static_assert(D::GetStackParameterCount() == 1);
  __ Push(feedback().vector);

  __ Move(D::GetRegisterParameter(D::kActualArgumentsCount), num_args());
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());

  __ CallBuiltin(Builtin::kConstruct_WithFeedback);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int ConstructWithSpread::MaxCallStackArgs() const {
  int argc_no_spread = num_args() - 1;
  using D = CallInterfaceDescriptorFor<
      Builtin::kConstructWithSpread_WithFeedback>::type;
  return argc_no_spread + D::GetStackParameterCount();
}
void ConstructWithSpread::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<
      Builtin::kConstructWithSpread_WithFeedback>::type;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseFixed(new_target(), D::GetRegisterParameter(D::kNewTarget));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args() - 1; i++) {
    UseAny(arg(i));
  }
  UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  DefineAsFixed(this, kReturnRegister0);
}
void ConstructWithSpread::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<
      Builtin::kConstructWithSpread_WithFeedback>::type;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(new_target()), D::GetRegisterParameter(D::kNewTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  __ PushReverse(
      base::make_iterator_range(args_no_spread_begin(), args_no_spread_end()));

  // Feedback needs to be pushed (aligned) separately as it is consumed by
  // Construct_WithFeedback directly while the other arguments on the stack
  // are passed through to Construct.
  static_assert(D::GetStackParameterIndex(D::kFeedbackVector) == 0);
  static_assert(D::GetStackParameterCount() == 1);
  __ Push(feedback().vector);

  __ Move(D::GetRegisterParameter(D::kActualArgumentsCount),
          num_args_no_spread());
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
  __ CallBuiltin(Builtin::kConstructWithSpread_WithFeedback);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int TransitionElementsKind::MaxCallStackArgs() const {
  return std::max(WriteBarrierDescriptor::GetStackParameterCount(), 2);
}
void TransitionElementsKind::SetValueLocationConstraints() {
  UseRegister(object_input());
  set_temporaries_needed(1);
}
void TransitionElementsKind::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register object = ToRegister(object_input());

  ZoneLabelRef done(masm);

  // TODO(leszeks): We could consider just deopting straight away if the object
  // is a Smi, but it's also fine to not deopt since there will be another Smi
  // check in the map check after this transition. This second Smi check could
  // be elided if we deopted here, but OTOH then we'd need extra deopt info for
  // this node.
  __ JumpIfSmi(object, *done, Label::kNear);

  Register map = temps.Acquire();
  __ LoadMap(map, object);

  for (compiler::MapRef transition_source : transition_sources_) {
    bool is_simple = IsSimpleMapChangeTransition(
        transition_source.elements_kind(), transition_target_.elements_kind());

    // TODO(leszeks): If there are a lot of transition source maps, move the
    // source into a register and share the deferred code between maps.
    __ CompareTagged(map, transition_source.object());
    __ JumpToDeferredIf(
        kEqual,
        [](MaglevAssembler* masm, Register object, Register map,
           RegisterSnapshot register_snapshot,
           compiler::MapRef transition_target, bool is_simple,
           ZoneLabelRef done) {
          if (is_simple) {
            __ Move(map, transition_target.object());
            __ StoreTaggedFieldWithWriteBarrier(
                object, HeapObject::kMapOffset, map, register_snapshot,
                MaglevAssembler::kValueIsDecompressed,
                MaglevAssembler::kValueCannotBeSmi);
          } else {
            SaveRegisterStateForCall save_state(masm, register_snapshot);
            __ Push(object, transition_target.object());
            __ Move(kContextRegister, masm->native_context().object());
            __ CallRuntime(Runtime::kTransitionElementsKind);
            save_state.DefineSafepoint();
          }
          __ Jump(*done);
        },
        object, map, register_snapshot(), transition_target_, is_simple, done);
  }
  __ bind(*done);
}

// ---
// Arch agnostic control nodes
// ---

void Jump::SetValueLocationConstraints() {}
void Jump::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ Jump(target()->label());
  }
}

void JumpToInlined::SetValueLocationConstraints() {}
void JumpToInlined::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ Jump(target()->label());
  }
}
void JumpFromInlined::SetValueLocationConstraints() {}
void JumpFromInlined::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ Jump(target()->label());
  }
}

namespace {

void AttemptOnStackReplacement(MaglevAssembler* masm,
                               ZoneLabelRef no_code_for_osr,
                               JumpLoopPrologue* node, Register scratch0,
                               Register scratch1, int32_t loop_depth,
                               FeedbackSlot feedback_slot,
                               BytecodeOffset osr_offset) {
  // Two cases may cause us to attempt OSR, in the following order:
  //
  // 1) Presence of cached OSR Turbofan code.
  // 2) The OSR urgency exceeds the current loop depth - in that case, call
  //    into runtime to trigger a Turbofan OSR compilation. A non-zero return
  //    value means we should deopt into Ignition which will handle all further
  //    necessary steps (rewriting the stack frame, jumping to OSR'd code).
  //
  // See also: InterpreterAssembler::OnStackReplacement.

  __ AssertFeedbackVector(scratch0);

  // Case 1).
  Label deopt;
  Register maybe_target_code = scratch1;
  __ TryLoadOptimizedOsrCode(maybe_target_code, scratch0, feedback_slot, &deopt,
                             Label::kFar);

  // Case 2).
  {
    __ LoadByte(scratch0,
                FieldMemOperand(scratch0, FeedbackVector::kOsrStateOffset));
    __ DecodeField<FeedbackVector::OsrUrgencyBits>(scratch0);
    __ JumpIfByte(kUnsignedLessThanEqual, scratch0, loop_depth,
                  *no_code_for_osr, Label::Distance::kNear);

    // The osr_urgency exceeds the current loop_depth, signaling an OSR
    // request. Call into runtime to compile.
    {
      // At this point we need a custom register snapshot since additional
      // registers may be live at the eager deopt below (the normal
      // register_snapshot only contains live registers *after this
      // node*).
      // TODO(v8:7700): Consider making the snapshot location
      // configurable.
      RegisterSnapshot snapshot = node->register_snapshot();
      AddDeoptRegistersToSnapshot(&snapshot, node->eager_deopt_info());
      DCHECK(!snapshot.live_registers.has(maybe_target_code));
      SaveRegisterStateForCall save_register_state(masm, snapshot);
      __ Move(kContextRegister, masm->native_context().object());
      __ Push(Smi::FromInt(osr_offset.ToInt()));
      __ CallRuntime(Runtime::kCompileOptimizedOSRFromMaglev, 1);
      save_register_state.DefineSafepoint();
      __ Move(maybe_target_code, kReturnRegister0);
    }

    // A `0` return value means there is no OSR code available yet. Continue
    // execution in Maglev, OSR code will be picked up once it exists and is
    // cached on the feedback vector.
    __ Cmp(maybe_target_code, 0);
    __ JumpIf(kEqual, *no_code_for_osr);
  }

  __ bind(&deopt);
  if (V8_LIKELY(v8_flags.turbofan)) {
    // None of the mutated input registers should be a register input into the
    // eager deopt info.
    DCHECK_REGLIST_EMPTY(
        RegList{scratch0, scratch1} &
        GetGeneralRegistersUsedAsInputs(node->eager_deopt_info()));
    __ EmitEagerDeopt(node, DeoptimizeReason::kPrepareForOnStackReplacement);
  } else {
    // Continue execution in Maglev. With TF disabled we cannot OSR and thus it
    // doesn't make sense to start the process. We do still perform all
    // remaining bookkeeping above though, to keep Maglev code behavior roughly
    // the same in both configurations.
    __ Jump(*no_code_for_osr);
  }
}

}  // namespace

int JumpLoopPrologue::MaxCallStackArgs() const {
  // For the kCompileOptimizedOSRFromMaglev call.
  return 1;
}
void JumpLoopPrologue::SetValueLocationConstraints() {
  if (!v8_flags.use_osr) return;
  set_temporaries_needed(2);
}
void JumpLoopPrologue::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  if (!v8_flags.use_osr) return;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch0 = temps.Acquire();
  Register scratch1 = temps.Acquire();

  const Register osr_state = scratch1;
  __ Move(scratch0, unit_->feedback().object());
  __ AssertFeedbackVector(scratch0);
  __ LoadByte(osr_state,
              FieldMemOperand(scratch0, FeedbackVector::kOsrStateOffset));

  // The quick initial OSR check. If it passes, we proceed on to more
  // expensive OSR logic.
  static_assert(FeedbackVector::MaybeHasOptimizedOsrCodeBit::encode(true) >
                FeedbackVector::kMaxOsrUrgency);
  __ CompareInt32(osr_state, loop_depth_);
  ZoneLabelRef no_code_for_osr(masm);
  __ JumpToDeferredIf(kUnsignedGreaterThan, AttemptOnStackReplacement,
                      no_code_for_osr, this, scratch0, scratch1, loop_depth_,
                      feedback_slot_, osr_offset_);
  __ bind(*no_code_for_osr);
}

void JumpLoop::SetValueLocationConstraints() {}
void JumpLoop::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  __ Jump(target()->label());
}

void BranchIfRootConstant::SetValueLocationConstraints() {
  UseRegister(condition_input());
}
void BranchIfRootConstant::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  __ CompareRoot(ToRegister(condition_input()), root_index());
  __ Branch(ConditionFor(Operation::kEqual), if_true(), if_false(),
            state.next_block());
}

void BranchIfToBooleanTrue::SetValueLocationConstraints() {
  // TODO(victorgomes): consider using any input instead.
  UseRegister(condition_input());
}
void BranchIfToBooleanTrue::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  // BasicBlocks are zone allocated and so safe to be casted to ZoneLabelRef.
  ZoneLabelRef true_label =
      ZoneLabelRef::UnsafeFromLabelPointer(if_true()->label());
  ZoneLabelRef false_label =
      ZoneLabelRef::UnsafeFromLabelPointer(if_false()->label());
  bool fallthrough_when_true = (if_true() == state.next_block());
  __ ToBoolean(ToRegister(condition_input()), true_label, false_label,
               fallthrough_when_true);
}

void BranchIfReferenceCompare::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfReferenceCompare::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ CmpTagged(left, right);
  __ Branch(ConditionFor(operation_), if_true(), if_false(),
            state.next_block());
}

void BranchIfInt32Compare::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfInt32Compare::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ CompareInt32(left, right);
  __ Branch(ConditionFor(operation_), if_true(), if_false(),
            state.next_block());
}

void BranchIfUndefinedOrNull::SetValueLocationConstraints() {
  UseRegister(condition_input());
}
void BranchIfUndefinedOrNull::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(condition_input());
  __ JumpIfRoot(value, RootIndex::kUndefinedValue, if_true()->label());
  __ JumpIfRoot(value, RootIndex::kNullValue, if_true()->label());
  auto* next_block = state.next_block();
  if (if_false() != next_block) {
    __ Jump(if_false()->label());
  }
}

void BranchIfTypeOf::SetValueLocationConstraints() {
  UseRegister(value_input());
  // One temporary for TestTypeOf.
  set_temporaries_needed(1);
}
void BranchIfTypeOf::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ TestTypeOf(value, literal_, if_true()->label(), Label::kFar,
                if_true() == state.next_block(), if_false()->label(),
                Label::kFar, if_false() == state.next_block());
}

void BranchIfJSReceiver::SetValueLocationConstraints() {
  UseRegister(condition_input());
}
void BranchIfJSReceiver::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register value = ToRegister(condition_input());
  __ JumpIfSmi(value, if_false()->label());
  __ JumpIfJSAnyIsNotPrimitive(value, if_true()->label());
  __ jmp(if_false()->label());
}

void Switch::SetValueLocationConstraints() {
  UseAndClobberRegister(value());
  // TODO(victorgomes): Create a arch-agnostic scratch register scope.
  set_temporaries_needed(1);
}
void Switch::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  std::unique_ptr<Label*[]> labels = std::make_unique<Label*[]>(size());
  for (int i = 0; i < size(); i++) {
    BasicBlock* block = (targets())[i].block_ptr();
    block->set_start_block_of_switch_case(true);
    labels[i] = block->label();
  }
  Register val = ToRegister(value());
  // Switch requires {val} (the switch's condition) to be 64-bit, but maglev
  // usually manipulates/creates 32-bit integers. We thus sign-extend {val} to
  // 64-bit to have the correct value for negative numbers.
  __ SignExtend32To64Bits(val, val);
  __ Switch(scratch, val, value_base(), labels.get(), size());
  if (has_fallthrough()) {
    DCHECK_EQ(fallthrough(), state.next_block());
  } else {
    __ Trap();
  }
}

// ---
// Print params
// ---

void ExternalConstant::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(" << reference() << ")";
}

void SmiConstant::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Int32Constant::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Float64Constant::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  if (value().is_nan()) {
    os << "(NaN [0x" << std::hex << value().get_bits() << std::dec << "]";
    if (value().is_hole_nan()) {
      os << ", the hole";
    } else if (value().get_bits() ==
               base::bit_cast<uint64_t>(
                   std::numeric_limits<double>::quiet_NaN())) {
      os << ", quiet NaN";
    }
    os << ")";

  } else {
    os << "(" << value().get_scalar() << ")";
  }
}

void Constant::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {
  os << "(" << object_ << ")";
}

void DeleteProperty::PrintParams(std::ostream& os,
                                 MaglevGraphLabeller* graph_labeller) const {
  os << "(" << LanguageMode2String(mode()) << ")";
}

void InitialValue::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  os << "(" << source().ToString() << ")";
}

void LoadGlobal::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name() << ")";
}

void StoreGlobal::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name() << ")";
}

void RegisterInput::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << input() << ")";
}

void RootConstant::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  os << "(" << RootsTable::name(index()) << ")";
}

void CreateFunctionContext::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *scope_info().object() << ", " << slot_count() << ")";
}

void FastCreateClosure::PrintParams(std::ostream& os,
                                    MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *shared_function_info().object() << ", "
     << feedback_cell().object() << ")";
}

void CreateClosure::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *shared_function_info().object() << ", "
     << feedback_cell().object();
  if (pretenured()) {
    os << " [pretenured]";
  }
  os << ")";
}

void AllocateRaw::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << allocation_type() << ", " << size() << ")";
}

void FoldedAllocation::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(+" << offset() << ")";
}

void Abort::PrintParams(std::ostream& os,
                        MaglevGraphLabeller* graph_labeller) const {
  os << "(" << GetAbortReason(reason()) << ")";
}

void AssertInt32::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << condition_ << ")";
}

void BuiltinStringPrototypeCharCodeOrCodePointAt::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  switch (mode_) {
    case BuiltinStringPrototypeCharCodeOrCodePointAt::kCharCodeAt:
      os << "(CharCodeAt)";
      break;
    case BuiltinStringPrototypeCharCodeOrCodePointAt::kCodePointAt:
      os << "(CodePointAt)";
      break;
  }
}

void CheckMaps::PrintParams(std::ostream& os,
                            MaglevGraphLabeller* graph_labeller) const {
  os << "(";
  size_t map_count = maps().size();
  if (map_count > 0) {
    for (size_t i = 0; i < map_count - 1; ++i) {
      os << maps().at(i) << ", ";
    }
    os << maps().at(map_count - 1);
  }
  os << ")";
}

void CheckValue::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *value().object() << ")";
}

void CheckValueEqualsString::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *value().object() << ")";
}

void CheckInstanceType::PrintParams(std::ostream& os,
                                    MaglevGraphLabeller* graph_labeller) const {
  os << "(" << instance_type() << ")";
}

void CheckMapsWithMigration::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(";
  size_t map_count = maps().size();
  if (map_count > 0) {
    for (size_t i = 0; i < map_count - 1; ++i) {
      os << maps().at(i) << ", ";
    }
    os << maps().at(map_count - 1);
  }
  os << ")";
}

void CheckInt32Condition::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << condition_ << ")";
}

void CheckedNumberOrOddballToFloat64::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << conversion_type() << ")";
}

void UncheckedNumberOrOddballToFloat64::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << conversion_type() << ")";
}

void CheckedTruncateNumberOrOddballToInt32::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << conversion_type() << ")";
}

void TruncateNumberOrOddballToInt32::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << conversion_type() << ")";
}

void LoadTaggedField::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec;
  // Print compression status only after the result is allocated, since that's
  // when we do decompression marking.
  if (!result().operand().IsUnallocated()) {
    if (decompresses_tagged_result()) {
      os << ", decompressed";
    } else {
      os << ", compressed";
    }
  }
  os << ")";
}

void LoadDoubleField::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void LoadFixedArrayElement::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  // Print compression status only after the result is allocated, since that's
  // when we do decompression marking.
  if (!result().operand().IsUnallocated()) {
    if (decompresses_tagged_result()) {
      os << "(decompressed)";
    } else {
      os << "(compressed)";
    }
  }
}

void StoreDoubleField::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void StoreFloat64::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void CheckedStoreSmiField::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << offset() << std::dec << ")";
}

void StoreTaggedFieldNoWriteBarrier::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void StoreMap::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *map_.object() << ")";
}

void StoreTaggedFieldWithWriteBarrier::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void LoadNamedGeneric::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void LoadNamedFromSuperGeneric::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void SetNamedGeneric::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void DefineNamedOwnGeneric::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void GapMove::PrintParams(std::ostream& os,
                          MaglevGraphLabeller* graph_labeller) const {
  os << "(" << source() << " → " << target() << ")";
}

void ConstantGapMove::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(";
  graph_labeller->PrintNodeLabel(os, node_);
  os << " → " << target() << ")";
}

void Float64Ieee754Unary::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "("
     << ExternalReferenceTable::NameOfIsolateIndependentAddress(
            ieee_function_.address())
     << ")";
}

void Float64Round::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  switch (kind_) {
    case Kind::kCeil:
      os << "(ceil)";
      return;
    case Kind::kFloor:
      os << "(floor)";
      return;
    case Kind::kNearest:
      os << "(nearest)";
      return;
  }
}

void Phi::PrintParams(std::ostream& os,
                      MaglevGraphLabeller* graph_labeller) const {
  os << "(" << owner().ToString() << ")";
}

void Call::PrintParams(std::ostream& os,
                       MaglevGraphLabeller* graph_labeller) const {
  os << "(" << receiver_mode_ << ", ";
  switch (target_type_) {
    case TargetType::kJSFunction:
      os << "JSFunction";
      break;
    case TargetType::kAny:
      os << "Any";
      break;
  }
  os << ")";
}

void CallSelf::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {
  os << "(" << function_.object() << ")";
}

void CallKnownJSFunction::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << function_.object() << ")";
}

void CallBuiltin::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << Builtins::name(builtin()) << ")";
}

void CallRuntime::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << Runtime::FunctionForId(function_id())->name << ")";
}

void TestTypeOf::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << interpreter::TestTypeOfFlags::ToString(literal_) << ")";
}

void IncreaseInterruptBudget::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << amount() << ")";
}

void ReduceInterruptBudgetForLoop::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << amount() << ")";
}

void ReduceInterruptBudgetForReturn::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << amount() << ")";
}

void Deopt::PrintParams(std::ostream& os,
                        MaglevGraphLabeller* graph_labeller) const {
  os << "(" << DeoptimizeReasonToString(reason()) << ")";
}

void JumpToInlined::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << Brief(*unit()->shared_function_info().object()) << ")";
}

void BranchIfRootConstant::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << RootsTable::name(root_index_) << ")";
}

void BranchIfFloat64Compare::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << operation_ << ")";
}

void BranchIfInt32Compare::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << operation_ << ")";
}

void BranchIfReferenceCompare::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << operation_ << ")";
}

void BranchIfTypeOf::PrintParams(std::ostream& os,
                                 MaglevGraphLabeller* graph_labeller) const {
  os << "(" << interpreter::TestTypeOfFlags::ToString(literal_) << ")";
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
