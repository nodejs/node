// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-ir.h"

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/maglev-safepoint-table.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/ic/handler-configuration.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-vreg-allocator.h"

namespace v8 {
namespace internal {
namespace maglev {

const char* ToString(Opcode opcode) {
#define DEF_NAME(Name) #Name,
  static constexpr const char* const names[] = {NODE_BASE_LIST(DEF_NAME)};
#undef DEF_NAME
  return names[static_cast<int>(opcode)];
}

#define __ code_gen_state->masm()->

namespace {

// ---
// Vreg allocation helpers.
// ---

int GetVirtualRegister(Node* node) {
  return compiler::UnallocatedOperand::cast(node->result().operand())
      .virtual_register();
}

void DefineAsRegister(MaglevVregAllocationState* vreg_state, Node* node) {
  node->result().SetUnallocated(
      compiler::UnallocatedOperand::MUST_HAVE_REGISTER,
      vreg_state->AllocateVirtualRegister());
}
void DefineAsConstant(MaglevVregAllocationState* vreg_state, Node* node) {
  node->result().SetUnallocated(compiler::UnallocatedOperand::NONE,
                                vreg_state->AllocateVirtualRegister());
}

void DefineAsFixed(MaglevVregAllocationState* vreg_state, Node* node,
                   Register reg) {
  node->result().SetUnallocated(compiler::UnallocatedOperand::FIXED_REGISTER,
                                reg.code(),
                                vreg_state->AllocateVirtualRegister());
}

void DefineSameAsFirst(MaglevVregAllocationState* vreg_state, Node* node) {
  node->result().SetUnallocated(vreg_state->AllocateVirtualRegister(), 0);
}

void UseRegister(Input& input) {
  input.SetUnallocated(compiler::UnallocatedOperand::MUST_HAVE_REGISTER,
                       compiler::UnallocatedOperand::USED_AT_START,
                       GetVirtualRegister(input.node()));
}
void UseAny(Input& input) {
  input.SetUnallocated(
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
      compiler::UnallocatedOperand::USED_AT_START,
      GetVirtualRegister(input.node()));
}
void UseFixed(Input& input, Register reg) {
  input.SetUnallocated(compiler::UnallocatedOperand::FIXED_REGISTER, reg.code(),
                       GetVirtualRegister(input.node()));
}
void UseFixed(Input& input, DoubleRegister reg) {
  input.SetUnallocated(compiler::UnallocatedOperand::FIXED_FP_REGISTER,
                       reg.code(), GetVirtualRegister(input.node()));
}

// ---
// Code gen helpers.
// ---

void PushInput(MaglevCodeGenState* code_gen_state, const Input& input) {
  if (input.operand().IsConstant()) {
    input.node()->LoadToRegister(code_gen_state, kScratchRegister);
    __ Push(kScratchRegister);
  } else {
    // TODO(leszeks): Consider special casing the value. (Toon: could possibly
    // be done through Input directly?)
    const compiler::AllocatedOperand& operand =
        compiler::AllocatedOperand::cast(input.operand());

    if (operand.IsRegister()) {
      __ Push(operand.GetRegister());
    } else {
      DCHECK(operand.IsStackSlot());
      __ Push(code_gen_state->GetStackSlot(operand));
    }
  }
}

class SaveRegisterStateForCall {
 public:
  SaveRegisterStateForCall(MaglevCodeGenState* code_gen_state,
                           RegisterSnapshot snapshot)
      : code_gen_state(code_gen_state), snapshot_(snapshot) {
    __ PushAll(snapshot_.live_registers);
    __ PushAll(snapshot_.live_double_registers);
  }

  ~SaveRegisterStateForCall() {
    __ PopAll(snapshot_.live_double_registers);
    __ PopAll(snapshot_.live_registers);
  }

  MaglevSafepointTableBuilder::Safepoint DefineSafepoint() {
    auto safepoint = code_gen_state->safepoint_table_builder()->DefineSafepoint(
        code_gen_state->masm());
    int pushed_reg_index = 0;
    for (Register reg : snapshot_.live_registers) {
      if (snapshot_.live_tagged_registers.has(reg)) {
        safepoint.DefineTaggedRegister(pushed_reg_index);
      }
      pushed_reg_index++;
    }
    return safepoint;
  }

 private:
  MaglevCodeGenState* code_gen_state;
  RegisterSnapshot snapshot_;
};

// ---
// Deferred code handling.
// ---

// Base case provides an error.
template <typename T, typename Enable = void>
struct CopyForDeferredHelper {
  template <typename U>
  struct No_Copy_Helper_Implemented_For_Type;
  static void Copy(MaglevCompilationInfo* compilation_info,
                   No_Copy_Helper_Implemented_For_Type<T>);
};

// Helper for copies by value.
template <typename T, typename Enable = void>
struct CopyForDeferredByValue {
  static T Copy(MaglevCompilationInfo* compilation_info, T node) {
    return node;
  }
};

// Node pointers are copied by value.
template <typename T>
struct CopyForDeferredHelper<
    T*, typename std::enable_if<std::is_base_of<NodeBase, T>::value>::type>
    : public CopyForDeferredByValue<T*> {};
// Arithmetic values and enums are copied by value.
template <typename T>
struct CopyForDeferredHelper<
    T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
    : public CopyForDeferredByValue<T> {};
template <typename T>
struct CopyForDeferredHelper<
    T, typename std::enable_if<std::is_enum<T>::value>::type>
    : public CopyForDeferredByValue<T> {};
// MaglevCompilationInfos are copied by value.
template <>
struct CopyForDeferredHelper<MaglevCompilationInfo*>
    : public CopyForDeferredByValue<MaglevCompilationInfo*> {};
// Machine registers are copied by value.
template <>
struct CopyForDeferredHelper<Register>
    : public CopyForDeferredByValue<Register> {};
// Bytecode offsets are copied by value.
template <>
struct CopyForDeferredHelper<BytecodeOffset>
    : public CopyForDeferredByValue<BytecodeOffset> {};
// EagerDeoptInfo pointers are copied by value.
template <>
struct CopyForDeferredHelper<EagerDeoptInfo*>
    : public CopyForDeferredByValue<EagerDeoptInfo*> {};

template <typename T>
T CopyForDeferred(MaglevCompilationInfo* compilation_info, T&& value) {
  return CopyForDeferredHelper<T>::Copy(compilation_info,
                                        std::forward<T>(value));
}

template <typename T>
T CopyForDeferred(MaglevCompilationInfo* compilation_info, T& value) {
  return CopyForDeferredHelper<T>::Copy(compilation_info, value);
}

template <typename T>
T CopyForDeferred(MaglevCompilationInfo* compilation_info, const T& value) {
  return CopyForDeferredHelper<T>::Copy(compilation_info, value);
}

template <typename Function, typename FunctionPointer = Function>
struct FunctionArgumentsTupleHelper
    : FunctionArgumentsTupleHelper<Function,
                                   decltype(&FunctionPointer::operator())> {};

template <typename T, typename C, typename R, typename... A>
struct FunctionArgumentsTupleHelper<T, R (C::*)(A...) const> {
  using FunctionPointer = R (*)(A...);
  using Tuple = std::tuple<A...>;
  static constexpr size_t kSize = sizeof...(A);
};

template <typename T>
struct StripFirstTwoTupleArgs;

template <typename T1, typename T2, typename... T>
struct StripFirstTwoTupleArgs<std::tuple<T1, T2, T...>> {
  using Stripped = std::tuple<T...>;
};

template <typename Function>
class DeferredCodeInfoImpl final : public DeferredCodeInfo {
 public:
  using FunctionPointer =
      typename FunctionArgumentsTupleHelper<Function>::FunctionPointer;
  using Tuple = typename StripFirstTwoTupleArgs<
      typename FunctionArgumentsTupleHelper<Function>::Tuple>::Stripped;
  static constexpr size_t kSize = FunctionArgumentsTupleHelper<Function>::kSize;

