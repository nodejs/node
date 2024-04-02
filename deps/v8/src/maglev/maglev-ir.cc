// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-ir.h"

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register.h"
#include "src/compiler/backend/instruction.h"
#include "src/ic/handler-configuration.h"
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

// TODO(v8:7700): Clean up after all code paths are supported.
static bool g_this_field_will_be_unused_once_all_code_paths_are_supported;
#define UNSUPPORTED(REASON)                                                \
  do {                                                                     \
    std::cerr << "Maglev: Can't compile, unsuppored codegen path (" REASON \
                 ")\n";                                                    \
    code_gen_state->set_found_unsupported_code_paths(true);                \
    g_this_field_will_be_unused_once_all_code_paths_are_supported = true;  \
  } while (false)

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

// ---
// Code gen helpers.
// ---

void PushInput(MaglevCodeGenState* code_gen_state, const Input& input) {
  // TODO(leszeks): Consider special casing the value. (Toon: could possibly
  // be done through Input directly?)
  const compiler::AllocatedOperand& operand =
      compiler::AllocatedOperand::cast(input.operand());

  if (operand.IsRegister()) {
    __ Push(operand.GetRegister());
  } else {
    DCHECK(operand.IsStackSlot());
    __ Push(GetStackSlot(operand));
  }
}

// ---
// Deferred code handling.
// ---

// Base case provides an error.
template <typename T, typename Enable = void>
struct CopyForDeferredHelper {
  template <typename U>
  struct No_Copy_Helper_Implemented_For_Type;
  static void Copy(MaglevCompilationUnit* compilation_unit,
                   No_Copy_Helper_Implemented_For_Type<T>);
};

