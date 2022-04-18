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
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
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
#define UNSUPPORTED()                                                     \
  do {                                                                    \
    std::cerr << "Maglev: Can't compile, unsuppored codegen path.\n";     \
    code_gen_state->set_found_unsupported_code_paths(true);               \
    g_this_field_will_be_unused_once_all_code_paths_are_supported = true; \
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

// TODO(victorgomes): Use this for smi binary operation and remove attribute
// [[maybe_unused]].
[[maybe_unused]] void DefineSameAsFirst(MaglevVregAllocationState* vreg_state,
                                        Node* node) {
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
class DeferredCodeInfoImpl final : public MaglevCodeGenState::DeferredCodeInfo {
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

void EmitDeopt(MaglevCodeGenState* code_gen_state, Node* node,
               int deopt_bytecode_position,
               const InterpreterFrameState* checkpoint_state) {
  DCHECK(node->properties().can_deopt());
  // TODO(leszeks): Extract to separate call, or at the very least defer.

  // TODO(leszeks): Stack check.
  MaglevCompilationUnit* compilation_unit = code_gen_state->compilation_unit();
  int maglev_frame_size = code_gen_state->vreg_slots();

  ASM_CODE_COMMENT_STRING(code_gen_state->masm(), "Deoptimize");
  __ RecordComment("Push registers and load accumulator");
  int num_saved_slots = 0;
  // TODO(verwaest): We probably shouldn't be spilling all values that go
  // through deopt :)
  for (int i = 0; i < compilation_unit->register_count(); ++i) {
    ValueNode* node = checkpoint_state->get(interpreter::Register(i));
    if (node == nullptr) continue;
    __ Push(ToMemOperand(node->spill_slot()));
    num_saved_slots++;
  }
  ValueNode* accumulator = checkpoint_state->accumulator();
  if (accumulator) {
    __ movq(kInterpreterAccumulatorRegister,
            ToMemOperand(accumulator->spill_slot()));
  }

  __ RecordComment("Load registers from extra pushed slots");
  int slot = 0;
  for (int i = 0; i < compilation_unit->register_count(); ++i) {
    ValueNode* node = checkpoint_state->get(interpreter::Register(i));
    if (node == nullptr) continue;
    __ movq(kScratchRegister, MemOperand(rsp, (num_saved_slots - slot++ - 1) *
                                                  kSystemPointerSize));
    __ movq(MemOperand(rbp, InterpreterFrameConstants::kRegisterFileFromFp -
                                i * kSystemPointerSize),
            kScratchRegister);
  }
  DCHECK_EQ(slot, num_saved_slots);

  __ RecordComment("Materialize bytecode array and offset");
  __ Move(MemOperand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp),
          compilation_unit->bytecode().object());
  __ Move(MemOperand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp),
          Smi::FromInt(deopt_bytecode_position +
                       (BytecodeArray::kHeaderSize - kHeapObjectTag)));

  // Reset rsp to bytecode sized frame.
  __ addq(rsp, Immediate((maglev_frame_size + num_saved_slots -
                          (2 + compilation_unit->register_count())) *
                         kSystemPointerSize));
  __ TailCallBuiltin(Builtin::kBaselineOrInterpreterEnterAtBytecode);
}

void EmitDeopt(MaglevCodeGenState* code_gen_state, Node* node,
               const ProcessingState& state) {
  EmitDeopt(code_gen_state, node, state.checkpoint()->bytecode_position(),
            state.checkpoint_frame_state());
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

void Checkpoint::AllocateVreg(MaglevVregAllocationState* vreg_state,
                              const ProcessingState& state) {}
void Checkpoint::GenerateCode(MaglevCodeGenState* code_gen_state,
                              const ProcessingState& state) {}
void Checkpoint::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << PrintNodeLabel(graph_labeller, accumulator()) << ")";
}

void SoftDeopt::AllocateVreg(MaglevVregAllocationState* vreg_state,
                             const ProcessingState& state) {}
void SoftDeopt::GenerateCode(MaglevCodeGenState* code_gen_state,
                             const ProcessingState& state) {
  EmitDeopt(code_gen_state, this, state);
}

void Constant::AllocateVreg(MaglevVregAllocationState* vreg_state,
                            const ProcessingState& state) {
  DefineAsRegister(vreg_state, this);
}
void Constant::GenerateCode(MaglevCodeGenState* code_gen_state,
                            const ProcessingState& state) {
  UNREACHABLE();
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
  if (map().object()->is_migration_target()) {
    JumpToDeferredIf(
        not_equal, code_gen_state,
        [](MaglevCodeGenState* code_gen_state, Label* return_label,
           Register object, CheckMaps* node, int checkpoint_position,
           const InterpreterFrameState* checkpoint_state_snapshot,
           Register map_tmp) {
          Label deopt;

          // If the map is not deprecated, deopt straight away.
          __ movl(kScratchRegister,
                  FieldOperand(map_tmp, Map::kBitField3Offset));
          __ testl(kScratchRegister,
                   Immediate(Map::Bits3::IsDeprecatedBit::kMask));
          __ j(zero, &deopt);

          // Otherwise, try migrating the object. If the migration returns Smi
          // zero, then it failed and we should deopt.
          __ Push(object);
          __ Move(kContextRegister,
                  code_gen_state->broker()->target_native_context().object());
          // TODO(verwaest): We're calling so we need to spill around it.
          __ CallRuntime(Runtime::kTryMigrateInstance);
          __ cmpl(kReturnRegister0, Immediate(0));
          __ j(equal, &deopt);

          // The migrated object is returned on success, retry the map check.
          __ Move(object, kReturnRegister0);
          __ LoadMap(map_tmp, object);
          __ Cmp(map_tmp, node->map().object());
          __ j(equal, return_label);

          __ bind(&deopt);
          EmitDeopt(code_gen_state, node, checkpoint_position,
                    checkpoint_state_snapshot);
        },
        object, this, state.checkpoint()->bytecode_position(),
        state.checkpoint_frame_state(), map_tmp);
  } else {
    Label is_ok;
    __ j(equal, &is_ok);
    EmitDeopt(code_gen_state, this, state);
    __ bind(&is_ok);
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
  int handler = this->handler();

  if (LoadHandler::IsInobjectBits::decode(handler)) {
    Operand input_field_operand = FieldOperand(
        object, LoadHandler::FieldIndexBits::decode(handler) * kTaggedSize);
    __ DecompressAnyTagged(ToRegister(result()), input_field_operand);
    if (LoadHandler::IsDoubleBits::decode(handler)) {
      // TODO(leszeks): Copy out the value, either as a double or a HeapNumber.
      UNSUPPORTED();
    }
  } else {
    // TODO(leszeks): Handle out-of-object properties.
    UNSUPPORTED();
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
    UNSUPPORTED();
  }
}

void StoreField::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << std::hex << handler() << std::dec << ")";
}

void LoadNamedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                    const ProcessingState& state) {
  using D = LoadNoFeedbackDescriptor;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void LoadNamedGeneric::GenerateCode(MaglevCodeGenState* code_gen_state,
                                    const ProcessingState& state) {
  using D = LoadNoFeedbackDescriptor;
  const int ic_kind = static_cast<int>(FeedbackSlotKind::kLoadProperty);
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  __ Move(D::GetRegisterParameter(D::kName), name().object());
  __ Move(D::GetRegisterParameter(D::kICKind),
          Immediate(Smi::FromInt(ic_kind)));
  __ CallBuiltin(Builtin::kLoadIC_NoFeedback);
}
void LoadNamedGeneric::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void StoreToFrame::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                const ProcessingState& state) {}
void StoreToFrame::GenerateCode(MaglevCodeGenState* code_gen_state,
                                const ProcessingState& state) {}