  template <typename... InArgs>
  explicit DeferredCodeInfoImpl(MaglevCompilationInfo* compilation_info,
                                FunctionPointer function, InArgs&&... args)
      : function(function),
        args(CopyForDeferred(compilation_info, std::forward<InArgs>(args))...) {
  }

  DeferredCodeInfoImpl(DeferredCodeInfoImpl&&) = delete;
  DeferredCodeInfoImpl(const DeferredCodeInfoImpl&) = delete;

  void Generate(MaglevCodeGenState* code_gen_state,
                Label* return_label) override {
    DoCall(code_gen_state, return_label, std::make_index_sequence<kSize - 2>{});
  }

 private:
  template <size_t... I>
  auto DoCall(MaglevCodeGenState* code_gen_state, Label* return_label,
              std::index_sequence<I...>) {
    // TODO(leszeks): This could be replaced with std::apply in C++17.
    return function(code_gen_state, return_label, std::get<I>(args)...);
  }

  FunctionPointer function;
  Tuple args;
};

template <typename Function, typename... Args>
DeferredCodeInfo* PushDeferredCode(MaglevCodeGenState* code_gen_state,
                                   Function&& deferred_code_gen,
                                   Args&&... args) {
  using DeferredCodeInfoT = DeferredCodeInfoImpl<Function>;
  DeferredCodeInfoT* deferred_code =
      code_gen_state->compilation_info()->zone()->New<DeferredCodeInfoT>(
          code_gen_state->compilation_info(), deferred_code_gen,
          std::forward<Args>(args)...);

  code_gen_state->PushDeferredCode(deferred_code);
  return deferred_code;
}

template <typename Function, typename... Args>
void JumpToDeferredIf(Condition cond, MaglevCodeGenState* code_gen_state,
                      Function&& deferred_code_gen, Args&&... args) {
  DeferredCodeInfo* deferred_code = PushDeferredCode<Function, Args...>(
      code_gen_state, std::forward<Function>(deferred_code_gen),
      std::forward<Args>(args)...);
  if (FLAG_code_comments) {
    __ RecordComment("-- Jump to deferred code");
  }
  __ j(cond, &deferred_code->deferred_code_label);
  __ bind(&deferred_code->return_label);
}

// ---
// Deopt
// ---

void RegisterEagerDeopt(MaglevCodeGenState* code_gen_state,
                        EagerDeoptInfo* deopt_info, DeoptimizeReason reason) {
  if (deopt_info->reason != DeoptimizeReason::kUnknown) {
    DCHECK_EQ(deopt_info->reason, reason);
  }
  if (deopt_info->deopt_entry_label.is_unused()) {
    code_gen_state->PushEagerDeopt(deopt_info);
    deopt_info->reason = reason;
  }
}

void EmitEagerDeopt(MaglevCodeGenState* code_gen_state,
                    EagerDeoptInfo* deopt_info, DeoptimizeReason reason) {
  RegisterEagerDeopt(code_gen_state, deopt_info, reason);
  __ RecordComment("-- Jump to eager deopt");
  __ jmp(&deopt_info->deopt_entry_label);
}

template <typename NodeT>
void EmitEagerDeopt(MaglevCodeGenState* code_gen_state, NodeT* node,
                    DeoptimizeReason reason) {
  static_assert(NodeT::kProperties.can_eager_deopt());
  EmitEagerDeopt(code_gen_state, node->eager_deopt_info(), reason);
}

void EmitEagerDeoptIf(Condition cond, MaglevCodeGenState* code_gen_state,
                      DeoptimizeReason reason, EagerDeoptInfo* deopt_info) {
  RegisterEagerDeopt(code_gen_state, deopt_info, reason);
  __ RecordComment("-- Jump to eager deopt");
  __ j(cond, &deopt_info->deopt_entry_label);
}

template <typename NodeT>
void EmitEagerDeoptIf(Condition cond, MaglevCodeGenState* code_gen_state,
                      DeoptimizeReason reason, NodeT* node) {
  static_assert(NodeT::kProperties.can_eager_deopt());
  EmitEagerDeoptIf(cond, code_gen_state, reason, node->eager_deopt_info());
}

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
                  const ConditionalControlNode* node) {
  os << " b" << graph_labeller->BlockId(node->if_true()) << " b"
     << graph_labeller->BlockId(node->if_false());
}

template <typename NodeT>
void PrintImpl(std::ostream& os, MaglevGraphLabeller* graph_labeller,
               const NodeT* node) {
  os << node->opcode();
  node->PrintParams(os, graph_labeller);
  PrintInputs(os, graph_labeller, node);
  PrintResult(os, graph_labeller, node);
  PrintTargets(os, graph_labeller, node);
}

}  // namespace

void NodeBase::Print(std::ostream& os,
                     MaglevGraphLabeller* graph_labeller) const {
  switch (opcode()) {
#define V(Name)         \
  case Opcode::k##Name: \
    return PrintImpl(os, graph_labeller, this->Cast<Name>());
    NODE_BASE_LIST(V)
#undef V
  }
  UNREACHABLE();
}

namespace {
size_t GetInputLocationsArraySize(const MaglevCompilationUnit& compilation_unit,
                                  const CheckpointedInterpreterState& state) {
  size_t size = state.register_frame->size(compilation_unit);
  const CheckpointedInterpreterState* parent = state.parent;
  const MaglevCompilationUnit* parent_unit = compilation_unit.caller();
  while (parent != nullptr) {
    size += parent->register_frame->size(*parent_unit);
    parent = parent->parent;
    parent_unit = parent_unit->caller();
  }
  return size;
}
}  // namespace

DeoptInfo::DeoptInfo(Zone* zone, const MaglevCompilationUnit& compilation_unit,
                     CheckpointedInterpreterState state)
    : unit(compilation_unit),
      state(state),
      input_locations(zone->NewArray<InputLocation>(
          GetInputLocationsArraySize(compilation_unit, state))) {
  // Initialise InputLocations so that they correctly don't have a next use id.
  for (size_t i = 0; i < GetInputLocationsArraySize(compilation_unit, state);
       ++i) {
    new (&input_locations[i]) InputLocation();
  }
}

// ---
// Nodes
// ---
void ValueNode::LoadToRegister(MaglevCodeGenState* code_gen_state,
                               Register reg) {
  switch (opcode()) {
#define V(Name)         \
  case Opcode::k##Name: \
    return this->Cast<Name>()->DoLoadToRegister(code_gen_state, reg);
    VALUE_NODE_LIST(V)
#undef V
    default:
      UNREACHABLE();
  }
}
void ValueNode::DoLoadToRegister(MaglevCodeGenState* code_gen_state,
                                 Register reg) {
  DCHECK(is_spilled());
  __ movq(reg, code_gen_state->GetStackSlot(
                   compiler::AllocatedOperand::cast(spill_slot())));
}
Handle<Object> ValueNode::Reify(Isolate* isolate) {
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

void SmiConstant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsConstant(vreg_state, this);
}
void SmiConstant::GenerateCode(MaglevCodeGenState* code_gen_state,
                               const ProcessingState& state) {}
Handle<Object> SmiConstant::DoReify(Isolate* isolate) {
  return handle(value_, isolate);
}
void SmiConstant::DoLoadToRegister(MaglevCodeGenState* code_gen_state,
                                   Register reg) {
  __ Move(reg, Immediate(value()));
}
void SmiConstant::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Float64Constant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsConstant(vreg_state, this);
}
void Float64Constant::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {}
Handle<Object> Float64Constant::DoReify(Isolate* isolate) {
  return isolate->factory()->NewNumber(value_);
}
void Float64Constant::DoLoadToRegister(MaglevCodeGenState* code_gen_state,
                                       DoubleRegister reg) {
  __ Move(reg, value());
}
void Float64Constant::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Constant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsConstant(vreg_state, this);
}
void Constant::GenerateCode(MaglevCodeGenState* code_gen_state,
                            const ProcessingState& state) {}
