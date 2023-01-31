// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-ir.h"

#include "src/baseline/baseline-assembler-inl.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir-inl.h"

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

bool FromConstantToBool(MaglevAssembler* masm, ValueNode* node) {
  DCHECK(IsConstantNode(node->opcode()));
  LocalIsolate* local_isolate = masm->isolate()->AsLocalIsolate();
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

DeoptInfo::DeoptInfo(Zone* zone, DeoptFrame top_frame,
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
  if (V8_LIKELY(result_size_ == 1)) {
    return reg == result_location_;
  }
  DCHECK_EQ(result_size_, 2);
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

void ValueNode::SetNoSpillOrHint() {
  DCHECK_EQ(state_, kLastUse);
  DCHECK(!IsConstantNode(opcode()));
#ifdef DEBUG
  state_ = kSpillOrHint;
#endif  // DEBUG
  spill_or_hint_ = compiler::InstructionOperand();
}

void ValueNode::SetConstantLocation() {
  DCHECK(IsConstantNode(opcode()));
#ifdef DEBUG
  state_ = kSpillOrHint;
#endif  // DEBUG
  spill_or_hint_ = compiler::ConstantOperand(
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

void CheckValueInputIsWord32(const NodeBase* node, int i,
                             MaglevGraphLabeller* graph_labeller) {
  ValueNode* input = node->input(i).node();
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
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void Call::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallWithArrayLike::VerifyInputs(
    MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallWithSpread::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void CallKnownJSFunction::VerifyInputs(
    MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void Construct::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
}

void ConstructWithSpread::VerifyInputs(
    MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
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

void CallRuntime::VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
  for (int i = 0; i < input_count(); i++) {
    CheckValueInputIs(this, i, ValueRepresentation::kTagged, graph_labeller);
  }
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
  return isolate->factory()->NewNumber<AllocationType::kOld>(value_);
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

void CheckUint32IsSmi::SetValueLocationConstraints() { UseRegister(input()); }
void CheckUint32IsSmi::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register reg = ToRegister(input());
  // Perform an unsigned comparison against Smi::kMaxValue.
  __ Cmp(reg, Smi::kMaxValue);
  __ EmitEagerDeoptIf(ToCondition(AssertCondition::kAbove),
                      DeoptimizeReason::kNotASmi, this);
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

void LoadTaggedField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void LoadTaggedField::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressAnyTagged(ToRegister(result()),
                         FieldMemOperand(object, offset()));
}

void LoadPolymorphicTaggedField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void LoadPolymorphicTaggedField::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register result_reg = ToRegister(result());
  Register object = ToRegister(object_input());
  Register object_map = temps.Acquire();
  Label done;
  Label is_number;

  Condition is_smi = __ CheckSmi(object);
  __ JumpIf(is_smi, &is_number);
  __ LoadMap(object_map, object);
  for (compiler::PropertyAccessInfo access_info : access_infos_) {
    Label next;
    Label do_load;

    // Check if {object_map} one of the lookup_start_object_maps.
    bool has_heap_number_map = false;
    auto& maps = access_info.lookup_start_object_maps();
    for (auto it = maps.begin(); it != maps.end(); ++it) {
      if (it->IsHeapNumberMap()) {
        has_heap_number_map = true;
      }
      __ CompareTagged(object_map, it->object());
      if (it == maps.end() - 1) {
        __ JumpIfNotEqual(&next);
        // Fallthrough to {do_load}.
      } else {
        __ JumpIfEqual(&do_load);
        __ Jump(&next);
      }
    }

    // Bind number case here if one of the maps is HeapNumber.
    if (has_heap_number_map && !is_number.is_bound()) {
      __ bind(&is_number);
    }

    // Do the load based on the PropertyAccess kind.
    __ bind(&do_load);
    switch (access_info.kind()) {
      case compiler::PropertyAccessInfo::kNotFound:
        __ LoadRoot(result_reg, RootIndex::kUndefinedValue);
        break;
      case compiler::PropertyAccessInfo::kModuleExport: {
        Register cell = object_map;  // Reuse scratch.
        __ Move(cell, access_info.constant().value().AsCell().object());
        __ AssertNotSmi(cell);
        __ DecompressAnyTagged(result_reg,
                               FieldMemOperand(cell, Cell::kValueOffset));
        break;
      }
      case compiler::PropertyAccessInfo::kDataField:
      case compiler::PropertyAccessInfo::kFastDataConstant: {
        // TODO(victorgomes): Support ConstantDataFields.
        Register load_source = object;
        // Resolve property holder.
        if (access_info.holder().has_value()) {
          load_source = object_map;  // Reuse scratch.
          __ Move(load_source, access_info.holder().value().object());
        }
        FieldIndex field_index = access_info.field_index();
        if (!field_index.is_inobject()) {
          Register load_source_object = load_source;
          if (load_source == object) {
            load_source = object_map;  // Reuse scratch.
          }
          // The field is in the property array, first load it from there.
          __ AssertNotSmi(load_source_object);
          __ DecompressAnyTagged(
              load_source,
              FieldMemOperand(load_source_object,
                              JSReceiver::kPropertiesOrHashOffset));
        }
        __ AssertNotSmi(load_source);
        __ DecompressAnyTagged(
            result_reg, FieldMemOperand(load_source, field_index.offset()));

        break;
      }
      default:
        UNREACHABLE();
    }

    __ Jump(&done);
    __ bind(&next);
  }

  // A HeapNumberMap was not found, we should eager deopt here in case of a
  // number.
  if (!is_number.is_bound()) {
    __ bind(&is_number);
  }

  // No map matched!
  __ EmitEagerDeopt(this, DeoptimizeReason::kWrongMap);
  __ bind(&done);
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
  __ Cmp(target, value().object());
  __ EmitEagerDeoptIfNotEqual(DeoptimizeReason::kWrongValue, this);
}

void CheckDynamicValue::SetValueLocationConstraints() {
  UseRegister(first_input());
  UseRegister(second_input());
}
void CheckDynamicValue::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register first = ToRegister(first_input());
  Register second = ToRegister(second_input());
  __ CompareInt32(first, second);
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

void CheckInt32Condition::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void CheckInt32Condition::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  __ CompareInt32(ToRegister(left_input()), ToRegister(right_input()));
  __ EmitEagerDeoptIf(NegateCondition(ToCondition(condition_)), reason_, this);
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
  DefineAsRegister(this);
  // TODO(victorgomes): Create a arch-agnostic scratch register scope.
  set_temporaries_needed(2);
}
void GeneratorRestoreRegister::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register temp = temps.Acquire();
  Register array = ToRegister(array_input());
  Register result_reg = ToRegister(result());

  // The input and the output can alias, if that happen we use a temporary
  // register and a move at the end.
  Register value = (array == result_reg ? temp : result_reg);

  // Loads the current value in the generator register file.
  __ DecompressAnyTagged(
      value, FieldMemOperand(array, FixedArray::OffsetOfElementAt(index())));

  // And trashs it with StaleRegisterConstant.
  Register scratch = temps.Acquire();
  __ LoadRoot(scratch, RootIndex::kStaleRegister);
  __ StoreTaggedField(
      FieldMemOperand(array, FixedArray::OffsetOfElementAt(index())), scratch);

  if (value != result_reg) {
    __ Move(result_reg, value);
  }
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

void Float64Box::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Box::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  Register object = ToRegister(result());
  __ AllocateHeapNumber(register_snapshot(), object, value);
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
  __ StringCharCodeAt(save_registers, char_code, string, index, scratch,
                      &cached_one_byte_string);
  __ StringFromCharCode(save_registers, &cached_one_byte_string, result_string,
                        char_code, scratch);
}

void TaggedEqual::SetValueLocationConstraints() {
  UseRegister(lhs());
  UseRegister(rhs());
  DefineAsRegister(this);
}
void TaggedEqual::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Label done, if_equal;
  __ JumpIfTaggedEqual(ToRegister(lhs()), ToRegister(rhs()), &if_equal);
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
  __ JumpIfTaggedEqual(ToRegister(lhs()), ToRegister(rhs()), &if_equal);
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
  __ TestTypeOf(object, literal_, &is_true, Label::kNear, true, &is_false,
                Label::kNear, false);
  // Fallthrough into true.
  __ bind(&is_true);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ Jump(&done, Label::kNear);
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
  __ Move(kContextRegister, function_.context().object());
  __ Move(kJavaScriptCallTargetRegister, function_.object());
  __ LoadRoot(kJavaScriptCallNewTargetRegister, RootIndex::kUndefinedValue);
  __ Move(kJavaScriptCallArgCountRegister, actual_parameter_count);
  if (shared_function_info().HasBuiltinId()) {
    __ CallBuiltin(shared_function_info().builtin_id());
  } else {
    __ AssertCallableFunction(kJavaScriptCallTargetRegister);
    __ LoadTaggedPointerField(kJavaScriptCallCodeStartRegister,
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

  baseline::BaselineAssembler basm(masm);
  __ AssertFeedbackVector(scratch0);

  // Case 1).
  Label deopt;
  Register maybe_target_code = scratch1;
  {
    basm.TryLoadOptimizedOsrCode(maybe_target_code, scratch0, feedback_slot,
                                 &deopt, Label::kFar);
  }

  // Case 2).
  {
    __ LoadByte(scratch0,
                FieldMemOperand(scratch0, FeedbackVector::kOsrStateOffset));
    __ DecodeField<FeedbackVector::OsrUrgencyBits>(scratch0);
    basm.JumpIfByte(baseline::Condition::kUnsignedLessThanEqual, scratch0,
                    loop_depth, *no_code_for_osr, Label::kNear);

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
    __ JumpIf(ToCondition(AssertCondition::kEqual), *no_code_for_osr);
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
  __ JumpToDeferredIf(ToCondition(AssertCondition::kAbove),
                      AttemptOnStackReplacement, no_code_for_osr, this,
                      scratch0, scratch1, loop_depth_, feedback_slot_,
                      osr_offset_);
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
  os << "(" << value() << ")";
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

void Abort::PrintParams(std::ostream& os,
                        MaglevGraphLabeller* graph_labeller) const {
  os << "(" << GetAbortReason(reason()) << ")";
}

void AssertInt32::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << condition_ << ")";
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

void LoadTaggedField::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void LoadDoubleField::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void StoreDoubleField::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << offset() << std::dec << ")";
}

void StoreTaggedFieldNoWriteBarrier::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << offset() << std::dec << ")";
}

void StoreMap::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *map_.object() << ")";
}

void StoreTaggedFieldWithWriteBarrier::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << offset() << std::dec << ")";
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

void ReduceInterruptBudget::PrintParams(
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