void StoreToFrame::PrintParams(std::ostream& os,
                               MaglevGraphLabeller* graph_labeller) const {
  os << "(" << target().ToString() << " ← "
     << PrintNodeLabel(graph_labeller, value()) << ")";
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
                       const ProcessingState& state) {
  DCHECK_EQ(state.interpreter_frame_state()->get(owner()), this);
}
void Phi::PrintParams(std::ostream& os,
                      MaglevGraphLabeller* graph_labeller) const {
  os << "(" << owner().ToString() << ")";
}

void CallProperty::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                const ProcessingState& state) {
  UseFixed(function(), CallTrampolineDescriptor::GetRegisterParameter(
                           CallTrampolineDescriptor::kFunction));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CallProperty::GenerateCode(MaglevCodeGenState* code_gen_state,
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
  __ CallBuiltin(Builtin::kCall_ReceiverIsNotNullOrUndefined);
}

void CallUndefinedReceiver::AllocateVreg(MaglevVregAllocationState* vreg_state,
                                         const ProcessingState& state) {
  UNREACHABLE();
}
void CallUndefinedReceiver::GenerateCode(MaglevCodeGenState* code_gen_state,
                                         const ProcessingState& state) {
  UNREACHABLE();
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

  __ LeaveFrame(StackFrame::BASELINE);
  __ Ret(code_gen_state->parameter_count() * kSystemPointerSize,
         kScratchRegister);
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