// Helper for copies by value.
template <typename T, typename Enable = void>
struct CopyForDeferredByValue {
  static T Copy(MaglevCompilationUnit* compilation_unit, T node) {
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
// MaglevCompilationUnits are copied by value.
template <>
struct CopyForDeferredHelper<MaglevCompilationUnit*>
    : public CopyForDeferredByValue<MaglevCompilationUnit*> {};
// Machine registers are copied by value.
template <>
struct CopyForDeferredHelper<Register>
    : public CopyForDeferredByValue<Register> {};
// Bytecode offsets are copied by value.
template <>
struct CopyForDeferredHelper<BytecodeOffset>
    : public CopyForDeferredByValue<BytecodeOffset> {};

// InterpreterFrameState is cloned.
template <>
struct CopyForDeferredHelper<const InterpreterFrameState*> {
  static const InterpreterFrameState* Copy(
      MaglevCompilationUnit* compilation_unit,
      const InterpreterFrameState* frame_state) {
    return compilation_unit->zone()->New<InterpreterFrameState>(
        *compilation_unit, *frame_state);
  }
};
// EagerDeoptInfo pointers are copied by value.
template <>
struct CopyForDeferredHelper<EagerDeoptInfo*>
    : public CopyForDeferredByValue<EagerDeoptInfo*> {};

template <typename T>
T CopyForDeferred(MaglevCompilationUnit* compilation_unit, T&& value) {
  return CopyForDeferredHelper<T>::Copy(compilation_unit,
                                        std::forward<T>(value));
}

template <typename T>
T CopyForDeferred(MaglevCompilationUnit* compilation_unit, T& value) {
  return CopyForDeferredHelper<T>::Copy(compilation_unit, value);
}

template <typename T>
T CopyForDeferred(MaglevCompilationUnit* compilation_unit, const T& value) {
  return CopyForDeferredHelper<T>::Copy(compilation_unit, value);
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
  explicit DeferredCodeInfoImpl(MaglevCompilationUnit* compilation_unit,
                                FunctionPointer function, InArgs&&... args)
      : function(function),
        args(CopyForDeferred(compilation_unit, std::forward<InArgs>(args))...) {
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
void JumpToDeferredIf(Condition cond, MaglevCodeGenState* code_gen_state,
                      Function&& deferred_code_gen, Args&&... args) {
  using DeferredCodeInfoT = DeferredCodeInfoImpl<Function>;
  DeferredCodeInfoT* deferred_code =
      code_gen_state->compilation_unit()->zone()->New<DeferredCodeInfoT>(
          code_gen_state->compilation_unit(), deferred_code_gen,
          std::forward<Args>(args)...);

  code_gen_state->PushDeferredCode(deferred_code);
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
                        EagerDeoptInfo* deopt_info) {
  if (deopt_info->deopt_entry_label.is_unused()) {
    code_gen_state->PushEagerDeopt(deopt_info);
  }
}

void EmitEagerDeoptIf(Condition cond, MaglevCodeGenState* code_gen_state,
                      EagerDeoptInfo* deopt_info) {
  RegisterEagerDeopt(code_gen_state, deopt_info);
  __ RecordComment("-- Jump to eager deopt");
  __ j(cond, &deopt_info->deopt_entry_label);
}

template <typename NodeT>
void EmitEagerDeoptIf(Condition cond, MaglevCodeGenState* code_gen_state,
                      NodeT* node) {
  STATIC_ASSERT(NodeT::kProperties.can_eager_deopt());
  EmitEagerDeoptIf(cond, code_gen_state, node->eager_deopt_info());
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

DeoptInfo::DeoptInfo(Zone* zone, const MaglevCompilationUnit& compilation_unit,
                     CheckpointedInterpreterState state)
    : state(state),
      input_locations(zone->NewArray<InputLocation>(
          state.register_frame->size(compilation_unit))) {
  // Default initialise if we're printing the graph, to avoid printing junk
  // values.
  if (FLAG_print_maglev_graph) {
    for (size_t i = 0; i < state.register_frame->size(compilation_unit); ++i) {
      new (&input_locations[i]) InputLocation();
    }
  }
}

// ---
// Nodes
// ---
void SmiConstant::AllocateVreg(MaglevVregAllocationState* vreg_state,
                               const ProcessingState& state) {
  DefineAsRegister(vreg_state, this);
}
void SmiConstant::GenerateCode(MaglevCodeGenState* code_gen_state,
                               const ProcessingState& state) {
  __ Move(ToRegister(result()), Immediate(value()));
}
void SmiConstant::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Constant::AllocateVreg(MaglevVregAllocationState* vreg_state,
                            const ProcessingState& state) {
  DefineAsRegister(vreg_state, this);
}
void Constant::GenerateCode(MaglevCodeGenState* code_gen_state,
                            const ProcessingState& state) {
  __ Move(ToRegister(result()), object_.object());
}
void Constant::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {
  os << "(" << object_ << ")";
}

void InitialValue::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                const ProcessingState& state) {
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

void LoadGlobal::AllocateVreg(MaglevVregAllocationState* vreg_state,
                              const ProcessingState& state) {
  UseFixed(context(), kContextRegister);
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void LoadGlobal::GenerateCode(MaglevCodeGenState* code_gen_state,
                              const ProcessingState& state) {
  // TODO(leszeks): Port the nice Sparkplug CallBuiltin helper.

  DCHECK_EQ(ToRegister(context()), kContextRegister);

  // TODO(jgruber): Detect properly.
  const int ic_kind =
      static_cast<int>(FeedbackSlotKind::kLoadGlobalNotInsideTypeof);

  __ Move(LoadGlobalNoFeedbackDescriptor::GetRegisterParameter(
              LoadGlobalNoFeedbackDescriptor::kName),
          name().object());
  __ Move(LoadGlobalNoFeedbackDescriptor::GetRegisterParameter(
              LoadGlobalNoFeedbackDescriptor::kICKind),
          Immediate(Smi::FromInt(ic_kind)));

  // TODO(jgruber): Implement full LoadGlobal handling.
  __ CallBuiltin(Builtin::kLoadGlobalIC_NoFeedback);
}
void LoadGlobal::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name() << ")";
}

void RegisterInput::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                 const ProcessingState& state) {
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

void RootConstant::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                const ProcessingState& state) {
  DefineAsRegister(vreg_state, this);
}
void RootConstant::GenerateCode(MaglevCodeGenState* code_gen_state,
                                const ProcessingState& state) {
  if (!has_valid_live_range()) return;

  Register reg = ToRegister(result());
  __ LoadRoot(reg, index());
}
void RootConstant::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  os << "(" << RootsTable::name(index()) << ")";
}

void CheckMaps::AllocateVreg(MaglevVregAllocationState* vreg_state,
                             const ProcessingState& state) {
  UseRegister(actual_map_input());
  set_temporaries_needed(1);
}
void CheckMaps::GenerateCode(MaglevCodeGenState* code_gen_state,
                             const ProcessingState& state) {
  Register object = ToRegister(actual_map_input());
  RegList temps = temporaries();
  Register map_tmp = temps.PopFirst();

  __ LoadMap(map_tmp, object);
  __ Cmp(map_tmp, map().object());

  // TODO(leszeks): Encode as a bit on CheckMaps.
  if (map().is_migration_target()) {
    JumpToDeferredIf(
        not_equal, code_gen_state,
        [](MaglevCodeGenState* code_gen_state, Label* return_label,
           Register object, CheckMaps* node, EagerDeoptInfo* deopt_info,
           Register map_tmp) {
          RegisterEagerDeopt(code_gen_state, deopt_info);

          // If the map is not deprecated, deopt straight away.
          __ movl(kScratchRegister,
                  FieldOperand(map_tmp, Map::kBitField3Offset));
          __ testl(kScratchRegister,
                   Immediate(Map::Bits3::IsDeprecatedBit::kMask));
          __ j(zero, &deopt_info->deopt_entry_label);

          // Otherwise, try migrating the object. If the migration returns Smi
          // zero, then it failed and we should deopt.
          __ Push(object);
          __ Move(kContextRegister,
                  code_gen_state->broker()->target_native_context().object());
          // TODO(verwaest): We're calling so we need to spill around it.
          __ CallRuntime(Runtime::kTryMigrateInstance);
          __ cmpl(kReturnRegister0, Immediate(0));
          __ j(equal, &deopt_info->deopt_entry_label);

          // The migrated object is returned on success, retry the map check.
          __ Move(object, kReturnRegister0);
          __ LoadMap(map_tmp, object);
          __ Cmp(map_tmp, node->map().object());
          __ j(equal, return_label);
          __ jmp(&deopt_info->deopt_entry_label);
        },
        object, this, eager_deopt_info(), map_tmp);
  } else {
    EmitEagerDeoptIf(not_equal, code_gen_state, this);
  }
}
void CheckMaps::PrintParams(std::ostream& os,
                            MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *map().object() << ")";
}

void LoadField::AllocateVreg(MaglevVregAllocationState* vreg_state,
                             const ProcessingState& state) {
  UseRegister(object_input());
  DefineAsRegister(vreg_state, this);
}
void LoadField::GenerateCode(MaglevCodeGenState* code_gen_state,
                             const ProcessingState& state) {
  // os << "kField, is in object = "
  //    << LoadHandler::IsInobjectBits::decode(raw_handler)
  //    << ", is double = " << LoadHandler::IsDoubleBits::decode(raw_handler)
  //    << ", field index = " <<
  //    LoadHandler::FieldIndexBits::decode(raw_handler);

  Register object = ToRegister(object_input());
  Register res = ToRegister(result());
  int handler = this->handler();

  if (LoadHandler::IsInobjectBits::decode(handler)) {
    Operand input_field_operand = FieldOperand(
        object, LoadHandler::FieldIndexBits::decode(handler) * kTaggedSize);
    __ DecompressAnyTagged(res, input_field_operand);
  } else {
    Operand property_array_operand =
        FieldOperand(object, JSReceiver::kPropertiesOrHashOffset);
    __ DecompressAnyTagged(res, property_array_operand);

    __ AssertNotSmi(res);

    Operand input_field_operand = FieldOperand(
        res, LoadHandler::FieldIndexBits::decode(handler) * kTaggedSize);
    __ DecompressAnyTagged(res, input_field_operand);
  }

  if (LoadHandler::IsDoubleBits::decode(handler)) {
    // TODO(leszeks): Copy out the value, either as a double or a HeapNumber.
    UNSUPPORTED("LoadField double property");
  }
}
void LoadField::PrintParams(std::ostream& os,
                            MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << handler() << std::dec << ")";
}

void StoreField::AllocateVreg(MaglevVregAllocationState* vreg_state,
                              const ProcessingState& state) {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreField::GenerateCode(MaglevCodeGenState* code_gen_state,
                              const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register value = ToRegister(value_input());

  if (StoreHandler::IsInobjectBits::decode(this->handler())) {
    Operand operand = FieldOperand(
        object,
        StoreHandler::FieldIndexBits::decode(this->handler()) * kTaggedSize);
    __ StoreTaggedField(operand, value);
  } else {
    // TODO(victorgomes): Out-of-object properties.
    UNSUPPORTED("StoreField out-of-object property");
  }
}

void StoreField::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << handler() << std::dec << ")";
}

void LoadNamedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                    const ProcessingState& state) {
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
}
void LoadNamedGeneric::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void GapMove::AllocateVreg(MaglevVregAllocationState* vreg_state,
                           const ProcessingState& state) {
  UNREACHABLE();
}
void GapMove::GenerateCode(MaglevCodeGenState* code_gen_state,
                           const ProcessingState& state) {
  if (source().IsAnyRegister()) {
    Register source_reg = ToRegister(source());
    if (target().IsAnyRegister()) {
      __ movq(ToRegister(target()), source_reg);
    } else {
      __ movq(ToMemOperand(target()), source_reg);
    }
  } else {
    MemOperand source_op = ToMemOperand(source());
    if (target().IsAnyRegister()) {
      __ movq(ToRegister(target()), source_op);
    } else {
      __ movq(kScratchRegister, source_op);
      __ movq(ToMemOperand(target()), kScratchRegister);
    }
  }
}
void GapMove::PrintParams(std::ostream& os,
                          MaglevGraphLabeller* graph_labeller) const {
  os << "(" << source() << " → " << target() << ")";
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
    MaglevVregAllocationState* vreg_state, const ProcessingState& state) {
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
}

template <class Derived, Operation kOperation>
void BinaryWithFeedbackNode<Derived, kOperation>::AllocateVreg(
    MaglevVregAllocationState* vreg_state, const ProcessingState& state) {
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
}

#define DEF_OPERATION(Name)                                      \
  void Name::AllocateVreg(MaglevVregAllocationState* vreg_state, \
                          const ProcessingState& state) {        \
    Base::AllocateVreg(vreg_state, state);                       \
  }                                                              \
  void Name::GenerateCode(MaglevCodeGenState* code_gen_state,    \
                          const ProcessingState& state) {        \
    Base::GenerateCode(code_gen_state, state);                   \
  }
GENERIC_OPERATIONS_NODE_LIST(DEF_OPERATION)
#undef DEF_OPERATION

void CheckedSmiUntag::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                   const ProcessingState& state) {
  UseRegister(input());
  DefineSameAsFirst(vreg_state, this);
}

void CheckedSmiUntag::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  Register value = ToRegister(input());
  // TODO(leszeks): Consider optimizing away this test and using the carry bit
  // of the `sarl` for cases where the deopt uses the value from a different
  // register.
  __ testb(value, Immediate(1));
  EmitEagerDeoptIf(not_zero, code_gen_state, this);
  __ sarl(value, Immediate(1));
}