void Constant::DoLoadToRegister(MaglevCodeGenState* code_gen_state,
                                Register reg) {
  __ Move(reg, object_.object());
}
Handle<Object> Constant::DoReify(Isolate* isolate) { return object_.object(); }
void Constant::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {
  os << "(" << object_ << ")";
}

void InitialValue::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  // TODO(leszeks): Make this nicer.
  result().SetUnallocated(compiler::UnallocatedOperand::FIXED_SLOT,
                          (StandardFrameConstants::kExpressionsOffset -
                           UnoptimizedFrameConstants::kRegisterFileFromFp) /
                                  kSystemPointerSize +
                              source().index(),
                          vreg_state->AllocateVirtualRegister());
}
void InitialValue::GenerateCode(MaglevCodeGenState* code_gen_state,
                                const ProcessingState& state) {
  // No-op, the value is already in the appropriate slot.
}
void InitialValue::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  os << "(" << source().ToString() << ")";
}

void LoadGlobal::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseFixed(context(), kContextRegister);
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void LoadGlobal::GenerateCode(MaglevCodeGenState* code_gen_state,
                              const ProcessingState& state) {
  // TODO(leszeks): Port the nice Sparkplug CallBuiltin helper.
  using D = CallInterfaceDescriptorFor<Builtin::kLoadGlobalIC>::type;

  DCHECK_EQ(ToRegister(context()), kContextRegister);

  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);

  __ CallBuiltin(Builtin::kLoadGlobalIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}
void LoadGlobal::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name() << ")";
}

void RegisterInput::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, input());
}
void RegisterInput::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {
  // Nothing to be done, the value is already in the register.
}
void RegisterInput::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << input() << ")";
}

void RootConstant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsConstant(vreg_state, this);
}
void RootConstant::GenerateCode(MaglevCodeGenState* code_gen_state,
                                const ProcessingState& state) {}
void RootConstant::DoLoadToRegister(MaglevCodeGenState* code_gen_state,
                                    Register reg) {
  __ LoadRoot(reg, index());
}
Handle<Object> RootConstant::DoReify(Isolate* isolate) {
  return isolate->root_handle(index());
}
void RootConstant::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  os << "(" << RootsTable::name(index()) << ")";
}

void CreateEmptyArrayLiteral::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateEmptyArrayLiteral::GenerateCode(MaglevCodeGenState* code_gen_state,
                                           const ProcessingState& state) {
  using D = CreateEmptyArrayLiteralDescriptor;
  __ Move(kContextRegister, code_gen_state->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), Smi::FromInt(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(Builtin::kCreateEmptyArrayLiteral);
}

void CreateArrayLiteral::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateArrayLiteral::GenerateCode(MaglevCodeGenState* code_gen_state,
                                      const ProcessingState& state) {
  __ Move(kContextRegister, code_gen_state->native_context().object());
  __ Push(feedback().vector);
  __ Push(TaggedIndex::FromIntptr(feedback().index()));
  __ Push(constant_elements().object());
  __ Push(Smi::FromInt(flags()));
  __ CallRuntime(Runtime::kCreateArrayLiteral);
}

void CreateShallowArrayLiteral::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateShallowArrayLiteral::GenerateCode(MaglevCodeGenState* code_gen_state,
                                             const ProcessingState& state) {
  using D = CreateShallowArrayLiteralDescriptor;
  __ Move(D::ContextRegister(), code_gen_state->native_context().object());
  __ Move(D::GetRegisterParameter(D::kMaybeFeedbackVector), feedback().vector);
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kConstantElements),
          constant_elements().object());
  __ Move(D::GetRegisterParameter(D::kFlags), Smi::FromInt(flags()));
  __ CallBuiltin(Builtin::kCreateShallowArrayLiteral);
}

void CreateObjectLiteral::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateObjectLiteral::GenerateCode(MaglevCodeGenState* code_gen_state,
                                       const ProcessingState& state) {
  __ Move(kContextRegister, code_gen_state->native_context().object());
  __ Push(feedback().vector);
  __ Push(TaggedIndex::FromIntptr(feedback().index()));
  __ Push(boilerplate_descriptor().object());
  __ Push(Smi::FromInt(flags()));
  __ CallRuntime(Runtime::kCreateObjectLiteral);
}

void CreateShallowObjectLiteral::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateShallowObjectLiteral::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  using D = CreateShallowObjectLiteralDescriptor;
  __ Move(D::ContextRegister(), code_gen_state->native_context().object());
  __ Move(D::GetRegisterParameter(D::kMaybeFeedbackVector), feedback().vector);
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kDesc), boilerplate_descriptor().object());
  __ Move(D::GetRegisterParameter(D::kFlags), Smi::FromInt(flags()));
  __ CallBuiltin(Builtin::kCreateShallowObjectLiteral);
}

void CreateFunctionContext::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<
      Builtin::kFastNewFunctionContextFunction>::type;
  static_assert(D::HasContextParameter());
  UseFixed(context(), D::ContextRegister());
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateFunctionContext::GenerateCode(MaglevCodeGenState* code_gen_state,
                                         const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<
      Builtin::kFastNewFunctionContextFunction>::type;
  DCHECK_LE(slot_count(), ConstructorBuiltins::MaximumFunctionContextSlots());
  DCHECK_EQ(scope_info().object()->scope_type(), ScopeType::FUNCTION_SCOPE);

  DCHECK_EQ(ToRegister(context()), D::ContextRegister());
  __ Move(D::GetRegisterParameter(D::kScopeInfo), scope_info().object());
  __ Move(D::GetRegisterParameter(D::kSlots), Immediate(slot_count()));
  // TODO(leszeks): Consider inlining this allocation.
  __ CallBuiltin(Builtin::kFastNewFunctionContextFunction);
  code_gen_state->safepoint_table_builder()->DefineSafepoint(
      code_gen_state->masm());
}
void CreateFunctionContext::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *scope_info().object() << ", " << slot_count() << ")";
}

void FastCreateClosure::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kFastNewClosure>::type;
  static_assert(D::HasContextParameter());
  UseFixed(context(), D::ContextRegister());
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void FastCreateClosure::GenerateCode(MaglevCodeGenState* code_gen_state,
                                     const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kFastNewClosure>::type;

  DCHECK_EQ(ToRegister(context()), D::ContextRegister());
  __ Move(D::GetRegisterParameter(D::kSharedFunctionInfo),
          shared_function_info().object());
  __ Move(D::GetRegisterParameter(D::kFeedbackCell), feedback_cell().object());
  __ CallBuiltin(Builtin::kFastNewClosure);
}
void FastCreateClosure::PrintParams(std::ostream& os,
                                    MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *shared_function_info().object() << ", "
     << feedback_cell().object() << ")";
}