void CheckedSmiTag::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                 const ProcessingState& state) {
  UseRegister(input());
  DefineSameAsFirst(vreg_state, this);
}

void CheckedSmiTag::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {
  Register reg = ToRegister(input());
  __ addl(reg, reg);
  EmitEagerDeoptIf(overflow, code_gen_state, this);
}

void Int32Constant::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                 const ProcessingState& state) {
  DefineAsRegister(vreg_state, this);
}
void Int32Constant::GenerateCode(MaglevCodeGenState* code_gen_state,
                                 const ProcessingState& state) {
  __ Move(ToRegister(result()), Immediate(value()));
}
void Int32Constant::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Int32AddWithOverflow::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                        const ProcessingState& state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32AddWithOverflow::GenerateCode(MaglevCodeGenState* code_gen_state,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ addl(left, right);
  EmitEagerDeoptIf(overflow, code_gen_state, this);
}

void Phi::AllocateVreg(MaglevVregAllocationState* vreg_state,
                       const ProcessingState& state) {
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

void Call::AllocateVreg(MaglevVregAllocationState* vreg_state,
                        const ProcessingState& state) {
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

  lazy_deopt_info()->deopting_call_return_pc = __ pc_offset_for_safepoint();
  code_gen_state->PushLazyDeopt(lazy_deopt_info());

  SafepointTableBuilder::Safepoint safepoint =
      code_gen_state->safepoint_table_builder()->DefineSafepoint(
          code_gen_state->masm());
  code_gen_state->DefineSafepointStackSlots(safepoint);
}

// ---
// Control nodes
// ---
void Return::AllocateVreg(MaglevVregAllocationState* vreg_state,
                          const ProcessingState& state) {
  UseFixed(value_input(), kReturnRegister0);
}
void Return::GenerateCode(MaglevCodeGenState* code_gen_state,
                          const ProcessingState& state) {
  DCHECK_EQ(ToRegister(value_input()), kReturnRegister0);

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
  __ cmpq(actual_params_size, Immediate(code_gen_state->parameter_count()));
  __ j(greater, &drop_dynamic_arg_size);

  // Drop receiver + arguments according to static formal arguments size.
  __ Ret(code_gen_state->parameter_count() * kSystemPointerSize,
         kScratchRegister);

  __ bind(&drop_dynamic_arg_size);
  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(actual_params_size, r9, TurboAssembler::kCountIsInteger,
                   TurboAssembler::kCountIncludesReceiver);
  __ Ret();
}

void Deopt::AllocateVreg(MaglevVregAllocationState* vreg_state,
                         const ProcessingState& state) {}
void Deopt::GenerateCode(MaglevCodeGenState* code_gen_state,
                         const ProcessingState& state) {
  EmitEagerDeoptIf(always, code_gen_state, this);
}

void Jump::AllocateVreg(MaglevVregAllocationState* vreg_state,
                        const ProcessingState& state) {}
void Jump::GenerateCode(MaglevCodeGenState* code_gen_state,
                        const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ jmp(target()->label());
  }
}

void JumpLoop::AllocateVreg(MaglevVregAllocationState* vreg_state,
                            const ProcessingState& state) {}
void JumpLoop::GenerateCode(MaglevCodeGenState* code_gen_state,
                            const ProcessingState& state) {
  __ jmp(target()->label());
}

void BranchIfTrue::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                const ProcessingState& state) {
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

void BranchIfCompare::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                   const ProcessingState& state) {}
void BranchIfCompare::GenerateCode(MaglevCodeGenState* code_gen_state,
                                   const ProcessingState& state) {
  USE(operation_);
  UNREACHABLE();
}

void BranchIfToBooleanTrue::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                         const ProcessingState& state) {
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