void CreateClosure::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseFixed(context(), kContextRegister);
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateClosure::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {
  Runtime::FunctionId function_id =
      pretenured() ? Runtime::kNewClosure_Tenured : Runtime::kNewClosure;
  __ Push(shared_function_info().object());
  __ Push(feedback_cell().object());
  __ CallRuntime(function_id);
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

void CheckMaps::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckMaps::GenerateCode(MaglevCodeGenState* code_gen_state,
                             const ProcessingState& state) {
  Register object = ToRegister(receiver_input());

  __ AssertNotSmi(object);
  __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map().object());
  EmitEagerDeoptIf(not_equal, code_gen_state, DeoptimizeReason::kWrongMap,
                   this);
}
void CheckMaps::PrintParams(std::ostream& os,
                            MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *map().object() << ")";
}
void CheckSmi::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckSmi::GenerateCode(MaglevCodeGenState* code_gen_state,
                            const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  EmitEagerDeoptIf(NegateCondition(is_smi), code_gen_state,
                   DeoptimizeReason::kNotASmi, this);
}
void CheckSmi::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {}

void CheckHeapObject::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckHeapObject::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  EmitEagerDeoptIf(is_smi, code_gen_state, DeoptimizeReason::kSmi, this);
}
void CheckHeapObject::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {}

void CheckString::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckString::GenerateCode(MaglevCodeGenState* code_gen_state,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  __ AssertNotSmi(object);
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceTypeRange(kScratchRegister, kScratchRegister, FIRST_STRING_TYPE,
                          LAST_STRING_TYPE);
  EmitEagerDeoptIf(above, code_gen_state, DeoptimizeReason::kNotAString, this);
}
void CheckString::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {}

void CheckMapsWithMigration::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckMapsWithMigration::GenerateCode(MaglevCodeGenState* code_gen_state,
                                          const ProcessingState& state) {
  Register object = ToRegister(receiver_input());

  __ AssertNotSmi(object);
  __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map().object());

  JumpToDeferredIf(
      not_equal, code_gen_state,
      [](MaglevCodeGenState* code_gen_state, Label* return_label,
         Register object, CheckMapsWithMigration* node,
         EagerDeoptInfo* deopt_info) {
        RegisterEagerDeopt(code_gen_state, deopt_info,
                           DeoptimizeReason::kWrongMap);

        // Reload the map to avoid needing to save it on a temporary in the fast
        // path.
        __ LoadMap(kScratchRegister, object);
        // If the map is not deprecated, deopt straight away.
        __ movl(kScratchRegister,
                FieldOperand(kScratchRegister, Map::kBitField3Offset));
        __ testl(kScratchRegister,
                 Immediate(Map::Bits3::IsDeprecatedBit::kMask));
        __ j(zero, &deopt_info->deopt_entry_label);

        // Otherwise, try migrating the object. If the migration
        // returns Smi zero, then it failed and we should deopt.
        Register return_val = Register::no_reg();
        {
          SaveRegisterStateForCall save_register_state(
              code_gen_state, node->register_snapshot());

          __ Push(object);
          __ Move(kContextRegister,
                  code_gen_state->broker()->target_native_context().object());
          __ CallRuntime(Runtime::kTryMigrateInstance);
          save_register_state.DefineSafepoint();

          // Make sure the return value is preserved across the live register
          // restoring pop all.
          return_val = kReturnRegister0;
          if (node->register_snapshot().live_registers.has(return_val)) {
            DCHECK(!node->register_snapshot().live_registers.has(
                kScratchRegister));
            __ movq(kScratchRegister, return_val);
            return_val = kScratchRegister;
          }
        }

        // On failure, the returned value is zero
        __ cmpl(return_val, Immediate(0));
        __ j(equal, &deopt_info->deopt_entry_label);

        // The migrated object is returned on success, retry the map check.
        __ Move(object, return_val);
        // Manually load the map pointer without uncompressing it.
        __ Cmp(FieldOperand(object, HeapObject::kMapOffset),
               node->map().object());
        __ j(equal, return_label);
        __ jmp(&deopt_info->deopt_entry_label);
      },
      object, this, eager_deopt_info());
}
void CheckMapsWithMigration::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *map().object() << ")";
}

void CheckedInternalizedString::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  set_temporaries_needed(1);
  DefineSameAsFirst(vreg_state, this);
}
void CheckedInternalizedString::GenerateCode(MaglevCodeGenState* code_gen_state,
                                             const ProcessingState& state) {
  Register object = ToRegister(object_input());
  RegList temps = temporaries();
  Register map_tmp = temps.PopFirst();

  Condition is_smi = __ CheckSmi(object);
  EmitEagerDeoptIf(is_smi, code_gen_state, DeoptimizeReason::kWrongMap, this);

  __ LoadMap(map_tmp, object);
  __ RecordComment("Test IsInternalizedString");
  __ testw(FieldOperand(map_tmp, Map::kInstanceTypeOffset),
           Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  static_assert((kStringTag | kInternalizedTag) == 0);
  JumpToDeferredIf(
      not_zero, code_gen_state,
      [](MaglevCodeGenState* code_gen_state, Label* return_label,
         Register object, CheckedInternalizedString* node,
         EagerDeoptInfo* deopt_info, Register map_tmp) {
        __ RecordComment("Deferred Test IsThinString");
        __ movw(map_tmp, FieldOperand(map_tmp, Map::kInstanceTypeOffset));
        static_assert(kThinStringTagBit > 0);
        __ testb(map_tmp, Immediate(kThinStringTagBit));
        __ j(zero, &deopt_info->deopt_entry_label);
        __ LoadTaggedPointerField(
            object, FieldOperand(object, ThinString::kActualOffset));
        if (FLAG_debug_code) {
          __ RecordComment("DCHECK IsInternalizedString");
          __ LoadMap(map_tmp, object);
          __ testw(FieldOperand(map_tmp, Map::kInstanceTypeOffset),
                   Immediate(kIsNotStringMask | kIsNotInternalizedMask));
          static_assert((kStringTag | kInternalizedTag) == 0);
          __ Check(zero, AbortReason::kUnexpectedValue);
        }
        __ jmp(return_label);
      },
      object, this, eager_deopt_info(), map_tmp);
}

void LoadTaggedField::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  DefineAsRegister(vreg_state, this);
}
void LoadTaggedField::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressAnyTagged(ToRegister(result()), FieldOperand(object, offset()));
}
void LoadTaggedField::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void LoadDoubleField::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  DefineAsRegister(vreg_state, this);
  set_temporaries_needed(1);
}
void LoadDoubleField::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register tmp = temporaries().PopFirst();
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Movsd(ToDoubleRegister(result()),
           FieldOperand(tmp, HeapNumber::kValueOffset));
}
void LoadDoubleField::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(0x" << std::hex << offset() << std::dec << ")";
}

void StoreTaggedFieldNoWriteBarrier::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreTaggedFieldNoWriteBarrier::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  __ StoreTaggedField(FieldOperand(object, offset()), value);
}
void StoreTaggedFieldNoWriteBarrier::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << offset() << std::dec << ")";
}

void StoreTaggedFieldWithWriteBarrier::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
  UseRegister(value_input());
}
void StoreTaggedFieldWithWriteBarrier::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));

  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  __ StoreTaggedField(FieldOperand(object, offset()), value);

  DeferredCodeInfo* deferred_write_barrier = PushDeferredCode(
      code_gen_state,
      [](MaglevCodeGenState* code_gen_state, Label* return_label,
         Register value, Register object,
         StoreTaggedFieldWithWriteBarrier* node) {
        ASM_CODE_COMMENT_STRING(code_gen_state->masm(),
                                "Write barrier slow path");
        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }
        __ PushAll(saved);

        __ CheckPageFlag(value, kScratchRegister,
                         MemoryChunk::kPointersToHereAreInterestingMask, zero,
                         return_label);
        __ leaq(slot_reg, FieldOperand(object, node->offset()));

        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ jmp(return_label);
      },
      value, object, this);

  __ JumpIfSmi(value, &deferred_write_barrier->return_label);
  __ CheckPageFlag(object, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_write_barrier->deferred_code_label);
  __ bind(&deferred_write_barrier->return_label);
}
void StoreTaggedFieldWithWriteBarrier::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << offset() << std::dec << ")";
}

void LoadNamedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = LoadWithVectorDescriptor;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void LoadNamedGeneric::GenerateCode(MaglevCodeGenState* code_gen_state,
                                    const ProcessingState& state) {
  using D = LoadWithVectorDescriptor;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kLoadIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}
void LoadNamedGeneric::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void SetNamedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void SetNamedGeneric::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kStoreIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}
void SetNamedGeneric::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void DefineNamedOwnGeneric::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineNamedOwnIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void DefineNamedOwnGeneric::GenerateCode(MaglevCodeGenState* code_gen_state,
                                         const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineNamedOwnIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kDefineNamedOwnIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}
void DefineNamedOwnGeneric::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void SetKeyedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedStoreIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(key_input(), D::GetRegisterParameter(D::kName));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void SetKeyedGeneric::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedStoreIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(key_input()), D::GetRegisterParameter(D::kName));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kKeyedStoreIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}

void DefineKeyedOwnGeneric::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedStoreIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(key_input(), D::GetRegisterParameter(D::kName));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void DefineKeyedOwnGeneric::GenerateCode(MaglevCodeGenState* code_gen_state,
                                         const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kDefineKeyedOwnIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(key_input()), D::GetRegisterParameter(D::kName));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kDefineKeyedOwnIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}

void StoreInArrayLiteralGeneric::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreInArrayLiteralIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(name_input(), D::GetRegisterParameter(D::kName));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void StoreInArrayLiteralGeneric::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreInArrayLiteralIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  DCHECK_EQ(ToRegister(name_input()), D::GetRegisterParameter(D::kName));
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kStoreInArrayLiteralIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}

void GetKeyedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedLoadIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(key_input(), D::GetRegisterParameter(D::kName));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void GetKeyedGeneric::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedLoadIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(key_input()), D::GetRegisterParameter(D::kName));
  __ Move(D::GetRegisterParameter(D::kSlot),
          TaggedIndex::FromIntptr(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kKeyedLoadIC);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}

void GapMove::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UNREACHABLE();
}
void GapMove::GenerateCode(MaglevCodeGenState* code_gen_state,
                           const ProcessingState& state) {
  if (source().IsRegister()) {
    Register source_reg = ToRegister(source());
    if (target().IsAnyRegister()) {
      DCHECK(target().IsRegister());
      __ movq(ToRegister(target()), source_reg);
    } else {
      __ movq(code_gen_state->ToMemOperand(target()), source_reg);
    }
  } else if (source().IsDoubleRegister()) {
    DoubleRegister source_reg = ToDoubleRegister(source());
    if (target().IsAnyRegister()) {
      DCHECK(target().IsDoubleRegister());
      __ Movsd(ToDoubleRegister(target()), source_reg);
    } else {
      __ Movsd(code_gen_state->ToMemOperand(target()), source_reg);
    }
  } else {
    DCHECK(source().IsAnyStackSlot());
    MemOperand source_op = code_gen_state->ToMemOperand(source());
    if (target().IsRegister()) {
      __ movq(ToRegister(target()), source_op);
    } else if (target().IsDoubleRegister()) {
      __ Movsd(ToDoubleRegister(target()), source_op);
    } else {
      DCHECK(target().IsAnyStackSlot());
      __ movq(kScratchRegister, source_op);
      __ movq(code_gen_state->ToMemOperand(target()), kScratchRegister);
    }
  }
}
void GapMove::PrintParams(std::ostream& os,
                          MaglevGraphLabeller* graph_labeller) const {
  os << "(" << source() << " → " << target() << ")";
}
void ConstantGapMove::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UNREACHABLE();
}

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
};  // namespace
void ConstantGapMove::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  switch (node_->opcode()) {
#define CASE(Name)                                \
  case Opcode::k##Name:                           \
    return node_->Cast<Name>()->DoLoadToRegister( \
        code_gen_state, GetRegister<Name::OutputRegister>::Get(target()));
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}
void ConstantGapMove::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {
  os << "(";
  graph_labeller->PrintNodeLabel(os, node_);
  os << " → " << target() << ")";
}

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
void UnaryWithFeedbackNode<Derived, kOperation>::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  using D = UnaryOp_WithFeedbackDescriptor;
  UseFixed(operand_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}

template <class Derived, Operation kOperation>
void UnaryWithFeedbackNode<Derived, kOperation>::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  using D = UnaryOp_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(operand_input()), D::GetRegisterParameter(D::kValue));
  __ Move(kContextRegister, code_gen_state->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), Immediate(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(BuiltinFor(kOperation));
  code_gen_state->DefineLazyDeoptPoint(this->lazy_deopt_info());
}

template <class Derived, Operation kOperation>
void BinaryWithFeedbackNode<Derived, kOperation>::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  using D = BinaryOp_WithFeedbackDescriptor;
  UseFixed(left_input(), D::GetRegisterParameter(D::kLeft));
  UseFixed(right_input(), D::GetRegisterParameter(D::kRight));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}

template <class Derived, Operation kOperation>
void BinaryWithFeedbackNode<Derived, kOperation>::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  using D = BinaryOp_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(left_input()), D::GetRegisterParameter(D::kLeft));
  DCHECK_EQ(ToRegister(right_input()), D::GetRegisterParameter(D::kRight));
  __ Move(kContextRegister, code_gen_state->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), Immediate(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(BuiltinFor(kOperation));
  code_gen_state->DefineLazyDeoptPoint(this->lazy_deopt_info());
}

#define DEF_OPERATION(Name)                                        \
  void Name::AllocateVreg(MaglevVregAllocationState* vreg_state) { \
    Base::AllocateVreg(vreg_state);                                \
  }                                                                \
  void Name::GenerateCode(MaglevCodeGenState* code_gen_state,      \
                          const ProcessingState& state) {          \
    Base::GenerateCode(code_gen_state, state);                     \
  }
GENERIC_OPERATIONS_NODE_LIST(DEF_OPERATION)
#undef DEF_OPERATION

void Int32AddWithOverflow::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32AddWithOverflow::GenerateCode(MaglevCodeGenState* code_gen_state,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ addl(left, right);
  EmitEagerDeoptIf(overflow, code_gen_state, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32SubtractWithOverflow::GenerateCode(MaglevCodeGenState* code_gen_state,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ subl(left, right);
  EmitEagerDeoptIf(overflow, code_gen_state, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
  set_temporaries_needed(1);
}

void Int32MultiplyWithOverflow::GenerateCode(MaglevCodeGenState* code_gen_state,
                                             const ProcessingState& state) {
  Register result = ToRegister(this->result());
  Register right = ToRegister(right_input());
  DCHECK_EQ(result, ToRegister(left_input()));

  Register saved_left = temporaries().first();
  __ movl(saved_left, result);
  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ imull(result, right);
  EmitEagerDeoptIf(overflow, code_gen_state, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ cmpl(result, Immediate(0));
  __ j(not_zero, &end);
  {
    __ orl(saved_left, right);
    __ cmpl(saved_left, Immediate(0));
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    EmitEagerDeoptIf(less, code_gen_state, DeoptimizeReason::kOverflow, this);
  }
  __ bind(&end);
}

void Int32DivideWithOverflow::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseFixed(left_input(), rax);
  UseRegister(right_input());
  DefineAsFixed(vreg_state, this, rax);
  // rdx is clobbered by idiv.
  RequireSpecificTemporary(rdx);
}

void Int32DivideWithOverflow::GenerateCode(MaglevCodeGenState* code_gen_state,
                                           const ProcessingState& state) {
  DCHECK_EQ(rax, ToRegister(left_input()));
  DCHECK(temporaries().has(rdx));
  Register right = ToRegister(right_input());
  // Clear rdx so that it doesn't participate in the division.
  __ xorl(rdx, rdx);
  // TODO(leszeks): peephole optimise division by a constant.
  __ idivl(right);
  __ cmpl(rdx, Immediate(0));
  EmitEagerDeoptIf(equal, code_gen_state, DeoptimizeReason::kNotInt32, this);
}

void Int32BitwiseAnd::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32BitwiseAnd::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ andl(left, right);
}

void Int32BitwiseOr::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32BitwiseOr::GenerateCode(MaglevCodeGenState* code_gen_state,
                                  const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ orl(left, right);
}

void Int32BitwiseXor::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32BitwiseXor::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ xorl(left, right);
}

void Int32ShiftLeft::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  // Use the "shift by cl" variant of shl.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(vreg_state, this);
}

void Int32ShiftLeft::GenerateCode(MaglevCodeGenState* code_gen_state,
                                  const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ shll_cl(left);
}

void Int32ShiftRight::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  // Use the "shift by cl" variant of sar.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(vreg_state, this);
}

void Int32ShiftRight::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ sarl_cl(left);
}

void Int32ShiftRightLogical::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  // Use the "shift by cl" variant of shr.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(vreg_state, this);
}

void Int32ShiftRightLogical::GenerateCode(MaglevCodeGenState* code_gen_state,
                                          const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ shrl_cl(left);
}

namespace {

constexpr Condition ConditionFor(Operation operation) {
  switch (operation) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return equal;
    case Operation::kLessThan:
      return less;
    case Operation::kLessThanOrEqual:
      return less_equal;
    case Operation::kGreaterThan:
      return greater;
    case Operation::kGreaterThanOrEqual:
      return greater_equal;
    default:
      UNREACHABLE();
  }
}

}  // namespace

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(vreg_state, this);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register result = ToRegister(this->result());
  Label is_true, end;
  __ cmpl(left, right);
  // TODO(leszeks): Investigate using cmov here.
  __ j(ConditionFor(kOperation), &is_true);
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

#define DEF_OPERATION(Name)                                        \
  void Name::AllocateVreg(MaglevVregAllocationState* vreg_state) { \
    Base::AllocateVreg(vreg_state);                                \
  }                                                                \
  void Name::GenerateCode(MaglevCodeGenState* code_gen_state,      \
                          const ProcessingState& state) {          \
    Base::GenerateCode(code_gen_state, state);                     \
  }
DEF_OPERATION(Int32Equal)
DEF_OPERATION(Int32StrictEqual)
DEF_OPERATION(Int32LessThan)
DEF_OPERATION(Int32LessThanOrEqual)
DEF_OPERATION(Int32GreaterThan)
DEF_OPERATION(Int32GreaterThanOrEqual)
#undef DEF_OPERATION

void Float64Add::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Float64Add::GenerateCode(MaglevCodeGenState* code_gen_state,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Addsd(left, right);
}

void Float64Subtract::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Float64Subtract::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Subsd(left, right);
}

void Float64Multiply::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Float64Multiply::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Mulsd(left, right);
}

void Float64Divide::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Float64Divide::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Divsd(left, right);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(vreg_state, this);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::GenerateCode(
    MaglevCodeGenState* code_gen_state, const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  Register result = ToRegister(this->result());
  Label is_true, end;
  __ Ucomisd(left, right);
  // TODO(leszeks): Investigate using cmov here.
  __ j(ConditionFor(kOperation), &is_true);
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

#define DEF_OPERATION(Name)                                        \
  void Name::AllocateVreg(MaglevVregAllocationState* vreg_state) { \
    Base::AllocateVreg(vreg_state);                                \
  }                                                                \
  void Name::GenerateCode(MaglevCodeGenState* code_gen_state,      \
                          const ProcessingState& state) {          \
    Base::GenerateCode(code_gen_state, state);                     \
  }
DEF_OPERATION(Float64Equal)
DEF_OPERATION(Float64StrictEqual)
DEF_OPERATION(Float64LessThan)
DEF_OPERATION(Float64LessThanOrEqual)
DEF_OPERATION(Float64GreaterThan)
DEF_OPERATION(Float64GreaterThanOrEqual)
#undef DEF_OPERATION

void CheckedSmiUntag::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineSameAsFirst(vreg_state, this);
}

void CheckedSmiUntag::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register value = ToRegister(input());
  // TODO(leszeks): Consider optimizing away this test and using the carry bit
  // of the `sarl` for cases where the deopt uses the value from a different
  // register.
  Condition is_smi = __ CheckSmi(value);
  EmitEagerDeoptIf(NegateCondition(is_smi), code_gen_state,
                   DeoptimizeReason::kNotASmi, this);
  __ SmiToInt32(value);
}

void CheckedSmiTag::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineSameAsFirst(vreg_state, this);
}

void CheckedSmiTag::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {
  Register reg = ToRegister(input());
  __ addl(reg, reg);
  EmitEagerDeoptIf(overflow, code_gen_state, DeoptimizeReason::kOverflow, this);
}

void Int32Constant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsConstant(vreg_state, this);
}
void Int32Constant::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {}
void Int32Constant::DoLoadToRegister(MaglevCodeGenState* code_gen_state,
                                     Register reg) {
  __ Move(reg, Immediate(value()));
}
Handle<Object> Int32Constant::DoReify(Isolate* isolate) {
  return isolate->factory()->NewNumber(value());
}
void Int32Constant::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Float64Box::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = NewHeapNumberDescriptor;
  UseFixed(input(), D::GetDoubleRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void Float64Box::GenerateCode(MaglevCodeGenState* code_gen_state,
                              const ProcessingState& state) {
  // TODO(victorgomes): Inline heap number allocation.
  __ CallBuiltin(Builtin::kNewHeapNumber);
}

void CheckedFloat64Unbox::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineAsRegister(vreg_state, this);
}
void CheckedFloat64Unbox::GenerateCode(MaglevCodeGenState* code_gen_state,
                                       const ProcessingState& state) {
  Register value = ToRegister(input());
  Label is_not_smi, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi);
  // If Smi, convert to Float64.
  __ SmiToInt32(value);
  __ Cvtlsi2sd(ToDoubleRegister(result()), value);
  // TODO(v8:7700): Add a constraint to the register allocator to indicate that
  // the value in the input register is "trashed" by this node. Currently we
  // have the invariant that the input register should not be mutated when it is
  // not the same as the output register or the function does not call a
  // builtin. So, we recover the Smi value here.
  __ SmiTag(value);
  __ jmp(&done);
  __ bind(&is_not_smi);
  // Check if HeapNumber, deopt otherwise.
  __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                 RootIndex::kHeapNumberMap);
  EmitEagerDeoptIf(not_equal, code_gen_state, DeoptimizeReason::kNotANumber,
                   this);
  __ Movsd(ToDoubleRegister(result()),
           FieldOperand(value, HeapNumber::kValueOffset));
  __ bind(&done);
}

void LogicalNot::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
  DefineAsRegister(vreg_state, this);
}
void LogicalNot::GenerateCode(MaglevCodeGenState* code_gen_state,
                              const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());
  Label not_equal_true;
  // We load the constant true to the return value and we return it if the
  // object is not equal to it. Otherwise we load the constant false.
  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ cmp_tagged(return_value, object);
  __ j(not_equal, &not_equal_true);
  __ LoadRoot(return_value, RootIndex::kFalseValue);
  if (FLAG_debug_code) {
    Label is_equal_true;
    __ jmp(&is_equal_true);
    __ bind(&not_equal_true);
    // LogicalNot expects either the constants true or false.
    // We know it is not true, so it must be false!
    __ CompareRoot(object, RootIndex::kFalseValue);
    __ Check(equal, AbortReason::kUnexpectedValue);
    __ bind(&is_equal_true);
  } else {
    __ bind(&not_equal_true);
  }
}

void TaggedEqual::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(lhs());
  UseRegister(rhs());
  DefineAsRegister(vreg_state, this);
}
void TaggedEqual::GenerateCode(MaglevCodeGenState* code_gen_state,
                               const ProcessingState& state) {
  Label done, if_equal;
  __ cmp_tagged(ToRegister(lhs()), ToRegister(rhs()));
  __ j(equal, &if_equal, Label::kNear);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);
  __ jmp(&done, Label::kNear);
  __ bind(&if_equal);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ bind(&done);
}

void TestInstanceOf::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kInstanceOf>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object(), D::GetRegisterParameter(D::kLeft));
  UseFixed(callable(), D::GetRegisterParameter(D::kRight));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void TestInstanceOf::GenerateCode(MaglevCodeGenState* code_gen_state,
                                  const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kInstanceOf>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object()), D::GetRegisterParameter(D::kLeft));
  DCHECK_EQ(ToRegister(callable()), D::GetRegisterParameter(D::kRight));
#endif
  __ CallBuiltin(Builtin::kInstanceOf);
  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}

void TestUndetectable::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
  set_temporaries_needed(1);
  DefineAsRegister(vreg_state, this);
}
void TestUndetectable::GenerateCode(MaglevCodeGenState* code_gen_state,
                                    const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());
  RegList temps = temporaries();
  Register tmp = temps.PopFirst();
  Label done;
  __ LoadRoot(return_value, RootIndex::kFalseValue);
  // If the object is an Smi, return false.
  __ JumpIfSmi(object, &done);
  // If it is a HeapObject, load the map and check for the undetectable bit.
  __ LoadMap(tmp, object);
  __ movl(tmp, FieldOperand(tmp, Map::kBitFieldOffset));
  __ testl(tmp, Immediate(Map::Bits1::IsUndetectableBit::kMask));
  __ j(zero, &done);
  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ bind(&done);
}

void ChangeInt32ToFloat64::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineAsRegister(vreg_state, this);
}
void ChangeInt32ToFloat64::GenerateCode(MaglevCodeGenState* code_gen_state,
                                        const ProcessingState& state) {
  __ Cvtlsi2sd(ToDoubleRegister(result()), ToRegister(input()));
}

void Phi::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  // Phi inputs are processed in the post-process, once loop phis' inputs'
  // v-regs are allocated.
  result().SetUnallocated(
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
      vreg_state->AllocateVirtualRegister());
}
// TODO(verwaest): Remove after switching the register allocator.
void Phi::AllocateVregInPostProcess(MaglevVregAllocationState* vreg_state) {
  for (Input& input : *this) {
    UseAny(input);
  }
}
void Phi::GenerateCode(MaglevCodeGenState* code_gen_state,
                       const ProcessingState& state) {}
void Phi::PrintParams(std::ostream& os,
                      MaglevGraphLabeller* graph_labeller) const {
  os << "(" << owner().ToString() << ")";
}

void Call::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseFixed(function(), CallTrampolineDescriptor::GetRegisterParameter(
                           CallTrampolineDescriptor::kFunction));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void Call::GenerateCode(MaglevCodeGenState* code_gen_state,
                        const ProcessingState& state) {
  // TODO(leszeks): Port the nice Sparkplug CallBuiltin helper.

  DCHECK_EQ(ToRegister(function()),
            CallTrampolineDescriptor::GetRegisterParameter(
                CallTrampolineDescriptor::kFunction));
  DCHECK_EQ(ToRegister(context()), kContextRegister);

  for (int i = num_args() - 1; i >= 0; --i) {
    PushInput(code_gen_state, arg(i));
  }

  uint32_t arg_count = num_args();
  __ Move(CallTrampolineDescriptor::GetRegisterParameter(
              CallTrampolineDescriptor::kActualArgumentsCount),
          Immediate(arg_count));

  // TODO(leszeks): This doesn't collect feedback yet, either pass in the
  // feedback vector by Handle.
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

  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}

void Construct::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = ConstructStubDescriptor;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseFixed(new_target(), D::GetRegisterParameter(D::kNewTarget));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void Construct::GenerateCode(MaglevCodeGenState* code_gen_state,
                             const ProcessingState& state) {
  using D = ConstructStubDescriptor;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(new_target()), D::GetRegisterParameter(D::kNewTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);

  for (int i = num_args() - 1; i >= 0; --i) {
    PushInput(code_gen_state, arg(i));
  }

  uint32_t arg_count = num_args();
  __ Move(D::GetRegisterParameter(D::kActualArgumentsCount),
          Immediate(arg_count));

  __ CallBuiltin(Builtin::kConstruct);

  code_gen_state->DefineLazyDeoptPoint(lazy_deopt_info());
}

void IncreaseInterruptBudget::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  set_temporaries_needed(1);
}
void IncreaseInterruptBudget::GenerateCode(MaglevCodeGenState* code_gen_state,
                                           const ProcessingState& state) {
  Register scratch = temporaries().first();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      scratch, FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ addl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount()));
}
void IncreaseInterruptBudget::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << amount() << ")";
}

void ReduceInterruptBudget::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  set_temporaries_needed(1);
}
void ReduceInterruptBudget::GenerateCode(MaglevCodeGenState* code_gen_state,
                                         const ProcessingState& state) {
  Register scratch = temporaries().first();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      scratch, FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ subl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount()));
  JumpToDeferredIf(
      less, code_gen_state,
      [](MaglevCodeGenState* code_gen_state, Label* return_label,
         ReduceInterruptBudget* node) {
        {
          SaveRegisterStateForCall save_register_state(
              code_gen_state, node->register_snapshot());
          __ Move(kContextRegister, code_gen_state->native_context().object());
          __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
          __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck, 1);
          save_register_state.DefineSafepoint();
        }
        __ jmp(return_label);
      },
      this);
}
void ReduceInterruptBudget::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << amount() << ")";
}

namespace {

void AttemptOnStackReplacement(MaglevCodeGenState* code_gen_state,
                               int32_t loop_depth, FeedbackSlot feedback_slot) {
  // TODO(v8:7700): Implement me. See also
  // InterpreterAssembler::OnStackReplacement.
}

}  // namespace

// ---
// Control nodes
// ---
void Return::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseFixed(value_input(), kReturnRegister0);
}
void Return::GenerateCode(MaglevCodeGenState* code_gen_state,
                          const ProcessingState& state) {
  DCHECK_EQ(ToRegister(value_input()), kReturnRegister0);

  // Read the formal number of parameters from the top level compilation unit
  // (i.e. the outermost, non inlined function).
  int formal_params_size = code_gen_state->compilation_info()
                               ->toplevel_compilation_unit()
                               ->parameter_count();

  // We're not going to continue execution, so we can use an arbitrary register
  // here instead of relying on temporaries from the register allocator.
  Register actual_params_size = r8;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to re-use the
  // incoming argc's register (if it's still valid).
  __ movq(actual_params_size,
          MemOperand(rbp, StandardFrameConstants::kArgCOffset));

  // Leave the frame.
  // TODO(leszeks): Add a new frame maker for Maglev.
  __ LeaveFrame(StackFrame::BASELINE);

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label drop_dynamic_arg_size;
  __ cmpq(actual_params_size, Immediate(formal_params_size));
  __ j(greater, &drop_dynamic_arg_size);

  // Drop receiver + arguments according to static formal arguments size.
  __ Ret(formal_params_size * kSystemPointerSize, kScratchRegister);

  __ bind(&drop_dynamic_arg_size);
  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(actual_params_size, r9, TurboAssembler::kCountIsInteger,
                   TurboAssembler::kCountIncludesReceiver);
  __ Ret();
}

void Deopt::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void Deopt::GenerateCode(MaglevCodeGenState* code_gen_state,
                         const ProcessingState& state) {
  EmitEagerDeopt(code_gen_state, this, reason());
}
void Deopt::PrintParams(std::ostream& os,
                        MaglevGraphLabeller* graph_labeller) const {
  os << "(" << DeoptimizeReasonToString(reason()) << ")";
}

void Jump::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void Jump::GenerateCode(MaglevCodeGenState* code_gen_state,
                        const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ jmp(target()->label());
  }
}

void JumpToInlined::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void JumpToInlined::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ jmp(target()->label());
  }
}
void JumpToInlined::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << Brief(*unit()->shared_function_info().object()) << ")";
}

void JumpFromInlined::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void JumpFromInlined::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ jmp(target()->label());
  }
}

void JumpLoop::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void JumpLoop::GenerateCode(MaglevCodeGenState* code_gen_state,
                            const ProcessingState& state) {
  AttemptOnStackReplacement(code_gen_state, loop_depth_, feedback_slot_);

  __ jmp(target()->label());
}

void BranchIfTrue::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(condition_input());
}
void BranchIfTrue::GenerateCode(MaglevCodeGenState* code_gen_state,
                                const ProcessingState& state) {
  Register value = ToRegister(condition_input());

  auto* next_block = state.next_block();

  // We don't have any branch probability information, so try to jump
  // over whatever the next block emitted is.
  if (if_false() == next_block) {
    // Jump over the false block if true, otherwise fall through into it.
    __ JumpIfRoot(value, RootIndex::kTrueValue, if_true()->label());
  } else {
    // Jump to the false block if true.
    __ JumpIfNotRoot(value, RootIndex::kTrueValue, if_false()->label());
    // Jump to the true block if it's not the next block.
    if (if_true() != next_block) {
      __ jmp(if_true()->label());
    }
  }
}

void BranchIfInt32Compare::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfInt32Compare::GenerateCode(MaglevCodeGenState* code_gen_state,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());

  auto* next_block = state.next_block();

  __ cmpl(left, right);

  // We don't have any branch probability information, so try to jump
  // over whatever the next block emitted is.
  if (if_false() == next_block) {
    // Jump over the false block if true, otherwise fall through into it.
    __ j(ConditionFor(operation_), if_true()->label());
  } else {
    // Jump to the false block if true.
    __ j(NegateCondition(ConditionFor(operation_)), if_false()->label());
    // Jump to the true block if it's not the next block.
    if (if_true() != next_block) {
      __ jmp(if_true()->label());
    }
  }
}
void BranchIfFloat64Compare::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << operation_ << ")";
}

void BranchIfFloat64Compare::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfFloat64Compare::GenerateCode(MaglevCodeGenState* code_gen_state,
                                          const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());

  auto* next_block = state.next_block();

  __ Ucomisd(left, right);

  // We don't have any branch probability information, so try to jump
  // over whatever the next block emitted is.
  if (if_false() == next_block) {
    // Jump over the false block if true, otherwise fall through into it.
    __ j(ConditionFor(operation_), if_true()->label());
  } else {
    // Jump to the false block if true.
    __ j(NegateCondition(ConditionFor(operation_)), if_false()->label());
    // Jump to the true block if it's not the next block.
    if (if_true() != next_block) {
      __ jmp(if_true()->label());
    }
  }
}
void BranchIfInt32Compare::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << operation_ << ")";
}

void BranchIfReferenceCompare::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfReferenceCompare::GenerateCode(MaglevCodeGenState* code_gen_state,
                                            const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());

  auto* next_block = state.next_block();
  __ cmp_tagged(left, right);
  // We don't have any branch probability information, so try to jump
  // over whatever the next block emitted is.
  if (if_false() == next_block) {
    // Jump over the false block if true, otherwise fall through into it.
    __ j(ConditionFor(operation_), if_true()->label());
  } else {
    // Jump to the false block if true.
    __ j(NegateCondition(ConditionFor(operation_)), if_false()->label());
    // Jump to the true block if it's not the next block.
    if (if_true() != next_block) {
      __ jmp(if_true()->label());
    }
  }
}
void BranchIfReferenceCompare::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << operation_ << ")";
}

void BranchIfToBooleanTrue::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseFixed(condition_input(),
           ToBooleanForBaselineJumpDescriptor::GetRegisterParameter(0));
}
void BranchIfToBooleanTrue::GenerateCode(MaglevCodeGenState* code_gen_state,
                                         const ProcessingState& state) {
  DCHECK_EQ(ToRegister(condition_input()),
            ToBooleanForBaselineJumpDescriptor::GetRegisterParameter(0));

  // ToBooleanForBaselineJump returns the ToBoolean value into return reg 1, and
  // the original value into kInterpreterAccumulatorRegister, so we don't have
  // to worry about it getting clobbered.
  __ CallBuiltin(Builtin::kToBooleanForBaselineJump);
  __ SmiCompare(kReturnRegister1, Smi::zero());

  auto* next_block = state.next_block();

  // We don't have any branch probability information, so try to jump
  // over whatever the next block emitted is.
  if (if_false() == next_block) {
    // Jump over the false block if non zero, otherwise fall through into it.
    __ j(not_equal, if_true()->label());
  } else {
    // Jump to the false block if zero.
    __ j(equal, if_false()->label());
    // Fall through or jump to the true block.
    if (if_true() != next_block) {
      __ jmp(if_true()->label());
    }
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
