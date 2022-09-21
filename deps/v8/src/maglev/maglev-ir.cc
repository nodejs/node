// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-ir.h"

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/baseline/baseline-assembler-inl.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/maglev-safepoint-table.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/ic/handler-configuration.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-vreg-allocator.h"
#include "src/objects/instance-type.h"

namespace v8 {
namespace internal {
namespace maglev {

const char* OpcodeToString(Opcode opcode) {
#define DEF_NAME(Name) #Name,
  static constexpr const char* const names[] = {NODE_BASE_LIST(DEF_NAME)};
#undef DEF_NAME
  return names[static_cast<int>(opcode)];
}

#define __ masm->

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
[[maybe_unused]] void UseFixed(Input& input, DoubleRegister reg) {
  input.SetUnallocated(compiler::UnallocatedOperand::FIXED_FP_REGISTER,
                       reg.code(), GetVirtualRegister(input.node()));
}

// ---
// Code gen helpers.
// ---

class SaveRegisterStateForCall {
 public:
  SaveRegisterStateForCall(MaglevAssembler* masm, RegisterSnapshot snapshot)
      : masm(masm), snapshot_(snapshot) {
    __ PushAll(snapshot_.live_registers);
    __ PushAll(snapshot_.live_double_registers, kDoubleSize);
  }

  ~SaveRegisterStateForCall() {
    __ PopAll(snapshot_.live_double_registers, kDoubleSize);
    __ PopAll(snapshot_.live_registers);
  }

  MaglevSafepointTableBuilder::Safepoint DefineSafepoint() {
    // TODO(leszeks): Avoid emitting safepoints when there are no registers to
    // save.
    auto safepoint = masm->safepoint_table_builder()->DefineSafepoint(masm);
    int pushed_reg_index = 0;
    for (Register reg : snapshot_.live_registers) {
      if (snapshot_.live_tagged_registers.has(reg)) {
        safepoint.DefineTaggedRegister(pushed_reg_index);
      }
      pushed_reg_index++;
    }
    int num_pushed_double_reg = snapshot_.live_double_registers.Count();
    safepoint.SetNumPushedRegisters(pushed_reg_index + num_pushed_double_reg);
    return safepoint;
  }

  MaglevSafepointTableBuilder::Safepoint DefineSafepointWithLazyDeopt(
      LazyDeoptInfo* lazy_deopt_info) {
    lazy_deopt_info->deopting_call_return_pc = masm->pc_offset_for_safepoint();
    masm->code_gen_state()->PushLazyDeopt(lazy_deopt_info);
    return DefineSafepoint();
  }

 private:
  MaglevAssembler* masm;
  RegisterSnapshot snapshot_;
};

// ---
// Inlined computations.
// ---

void AllocateRaw(MaglevAssembler* masm, RegisterSnapshot& register_snapshot,
                 Register object, int size_in_bytes,
                 AllocationType alloc_type = AllocationType::kYoung,
                 AllocationAlignment alignment = kTaggedAligned) {
  // TODO(victorgomes): Call the runtime for large object allocation.
  // TODO(victorgomes): Support double alignment.
  DCHECK_EQ(alignment, kTaggedAligned);
  if (FLAG_single_generation) {
    alloc_type = AllocationType::kOld;
  }
  bool in_new_space = alloc_type == AllocationType::kYoung;
  Isolate* isolate = masm->isolate();
  ExternalReference top =
      in_new_space
          ? ExternalReference::new_space_allocation_top_address(isolate)
          : ExternalReference::old_space_allocation_top_address(isolate);
  ExternalReference limit =
      in_new_space
          ? ExternalReference::new_space_allocation_limit_address(isolate)
          : ExternalReference::old_space_allocation_limit_address(isolate);

  ZoneLabelRef done(masm->compilation_info()->zone());
  Register new_top = kScratchRegister;
  // Check if there is enough space.
  __ Move(object, __ ExternalReferenceAsOperand(top));
  __ leaq(new_top, Operand(object, size_in_bytes));
  __ cmpq(new_top, __ ExternalReferenceAsOperand(limit));
  // Otherwise call runtime.
  __ JumpToDeferredIf(
      greater_equal,
      [](MaglevAssembler* masm, Label* return_label,
         RegisterSnapshot register_snapshot, Register object, Builtin builtin,
         int size_in_bytes, ZoneLabelRef done) {
        // Remove {object} from snapshot, since it is the returned allocated
        // HeapObject.
        register_snapshot.live_registers.clear(object);
        {
          SaveRegisterStateForCall save_register_state(masm, register_snapshot);
          using D = AllocateDescriptor;
          __ Move(D::GetRegisterParameter(D::kRequestedSize), size_in_bytes);
          __ CallBuiltin(builtin);
          save_register_state.DefineSafepoint();
          __ Move(object, kReturnRegister0);
        }
        __ jmp(*done);
      },
      register_snapshot, object,
      in_new_space ? Builtin::kAllocateRegularInYoungGeneration
                   : Builtin::kAllocateRegularInOldGeneration,
      size_in_bytes, done);
  // Store new top and tag object.
  __ movq(__ ExternalReferenceAsOperand(top), new_top);
  __ addq(object, Immediate(kHeapObjectTag));
  __ bind(*done);
}

void ToBoolean(MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
               ZoneLabelRef is_false, bool fallthrough_when_true) {
  Register map = kScratchRegister;

  // Check if {{value}} is Smi.
  __ CheckSmi(value);
  __ JumpToDeferredIf(
      zero,
      [](MaglevAssembler* masm, Label* return_label, Register value,
         ZoneLabelRef is_true, ZoneLabelRef is_false) {
        // Check if {value} is not zero.
        __ SmiCompare(value, Smi::FromInt(0));
        __ j(equal, *is_false);
        __ jmp(*is_true);
      },
      value, is_true, is_false);

  // Check if {{value}} is false.
  __ CompareRoot(value, RootIndex::kFalseValue);
  __ j(equal, *is_false);

  // Check if {{value}} is empty string.
  __ CompareRoot(value, RootIndex::kempty_string);
  __ j(equal, *is_false);

  // Check if {{value}} is undetectable.
  __ LoadMap(map, value);
  __ testl(FieldOperand(map, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsUndetectableBit::kMask));
  __ j(not_zero, *is_false);

  // Check if {{value}} is a HeapNumber.
  __ CompareRoot(map, RootIndex::kHeapNumberMap);
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Label* return_label, Register value,
         ZoneLabelRef is_true, ZoneLabelRef is_false) {
        // Sets scratch register to 0.0.
        __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
        // Sets ZF if equal to 0.0, -0.0 or NaN.
        __ Ucomisd(kScratchDoubleReg,
                   FieldOperand(value, HeapNumber::kValueOffset));
        __ j(zero, *is_false);
        __ jmp(*is_true);
      },
      value, is_true, is_false);

  // Check if {{value}} is a BigInt.
  __ CompareRoot(map, RootIndex::kBigIntMap);
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Label* return_label, Register value,
         ZoneLabelRef is_true, ZoneLabelRef is_false) {
        __ testl(FieldOperand(value, BigInt::kBitfieldOffset),
                 Immediate(BigInt::LengthBits::kMask));
        __ j(zero, *is_false);
        __ jmp(*is_true);
      },
      value, is_true, is_false);

  // Otherwise true.
  if (!fallthrough_when_true) {
    __ jmp(*is_true);
  }
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

template <typename NodeT>
void PrintImpl(std::ostream& os, MaglevGraphLabeller* graph_labeller,
               const NodeT* node, bool skip_targets) {
  os << node->opcode();
  node->PrintParams(os, graph_labeller);
  PrintInputs(os, graph_labeller, node);
  PrintResult(os, graph_labeller, node);
  if (!skip_targets) {
    PrintTargets(os, graph_labeller, node);
  }
}

}  // namespace

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

bool LazyDeoptInfo::IsResultRegister(interpreter::Register reg) const {
  if (V8_LIKELY(result_size == 1)) {
    return reg == result_location;
  }
  DCHECK_EQ(result_size, 2);
  return reg == result_location ||
         reg == interpreter::Register(result_location.index() + 1);
}

// ---
// Nodes
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
  __ movq(reg,
          masm->GetStackSlot(compiler::AllocatedOperand::cast(spill_slot())));
}
void ValueNode::DoLoadToRegister(MaglevAssembler* masm, DoubleRegister reg) {
  DCHECK(is_spilled());
  DCHECK(use_double_register());
  __ Movsd(reg,
           masm->GetStackSlot(compiler::AllocatedOperand::cast(spill_slot())));
}
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
void SmiConstant::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {}
Handle<Object> SmiConstant::DoReify(LocalIsolate* isolate) {
  return handle(value_, isolate);
}
void SmiConstant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ Move(reg, Immediate(value()));
}
void SmiConstant::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Float64Constant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsConstant(vreg_state, this);
}
void Float64Constant::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {}
Handle<Object> Float64Constant::DoReify(LocalIsolate* isolate) {
  return isolate->factory()->NewNumber<AllocationType::kOld>(value_);
}
void Float64Constant::DoLoadToRegister(MaglevAssembler* masm,
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
void Constant::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {}
void Constant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ Move(reg, object_.object());
}
Handle<Object> Constant::DoReify(LocalIsolate* isolate) {
  return object_.object();
}
void Constant::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {
  os << "(" << object_ << ")";
}

void DeleteProperty::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kDeleteProperty>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object(), D::GetRegisterParameter(D::kObject));
  UseFixed(key(), D::GetRegisterParameter(D::kKey));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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
void DeleteProperty::PrintParams(std::ostream& os,
                                 MaglevGraphLabeller* graph_labeller) const {
  os << "(" << LanguageMode2String(mode()) << ")";
}

void GeneratorStore::AllocateVreg(MaglevVregAllocationState* vreg_state) {
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
  __ LoadTaggedPointerField(
      array, FieldOperand(generator,
                          JSGeneratorObject::kParametersAndRegistersOffset));

  for (int i = 0; i < num_parameters_and_registers(); i++) {
    // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
    // register since it's a known temporary, and the write barrier slow path
    // generates better code when value == scratch. Can't use kScratchRegister
    // because CheckPageFlag uses it.
    Register value =
        __ FromAnyToRegister(parameters_and_registers(i),
                             WriteBarrierDescriptor::SlotAddressRegister());

    DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
        [](MaglevAssembler* masm, Label* return_label, Register value,
           Register array, GeneratorStore* node, int32_t offset) {
          ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
          // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
          // register, see comment above.
          __ CheckPageFlag(
              value, WriteBarrierDescriptor::SlotAddressRegister(),
              MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask,
              zero, return_label);

          Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

          __ leaq(slot_reg, FieldOperand(array, offset));

          // TODO(leszeks): Add an interface for flushing all double registers
          // before this Node, to avoid needing to save them here.
          SaveFPRegsMode const save_fp_mode =
              !node->register_snapshot().live_double_registers.is_empty()
                  ? SaveFPRegsMode::kSave
                  : SaveFPRegsMode::kIgnore;

          __ CallRecordWriteStub(array, slot_reg, save_fp_mode);

          __ jmp(return_label);
        },
        value, array, this, FixedArray::OffsetOfElementAt(i));

    __ StoreTaggedField(FieldOperand(array, FixedArray::OffsetOfElementAt(i)),
                        value);
    __ JumpIfSmi(value, &deferred_write_barrier->return_label, Label::kNear);
    // TODO(leszeks): This will stay either false or true throughout this loop.
    // Consider hoisting the check out of the loop and duplicating the loop into
    // with and without write barrier.
    __ CheckPageFlag(array, kScratchRegister,
                     MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                     &deferred_write_barrier->deferred_code_label);

    __ bind(&deferred_write_barrier->return_label);
  }

  // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
  // register, see comment above.
  Register context = __ FromAnyToRegister(
      context_input(), WriteBarrierDescriptor::SlotAddressRegister());

  DeferredCodeInfo* deferred_context_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, Label* return_label, Register context,
         Register generator, GeneratorStore* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
        // register, see comment above.
        // TODO(leszeks): The context is almost always going to be in old-space,
        // consider moving this check to the fast path, maybe even as the first
        // bailout.
        __ CheckPageFlag(
            context, WriteBarrierDescriptor::SlotAddressRegister(),
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            return_label);

        __ Move(WriteBarrierDescriptor::ObjectRegister(), generator);
        generator = WriteBarrierDescriptor::ObjectRegister();
        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

        __ leaq(slot_reg,
                FieldOperand(generator, JSGeneratorObject::kContextOffset));

        // TODO(leszeks): Add an interface for flushing all double registers
        // before this Node, to avoid needing to save them here.
        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(generator, slot_reg, save_fp_mode);

        __ jmp(return_label);
      },
      context, generator, this);
  __ StoreTaggedField(
      FieldOperand(generator, JSGeneratorObject::kContextOffset), context);
  __ AssertNotSmi(context);
  __ CheckPageFlag(generator, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_context_write_barrier->deferred_code_label);
  __ bind(&deferred_context_write_barrier->return_label);

  __ StoreTaggedSignedField(
      FieldOperand(generator, JSGeneratorObject::kContinuationOffset),
      Smi::FromInt(suspend_id()));
  __ StoreTaggedSignedField(
      FieldOperand(generator, JSGeneratorObject::kInputOrDebugPosOffset),
      Smi::FromInt(bytecode_offset()));
}

void GeneratorRestoreRegister::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(array_input());
  DefineAsRegister(vreg_state, this);
  set_temporaries_needed(1);
}
void GeneratorRestoreRegister::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register array = ToRegister(array_input());
  Register result_reg = ToRegister(result());
  Register temp = temporaries().PopFirst();

  // The input and the output can alias, if that happen we use a temporary
  // register and a move at the end.
  Register value = (array == result_reg ? temp : result_reg);

  // Loads the current value in the generator register file.
  __ DecompressAnyTagged(
      value, FieldOperand(array, FixedArray::OffsetOfElementAt(index())));

  // And trashs it with StaleRegisterConstant.
  __ LoadRoot(kScratchRegister, RootIndex::kStaleRegister);
  __ StoreTaggedField(
      FieldOperand(array, FixedArray::OffsetOfElementAt(index())),
      kScratchRegister);

  if (value != result_reg) {
    __ Move(result_reg, value);
  }
}

void ForInPrepare::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kForInPrepare>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(enumerator(), D::GetRegisterParameter(D::kEnumerator));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void ForInNext::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kForInNext>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(receiver(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(cache_array(), D::GetRegisterParameter(D::kCacheArray));
  UseFixed(cache_type(), D::GetRegisterParameter(D::kCacheType));
  UseFixed(cache_index(), D::GetRegisterParameter(D::kCacheIndex));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void ForInNext::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kForInNext>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(receiver()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(cache_array()), D::GetRegisterParameter(D::kCacheArray));
  DCHECK_EQ(ToRegister(cache_type()), D::GetRegisterParameter(D::kCacheType));
  DCHECK_EQ(ToRegister(cache_index()), D::GetRegisterParameter(D::kCacheIndex));
  __ Move(D::GetRegisterParameter(D::kSlot), Immediate(feedback().index()));
  // Feedback vector is pushed into the stack.
  DCHECK_EQ(D::GetRegisterParameterCount(), D::kFeedbackVector);
  DCHECK_EQ(D::GetStackParameterCount(), 1);
  __ Push(feedback().vector);
  __ CallBuiltin(Builtin::kForInNext);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void GetIterator::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kGetIteratorWithFeedback>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(receiver(), D::GetRegisterParameter(D::kReceiver));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void GetSecondReturnedValue::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister1);
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

void InitialValue::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  // TODO(leszeks): Make this nicer.
  result().SetUnallocated(compiler::UnallocatedOperand::FIXED_SLOT,
                          (StandardFrameConstants::kExpressionsOffset -
                           UnoptimizedFrameConstants::kRegisterFileFromFp) /
                                  kSystemPointerSize +
                              source().index(),
                          vreg_state->AllocateVirtualRegister());
}
void InitialValue::GenerateCode(MaglevAssembler* masm,
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
void LoadGlobal::PrintParams(std::ostream& os,
                             MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name() << ")";
}

void StoreGlobal::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreGlobalIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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
void StoreGlobal::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name() << ")";
}

void RegisterInput::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, input());
}
void RegisterInput::GenerateCode(MaglevAssembler* masm,
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
void RootConstant::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {}
bool RootConstant::ToBoolean(LocalIsolate* local_isolate) const {
  switch (index_) {
    case RootIndex::kFalseValue:
    case RootIndex::kNullValue:
    case RootIndex::kUndefinedValue:
    case RootIndex::kempty_string:
      return false;
    default:
      return true;
  }
}
void RootConstant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ LoadRoot(reg, index());
}
Handle<Object> RootConstant::DoReify(LocalIsolate* isolate) {
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
void CreateEmptyArrayLiteral::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  using D = CreateEmptyArrayLiteralDescriptor;
  __ Move(kContextRegister, masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), Smi::FromInt(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(Builtin::kCreateEmptyArrayLiteral);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void CreateArrayLiteral::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateArrayLiteral::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  __ Move(kContextRegister, masm->native_context().object());
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

void CreateObjectLiteral::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateObjectLiteral::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  __ Move(kContextRegister, masm->native_context().object());
  __ Push(feedback().vector);
  __ Push(TaggedIndex::FromIntptr(feedback().index()));
  __ Push(boilerplate_descriptor().object());
  __ Push(Smi::FromInt(flags()));
  __ CallRuntime(Runtime::kCreateObjectLiteral);
}

void CreateEmptyObjectLiteral::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  DefineAsRegister(vreg_state, this);
}
void CreateEmptyObjectLiteral::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register object = ToRegister(result());
  RegisterSnapshot save_registers = register_snapshot();
  AllocateRaw(masm, save_registers, object, map().instance_size());
  __ Move(kScratchRegister, map().object());
  __ StoreTaggedField(FieldOperand(object, HeapObject::kMapOffset),
                      kScratchRegister);
  __ LoadRoot(kScratchRegister, RootIndex::kEmptyFixedArray);
  __ StoreTaggedField(FieldOperand(object, JSObject::kPropertiesOrHashOffset),
                      kScratchRegister);
  __ StoreTaggedField(FieldOperand(object, JSObject::kElementsOffset),
                      kScratchRegister);
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  for (int i = 0; i < map().GetInObjectProperties(); i++) {
    int offset = map().GetInObjectPropertyOffset(i);
    __ StoreTaggedField(FieldOperand(object, offset), kScratchRegister);
  }
}

void CreateShallowObjectLiteral::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void CreateFunctionContext::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
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
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateFunctionContext::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  if (scope_type() == FUNCTION_SCOPE) {
    using D = CallInterfaceDescriptorFor<
        Builtin::kFastNewFunctionContextFunction>::type;
    DCHECK_EQ(ToRegister(context()), D::ContextRegister());
    __ Move(D::GetRegisterParameter(D::kScopeInfo), scope_info().object());
    __ Move(D::GetRegisterParameter(D::kSlots), Immediate(slot_count()));
    // TODO(leszeks): Consider inlining this allocation.
    __ CallBuiltin(Builtin::kFastNewFunctionContextFunction);
  } else {
    DCHECK_EQ(scope_type(), ScopeType::EVAL_SCOPE);
    using D =
        CallInterfaceDescriptorFor<Builtin::kFastNewFunctionContextEval>::type;
    DCHECK_EQ(ToRegister(context()), D::ContextRegister());
    __ Move(D::GetRegisterParameter(D::kScopeInfo), scope_info().object());
    __ Move(D::GetRegisterParameter(D::kSlots), Immediate(slot_count()));
    __ CallBuiltin(Builtin::kFastNewFunctionContextEval);
  }
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
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
void FastCreateClosure::PrintParams(std::ostream& os,
                                    MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *shared_function_info().object() << ", "
     << feedback_cell().object() << ")";
}

void CreateClosure::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseFixed(context(), kContextRegister);
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CreateClosure::GenerateCode(MaglevAssembler* masm,
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

void CreateRegExpLiteral::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void GetTemplateObject::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = GetTemplateObjectDescriptor;
  UseFixed(description(), D::GetRegisterParameter(D::kDescription));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void Abort::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void Abort::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  __ Push(Smi::FromInt(static_cast<int>(reason())));
  __ CallRuntime(Runtime::kAbort, 1);
  __ Trap();
}
void Abort::PrintParams(std::ostream& os,
                        MaglevGraphLabeller* graph_labeller) const {
  os << "(" << GetAbortReason(reason()) << ")";
}

namespace {
Condition ToCondition(AssertCondition cond) {
  switch (cond) {
    case AssertCondition::kLess:
      return less;
    case AssertCondition::kLessOrEqual:
      return less_equal;
    case AssertCondition::kGreater:
      return greater;
    case AssertCondition::kGeaterOrEqual:
      return greater_equal;
    case AssertCondition::kEqual:
      return equal;
    case AssertCondition::kNotEqual:
      return not_equal;
  }
}

std::ostream& operator<<(std::ostream& os, const AssertCondition cond) {
  switch (cond) {
    case AssertCondition::kLess:
      os << "Less";
      break;
    case AssertCondition::kLessOrEqual:
      os << "LessOrEqual";
      break;
    case AssertCondition::kGreater:
      os << "Greater";
      break;
    case AssertCondition::kGeaterOrEqual:
      os << "GeaterOrEqual";
      break;
    case AssertCondition::kEqual:
      os << "Equal";
      break;
    case AssertCondition::kNotEqual:
      os << "NotEqual";
      break;
  }
  return os;
}
}  // namespace

void AssertInt32::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
}
void AssertInt32::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  __ cmpq(ToRegister(left_input()), ToRegister(right_input()));
  __ Check(ToCondition(condition_), reason_);
}
void AssertInt32::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << condition_ << ")";
}

void CheckMaps::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckMaps::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  Register object = ToRegister(receiver_input());

  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
  }
  __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map().object());
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongMap, this);
}
void CheckMaps::PrintParams(std::ostream& os,
                            MaglevGraphLabeller* graph_labeller) const {
  os << "(" << *map().object() << ")";
}
void CheckSmi::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckSmi::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  __ EmitEagerDeoptIf(NegateCondition(is_smi), DeoptimizeReason::kNotASmi,
                      this);
}
void CheckSmi::PrintParams(std::ostream& os,
                           MaglevGraphLabeller* graph_labeller) const {}

void CheckNumber::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckNumber::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Label done;
  Register value = ToRegister(receiver_input());
  // If {value} is a Smi or a HeapNumber, we're done.
  __ JumpIfSmi(value, &done);
  __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                 RootIndex::kHeapNumberMap);
  if (mode() == Object::Conversion::kToNumeric) {
    // Jump to done if it is a HeapNumber.
    __ j(equal, &done);
    // Check if it is a BigInt.
    __ LoadMap(kScratchRegister, value);
    __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
            Immediate(BIGINT_TYPE));
  }
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotANumber, this);
  __ bind(&done);
}

void CheckHeapObject::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckHeapObject::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kSmi, this);
}
void CheckHeapObject::PrintParams(std::ostream& os,
                                  MaglevGraphLabeller* graph_labeller) const {}
void CheckSymbol::AllocateVreg(MaglevVregAllocationState* vreg_state) {
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
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceType(kScratchRegister, SYMBOL_TYPE);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotASymbol, this);
}
void CheckSymbol::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {}

void CheckString::AllocateVreg(MaglevVregAllocationState* vreg_state) {
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
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceTypeRange(kScratchRegister, kScratchRegister, FIRST_STRING_TYPE,
                          LAST_STRING_TYPE);
  __ EmitEagerDeoptIf(above, DeoptimizeReason::kNotAString, this);
}
void CheckString::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {}

void CheckMapsWithMigration::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
}
void CheckMapsWithMigration::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  Register object = ToRegister(receiver_input());

  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
  }
  __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map().object());

  __ JumpToDeferredIf(
      not_equal,
      [](MaglevAssembler* masm, Label* return_label, Register object,
         CheckMapsWithMigration* node, EagerDeoptInfo* deopt_info) {
        __ RegisterEagerDeopt(deopt_info, DeoptimizeReason::kWrongMap);

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
              masm, node->register_snapshot());

          __ Push(object);
          __ Move(kContextRegister, masm->native_context().object());
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

void CheckJSArrayBounds::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSArrayBounds::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  __ AssertNotSmi(object);
  __ AssertSmi(index);

  if (FLAG_debug_code) {
    __ CmpObjectType(object, JS_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  TaggedRegister length(kScratchRegister);
  __ LoadAnyTaggedField(length, FieldOperand(object, JSArray::kLengthOffset));
  __ cmp_tagged(index, length.reg());
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckJSObjectElementsBounds::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSObjectElementsBounds::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  __ AssertNotSmi(object);
  __ AssertSmi(index);

  if (FLAG_debug_code) {
    __ CmpObjectType(object, FIRST_JS_OBJECT_TYPE, kScratchRegister);
    __ Assert(greater_equal, AbortReason::kUnexpectedValue);
  }
  __ LoadAnyTaggedField(kScratchRegister,
                        FieldOperand(object, JSObject::kElementsOffset));
  if (FLAG_debug_code) {
    __ AssertNotSmi(kScratchRegister);
  }
  TaggedRegister length(kScratchRegister);
  __ LoadAnyTaggedField(
      length, FieldOperand(kScratchRegister, FixedArray::kLengthOffset));
  __ cmp_tagged(index, length.reg());
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckedInternalizedString::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  set_temporaries_needed(1);
  DefineSameAsFirst(vreg_state, this);
}
void CheckedInternalizedString::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register object = ToRegister(object_input());
  RegList temps = temporaries();
  Register map_tmp = temps.PopFirst();

  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
  }

  __ LoadMap(map_tmp, object);
  __ RecordComment("Test IsInternalizedString");
  // Go to the slow path if this is a non-string, or a non-internalised string.
  __ testw(FieldOperand(map_tmp, Map::kInstanceTypeOffset),
           Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  static_assert((kStringTag | kInternalizedTag) == 0);
  __ JumpToDeferredIf(
      not_zero,
      [](MaglevAssembler* masm, Label* return_label, Register object,
         CheckedInternalizedString* node, EagerDeoptInfo* deopt_info,
         Register map_tmp) {
        __ RecordComment("Deferred Test IsThinString");
        __ movw(map_tmp, FieldOperand(map_tmp, Map::kInstanceTypeOffset));
        static_assert(kThinStringTagBit > 0);
        // Deopt if this isn't a string.
        __ testw(map_tmp, Immediate(kIsNotStringMask));
        __ j(not_zero, &deopt_info->deopt_entry_label);
        // Deopt if this isn't a thin string.
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
void LoadTaggedField::GenerateCode(MaglevAssembler* masm,
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
void LoadDoubleField::GenerateCode(MaglevAssembler* masm,
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

void LoadTaggedElement::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(vreg_state, this);
}
void LoadTaggedElement::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  __ AssertNotSmi(object);
  if (FLAG_debug_code) {
    __ CmpObjectType(object, JS_OBJECT_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }
  __ DecompressAnyTagged(kScratchRegister,
                         FieldOperand(object, JSObject::kElementsOffset));
  if (FLAG_debug_code) {
    __ CmpObjectType(kScratchRegister, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    // Reload since CmpObjectType clobbered the scratch register.
    __ DecompressAnyTagged(kScratchRegister,
                           FieldOperand(object, JSObject::kElementsOffset));
  }
  __ AssertSmi(index);
  // Zero out top bits of index reg (these were previously either zero already,
  // or the cage base). This technically mutates it, but since it's a Smi, that
  // doesn't matter.
  __ movl(index, index);
  static_assert(kSmiTagSize + kSmiShiftSize < times_tagged_size,
                "Folding the Smi shift into the FixedArray entry size shift "
                "only works if the shift is small");
  __ DecompressAnyTagged(
      result_reg,
      FieldOperand(kScratchRegister, index,
                   static_cast<ScaleFactor>(times_tagged_size -
                                            (kSmiTagSize + kSmiShiftSize)),
                   FixedArray::kHeaderSize));
}

void LoadDoubleElement::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(vreg_state, this);
}
void LoadDoubleElement::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister result_reg = ToDoubleRegister(result());
  __ AssertNotSmi(object);
  if (FLAG_debug_code) {
    __ CmpObjectType(object, JS_OBJECT_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }
  __ DecompressAnyTagged(kScratchRegister,
                         FieldOperand(object, JSObject::kElementsOffset));
  if (FLAG_debug_code) {
    __ CmpObjectType(kScratchRegister, FIXED_DOUBLE_ARRAY_TYPE,
                     kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    // Reload since CmpObjectType clobbered the scratch register.
    __ DecompressAnyTagged(kScratchRegister,
                           FieldOperand(object, JSObject::kElementsOffset));
  }
  __ AssertSmi(index);
  // Zero out top bits of index reg (these were previously either zero already,
  // or the cage base). This technically mutates it, but since it's a Smi, that
  // doesn't matter.
  __ movl(index, index);
  static_assert(kSmiTagSize + kSmiShiftSize < times_8,
                "Folding the Smi shift into the FixedArray entry size shift "
                "only works if the shift is small");
  __ Movsd(result_reg,
           FieldOperand(kScratchRegister, index,
                        static_cast<ScaleFactor>(times_8 -
                                                 (kSmiTagSize + kSmiShiftSize)),
                        FixedDoubleArray::kHeaderSize));
}

void StoreTaggedFieldNoWriteBarrier::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreTaggedFieldNoWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
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
    MaglevAssembler* masm, const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));

  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  __ StoreTaggedField(FieldOperand(object, offset()), value);

  DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, Label* return_label, Register value,
         Register object, StoreTaggedFieldWithWriteBarrier* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(
            value, kScratchRegister,
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            return_label);

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);
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
void LoadNamedGeneric::PrintParams(std::ostream& os,
                                   MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void LoadNamedFromSuperGeneric::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  using D = LoadWithReceiverAndVectorDescriptor;
  UseFixed(context(), kContextRegister);
  UseFixed(receiver(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(lookup_start_object(),
           D::GetRegisterParameter(D::kLookupStartObject));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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
void LoadNamedFromSuperGeneric::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << name_ << ")";
}

void SetNamedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(value_input(), D::GetRegisterParameter(D::kValue));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void SetNamedGeneric::GenerateCode(MaglevAssembler* masm,
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
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
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
void DefineNamedOwnGeneric::GenerateCode(MaglevAssembler* masm,
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
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
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
void SetKeyedGeneric::GenerateCode(MaglevAssembler* masm,
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
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
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
void DefineKeyedOwnGeneric::GenerateCode(MaglevAssembler* masm,
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
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
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
void StoreInArrayLiteralGeneric::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kStoreInArrayLiteralIC>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object_input()), D::GetRegisterParameter(D::kReceiver));
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kValue));
  DCHECK_EQ(ToRegister(name_input()), D::GetRegisterParameter(D::kName));
  __ Move(D::GetRegisterParameter(D::kSlot),
          Smi::FromInt(feedback().slot.ToInt()));
  __ Move(D::GetRegisterParameter(D::kVector), feedback().vector);
  __ CallBuiltin(Builtin::kStoreInArrayLiteralIC);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void GetKeyedGeneric::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kKeyedLoadIC>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object_input(), D::GetRegisterParameter(D::kReceiver));
  UseFixed(key_input(), D::GetRegisterParameter(D::kName));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void GapMove::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UNREACHABLE();
}
void GapMove::GenerateCode(MaglevAssembler* masm,
                           const ProcessingState& state) {
  if (source().IsRegister()) {
    Register source_reg = ToRegister(source());
    if (target().IsAnyRegister()) {
      DCHECK(target().IsRegister());
      __ movq(ToRegister(target()), source_reg);
    } else {
      __ movq(masm->ToMemOperand(target()), source_reg);
    }
  } else if (source().IsDoubleRegister()) {
    DoubleRegister source_reg = ToDoubleRegister(source());
    if (target().IsAnyRegister()) {
      DCHECK(target().IsDoubleRegister());
      __ Movsd(ToDoubleRegister(target()), source_reg);
    } else {
      __ Movsd(masm->ToMemOperand(target()), source_reg);
    }
  } else {
    DCHECK(source().IsAnyStackSlot());
    MemOperand source_op = masm->ToMemOperand(source());
    if (target().IsRegister()) {
      __ movq(ToRegister(target()), source_op);
    } else if (target().IsDoubleRegister()) {
      __ Movsd(ToDoubleRegister(target()), source_op);
    } else {
      DCHECK(target().IsAnyStackSlot());
      __ movq(kScratchRegister, source_op);
      __ movq(masm->ToMemOperand(target()), kScratchRegister);
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
    MaglevAssembler* masm, const ProcessingState& state) {
  using D = UnaryOp_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(operand_input()), D::GetRegisterParameter(D::kValue));
  __ Move(kContextRegister, masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), Immediate(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(BuiltinFor(kOperation));
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
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
    MaglevAssembler* masm, const ProcessingState& state) {
  using D = BinaryOp_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(left_input()), D::GetRegisterParameter(D::kLeft));
  DCHECK_EQ(ToRegister(right_input()), D::GetRegisterParameter(D::kRight));
  __ Move(kContextRegister, masm->native_context().object());
  __ Move(D::GetRegisterParameter(D::kSlot), Immediate(feedback().index()));
  __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
  __ CallBuiltin(BuiltinFor(kOperation));
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

#define DEF_OPERATION(Name)                                        \
  void Name::AllocateVreg(MaglevVregAllocationState* vreg_state) { \
    Base::AllocateVreg(vreg_state);                                \
  }                                                                \
  void Name::GenerateCode(MaglevAssembler* masm,                   \
                          const ProcessingState& state) {          \
    Base::GenerateCode(masm, state);                               \
  }
GENERIC_OPERATIONS_NODE_LIST(DEF_OPERATION)
#undef DEF_OPERATION

void Int32AddWithOverflow::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ addl(left, right);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ subl(left, right);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
  set_temporaries_needed(1);
}

void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register result = ToRegister(this->result());
  Register right = ToRegister(right_input());
  DCHECK_EQ(result, ToRegister(left_input()));

  Register saved_left = temporaries().first();
  __ movl(saved_left, result);
  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ imull(result, right);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);

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
    __ EmitEagerDeoptIf(less, DeoptimizeReason::kOverflow, this);
  }
  __ bind(&end);
}

void Int32DivideWithOverflow::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsFixed(vreg_state, this, rax);
  // rax,rdx are clobbered by idiv.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  DCHECK(temporaries().has(rax));
  DCHECK(temporaries().has(rdx));
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ movl(rax, left);

  // TODO(leszeks): peephole optimise division by a constant.

  // Sign extend eax into edx.
  __ cdq();

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ cmpl(right, Immediate(0));
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, Label* return_label, Register right,
         Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        // TODO(leszeks): Using kNotInt32 here, but kDivisionByZero would be
        // better. Right now all eager deopts in a node have to be the same --
        // we should allow a node to emit multiple eager deopts with different
        // reasons.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is zero, as that would produce minus zero. Left is in
        // rax already.
        __ cmpl(rax, Immediate(0));
        // TODO(leszeks): Better DeoptimizeReason = kMinusZero.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ cmpl(rax, Immediate(kMinInt));
        __ j(not_equal, return_label);
        __ cmpl(right, Immediate(-1));
        __ j(not_equal, return_label);
        // TODO(leszeks): Better DeoptimizeReason = kOverflow, but
        // eager_deopt_info is already configured as kNotInt32.
        __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
      },
      right, this);

  // Perform the actual integer division.
  __ idivl(right);

  // Check that the remainder is zero.
  __ cmpl(rdx, Immediate(0));
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, this);
  DCHECK_EQ(ToRegister(result()), rax);
}

void Int32BitwiseAnd::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(vreg_state, this);
}

void Int32BitwiseAnd::GenerateCode(MaglevAssembler* masm,
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

void Int32BitwiseOr::GenerateCode(MaglevAssembler* masm,
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

void Int32BitwiseXor::GenerateCode(MaglevAssembler* masm,
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

void Int32ShiftLeft::GenerateCode(MaglevAssembler* masm,
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

void Int32ShiftRight::GenerateCode(MaglevAssembler* masm,
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

void Int32ShiftRightLogical::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ shrl_cl(left);
  // The result of >>> is unsigned, but Maglev doesn't yet track
  // signed/unsigned representations. Instead, deopt if the resulting smi would
  // be negative.
  // TODO(jgruber): Properly track signed/unsigned representations and
  // allocated a heap number if the result is outside smi range.
  __ testl(left, Immediate((1 << 31) | (1 << 30)));
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kOverflow, this);
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
    MaglevAssembler* masm, const ProcessingState& state) {
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
  void Name::GenerateCode(MaglevAssembler* masm,                   \
                          const ProcessingState& state) {          \
    Base::GenerateCode(masm, state);                               \
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

void Float64Add::GenerateCode(MaglevAssembler* masm,
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

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
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

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
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

void Float64Divide::GenerateCode(MaglevAssembler* masm,
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
    MaglevAssembler* masm, const ProcessingState& state) {
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
  void Name::GenerateCode(MaglevAssembler* masm,                   \
                          const ProcessingState& state) {          \
    Base::GenerateCode(masm, state);                               \
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

void CheckedSmiTag::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineSameAsFirst(vreg_state, this);
}

void CheckedSmiTag::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Register reg = ToRegister(input());
  __ addl(reg, reg);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32Constant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  DefineAsConstant(vreg_state, this);
}
void Int32Constant::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {}
void Int32Constant::DoLoadToRegister(MaglevAssembler* masm, Register reg) {
  __ Move(reg, Immediate(value()));
}
Handle<Object> Int32Constant::DoReify(LocalIsolate* isolate) {
  return isolate->factory()->NewNumber<AllocationType::kOld>(value());
}
void Int32Constant::PrintParams(std::ostream& os,
                                MaglevGraphLabeller* graph_labeller) const {
  os << "(" << value() << ")";
}

void Float64Box::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineAsRegister(vreg_state, this);
}
void Float64Box::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  Register object = ToRegister(result());
  // In the case we need to call the runtime, we should spill the input
  // register. Even if it is not live in the next node, otherwise the allocation
  // call might trash it.
  RegisterSnapshot save_registers = register_snapshot();
  save_registers.live_double_registers.set(value);
  AllocateRaw(masm, save_registers, object, HeapNumber::kSize);
  __ LoadRoot(kScratchRegister, RootIndex::kHeapNumberMap);
  __ StoreTaggedField(FieldOperand(object, HeapObject::kMapOffset),
                      kScratchRegister);
  __ Movsd(FieldOperand(object, HeapNumber::kValueOffset), value);
}

void CheckedFloat64Unbox::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineAsRegister(vreg_state, this);
}
void CheckedFloat64Unbox::GenerateCode(MaglevAssembler* masm,
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
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotANumber, this);
  __ Movsd(ToDoubleRegister(result()),
           FieldOperand(value, HeapNumber::kValueOffset));
  __ bind(&done);
}

void LogicalNot::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
  DefineAsRegister(vreg_state, this);
}
void LogicalNot::GenerateCode(MaglevAssembler* masm,
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

void SetPendingMessage::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
  DefineAsRegister(vreg_state, this);
}

void SetPendingMessage::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register message = ToRegister(value());
  Register return_value = ToRegister(result());
  Isolate* isolate = masm->isolate();
  MemOperand message_op = __ ExternalReferenceAsOperand(
      ExternalReference::address_of_pending_message(isolate), kScratchRegister);
  __ Move(return_value, message_op);
  __ movq(message_op, message);
}

void ToBooleanLogicalNot::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
  DefineAsRegister(vreg_state, this);
}
void ToBooleanLogicalNot::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());
  Label done;
  Zone* zone = masm->compilation_info()->zone();
  ZoneLabelRef object_is_true(zone), object_is_false(zone);
  ToBoolean(masm, object, object_is_true, object_is_false, true);
  __ bind(*object_is_true);
  __ LoadRoot(return_value, RootIndex::kFalseValue);
  __ jmp(&done);
  __ bind(*object_is_false);
  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ bind(&done);
}

void TaggedEqual::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(lhs());
  UseRegister(rhs());
  DefineAsRegister(vreg_state, this);
}
void TaggedEqual::GenerateCode(MaglevAssembler* masm,
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

void TaggedNotEqual::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(lhs());
  UseRegister(rhs());
  DefineAsRegister(vreg_state, this);
}
void TaggedNotEqual::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Label done, if_equal;
  __ cmp_tagged(ToRegister(lhs()), ToRegister(rhs()));
  __ j(equal, &if_equal, Label::kNear);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ jmp(&done, Label::kNear);
  __ bind(&if_equal);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);
  __ bind(&done);
}

void TestInstanceOf::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kInstanceOf>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(object(), D::GetRegisterParameter(D::kLeft));
  UseFixed(callable(), D::GetRegisterParameter(D::kRight));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void TestInstanceOf::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kInstanceOf>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(object()), D::GetRegisterParameter(D::kLeft));
  DCHECK_EQ(ToRegister(callable()), D::GetRegisterParameter(D::kRight));
#endif
  __ CallBuiltin(Builtin::kInstanceOf);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void TestUndetectable::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
  set_temporaries_needed(1);
  DefineAsRegister(vreg_state, this);
}
void TestUndetectable::GenerateCode(MaglevAssembler* masm,
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
  __ testl(FieldOperand(tmp, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsUndetectableBit::kMask));
  __ j(zero, &done);
  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ bind(&done);
}

void TestTypeOf::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
  DefineAsRegister(vreg_state, this);
}
void TestTypeOf::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  using LiteralFlag = interpreter::TestTypeOfFlags::LiteralFlag;
  Register object = ToRegister(value());
  // Use return register as temporary if needed.
  Register tmp = ToRegister(result());
  Label is_true, is_false, done;
  switch (literal_) {
    case LiteralFlag::kNumber:
      __ JumpIfSmi(object, &is_true);
      __ CompareRoot(FieldOperand(object, HeapObject::kMapOffset),
                     RootIndex::kHeapNumberMap);
      __ j(not_equal, &is_false);
      break;
    case LiteralFlag::kString:
      __ JumpIfSmi(object, &is_false);
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(FIRST_NONSTRING_TYPE));
      __ j(greater_equal, &is_false);
      break;
    case LiteralFlag::kSymbol:
      __ JumpIfSmi(object, &is_false);
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(SYMBOL_TYPE));
      __ j(not_equal, &is_false);
      break;
    case LiteralFlag::kBoolean:
      __ CompareRoot(object, RootIndex::kTrueValue);
      __ j(equal, &is_true);
      __ CompareRoot(object, RootIndex::kFalseValue);
      __ j(not_equal, &is_false);
      break;
    case LiteralFlag::kBigInt:
      __ JumpIfSmi(object, &is_false);
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(BIGINT_TYPE));
      __ j(not_equal, &is_false);
      break;
    case LiteralFlag::kUndefined:
      __ JumpIfSmi(object, &is_false);
      // Check it has the undetectable bit set and it is not null.
      __ LoadMap(tmp, object);
      __ testl(FieldOperand(tmp, Map::kBitFieldOffset),
               Immediate(Map::Bits1::IsUndetectableBit::kMask));
      __ j(zero, &is_false);
      __ CompareRoot(object, RootIndex::kNullValue);
      __ j(equal, &is_false);
      break;
    case LiteralFlag::kFunction:
      __ JumpIfSmi(object, &is_false);
      // Check if callable bit is set and not undetectable.
      __ LoadMap(tmp, object);
      __ movl(tmp, FieldOperand(tmp, Map::kBitFieldOffset));
      __ andl(tmp, Immediate(Map::Bits1::IsUndetectableBit::kMask |
                             Map::Bits1::IsCallableBit::kMask));
      __ cmpl(tmp, Immediate(Map::Bits1::IsCallableBit::kMask));
      __ j(not_equal, &is_false);
      break;
    case LiteralFlag::kObject:
      __ JumpIfSmi(object, &is_false);
      // If the object is null then return true.
      __ CompareRoot(object, RootIndex::kNullValue);
      __ j(equal, &is_true);
      // Check if the object is a receiver type,
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(FIRST_JS_RECEIVER_TYPE));
      __ j(less, &is_false);
      // ... and is not undefined (undetectable) nor callable.
      __ testl(FieldOperand(tmp, Map::kBitFieldOffset),
               Immediate(Map::Bits1::IsUndetectableBit::kMask |
                         Map::Bits1::IsCallableBit::kMask));
      __ j(not_zero, &is_false);
      break;
    case LiteralFlag::kOther:
      UNREACHABLE();
  }
  __ bind(&is_true);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ jmp(&done);
  __ bind(&is_false);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);
  __ bind(&done);
}

void ToName::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kToName>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void ToNumberOrNumeric::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = TypeConversionDescriptor;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kArgument));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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

void ToObject::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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
  __ JumpIfSmi(value, &call_builtin);
  __ LoadMap(kScratchRegister, value);
  __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
          Immediate(FIRST_JS_RECEIVER_TYPE));
  __ j(greater_equal, &done);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToObject);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

void ToString::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kO));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
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
  __ JumpIfSmi(value, &call_builtin);
  __ LoadMap(kScratchRegister, value);
  __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
          Immediate(FIRST_NONSTRING_TYPE));
  __ j(less, &done);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToString);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

void ChangeInt32ToFloat64::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(input());
  DefineAsRegister(vreg_state, this);
}
void ChangeInt32ToFloat64::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  __ Cvtlsi2sd(ToDoubleRegister(result()), ToRegister(input()));
}

void Phi::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  // Phi inputs are processed in the post-process, once loop phis' inputs'
  // v-regs are allocated.

  // We have to pass a policy, but it is later ignored during register
  // allocation. See StraightForwardRegisterAllocator::AllocateRegisters
  // which has special handling for Phis.
  static const compiler::UnallocatedOperand::ExtendedPolicy kIgnoredPolicy =
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT;

  result().SetUnallocated(kIgnoredPolicy,
                          vreg_state->AllocateVirtualRegister());
}
// TODO(verwaest): Remove after switching the register allocator.
void Phi::AllocateVregInPostProcess(MaglevVregAllocationState* vreg_state) {
  for (Input& input : *this) {
    UseAny(input);
  }
}
void Phi::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {}
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
void Call::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  // TODO(leszeks): Port the nice Sparkplug CallBuiltin helper.

  DCHECK_EQ(ToRegister(function()),
            CallTrampolineDescriptor::GetRegisterParameter(
                CallTrampolineDescriptor::kFunction));
  DCHECK_EQ(ToRegister(context()), kContextRegister);

  for (int i = num_args() - 1; i >= 0; --i) {
    __ PushInput(arg(i));
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

  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
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
void Construct::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  using D = ConstructStubDescriptor;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(new_target()), D::GetRegisterParameter(D::kNewTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);

  for (int i = num_args() - 1; i >= 0; --i) {
    __ PushInput(arg(i));
  }

  uint32_t arg_count = num_args();
  __ Move(D::GetRegisterParameter(D::kActualArgumentsCount),
          Immediate(arg_count));

  __ CallBuiltin(Builtin::kConstruct);

  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void CallBuiltin::AllocateVreg(MaglevVregAllocationState* vreg_state) {
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
  DCHECK_EQ(descriptor.GetReturnCount(), 1);
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}

void CallBuiltin::PassFeedbackSlotOnStack(MaglevAssembler* masm) {
  DCHECK(has_feedback());
  switch (slot_type()) {
    case kTaggedIndex:
      __ Push(TaggedIndex::FromIntptr(feedback().index()));
      break;
    case kSmi:
      __ Push(Smi::FromInt(feedback().index()));
      break;
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

void CallBuiltin::PushFeedback(MaglevAssembler* masm) {
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
  } else if (vector_index == descriptor.GetRegisterParameterCount()) {
    PassFeedbackSlotInRegister(masm);
    // We do not allow var args if has_feedback(), so here we have only one
    // parameter on stack and do not need to check stack arguments order.
    __ Push(feedback().vector);
  } else {
    // Same as above. We does not allow var args if has_feedback(), so feedback
    // slot and vector must be last two inputs.
    if (descriptor.GetStackArgumentOrder() == StackArgumentOrder::kDefault) {
      PassFeedbackSlotOnStack(masm);
      __ Push(feedback().vector);
    } else {
      DCHECK_EQ(descriptor.GetStackArgumentOrder(), StackArgumentOrder::kJS);
      __ Push(feedback().vector);
      PassFeedbackSlotOnStack(masm);
    }
  }
}

void CallBuiltin::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());

  if (descriptor.GetStackArgumentOrder() == StackArgumentOrder::kDefault) {
    for (int i = InputsInRegisterCount(); i < InputCountWithoutContext(); ++i) {
      __ PushInput(input(i));
    }
    if (has_feedback()) {
      PushFeedback(masm);
    }
  } else {
    DCHECK_EQ(descriptor.GetStackArgumentOrder(), StackArgumentOrder::kJS);
    if (has_feedback()) {
      PushFeedback(masm);
    }
    for (int i = InputCountWithoutContext() - 1; i >= InputsInRegisterCount();
         --i) {
      __ PushInput(input(i));
    }
  }
  __ CallBuiltin(builtin());
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}
void CallBuiltin::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << Builtins::name(builtin()) << ")";
}

void CallRuntime::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CallRuntime::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    __ PushInput(arg(i));
  }
  __ CallRuntime(function_id(), num_args());
  // TODO(victorgomes): Not sure if this is needed for all runtime calls.
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}
void CallRuntime::PrintParams(std::ostream& os,
                              MaglevGraphLabeller* graph_labeller) const {
  os << "(" << Runtime::FunctionForId(function_id())->name << ")";
}

void CallWithSpread::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args() - 1; i++) {
    UseAny(arg(i));
  }
  UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void CallWithSpread::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  // Push other arguments (other than the spread) to the stack.
  int argc_no_spread = num_args() - 1;
  for (int i = argc_no_spread - 1; i >= 0; --i) {
    __ PushInput(arg(i));
  }
  __ Move(D::GetRegisterParameter(D::kArgumentsCount),
          Immediate(argc_no_spread));
  __ CallBuiltin(Builtin::kCallWithSpread);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void ConstructWithSpread::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  using D = CallInterfaceDescriptorFor<Builtin::kConstructWithSpread>::type;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseFixed(new_target(), D::GetRegisterParameter(D::kNewTarget));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args() - 1; i++) {
    UseAny(arg(i));
  }
  UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  DefineAsFixed(vreg_state, this, kReturnRegister0);
}
void ConstructWithSpread::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<Builtin::kConstructWithSpread>::type;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(new_target()), D::GetRegisterParameter(D::kNewTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  // Push other arguments (other than the spread) to the stack.
  int argc_no_spread = num_args() - 1;
  for (int i = argc_no_spread - 1; i >= 0; --i) {
    __ PushInput(arg(i));
  }
  __ Move(D::GetRegisterParameter(D::kActualArgumentsCount),
          Immediate(argc_no_spread));
  __ CallBuiltin(Builtin::kConstructWithSpread);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

void IncreaseInterruptBudget::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  set_temporaries_needed(1);
}
void IncreaseInterruptBudget::GenerateCode(MaglevAssembler* masm,
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
void ReduceInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register scratch = temporaries().first();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      scratch, FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ subl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount()));
  __ JumpToDeferredIf(
      less,
      [](MaglevAssembler* masm, Label* return_label,
         ReduceInterruptBudget* node) {
        {
          SaveRegisterStateForCall save_register_state(
              masm, node->register_snapshot());
          __ Move(kContextRegister, masm->native_context().object());
          __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
          __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev,
                         1);
          save_register_state.DefineSafepointWithLazyDeopt(
              node->lazy_deopt_info());
        }
        __ jmp(return_label);
      },
      this);
}
void ReduceInterruptBudget::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << amount() << ")";
}

void ThrowReferenceErrorIfHole::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseAny(value());
}
void ThrowReferenceErrorIfHole::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  if (value().operand().IsRegister()) {
    __ CompareRoot(ToRegister(value()), RootIndex::kTheHoleValue);
  } else {
    DCHECK(value().operand().IsStackSlot());
    __ CompareRoot(masm->ToMemOperand(value()), RootIndex::kTheHoleValue);
  }
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Label* return_label,
         ThrowReferenceErrorIfHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ Push(node->name().object());
        __ CallRuntime(Runtime::kThrowAccessedUninitializedVariable, 1);
        masm->DefineLazyDeoptPoint(node->lazy_deopt_info());
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

void ThrowSuperNotCalledIfHole::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseAny(value());
}
void ThrowSuperNotCalledIfHole::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  if (value().operand().IsRegister()) {
    __ CompareRoot(ToRegister(value()), RootIndex::kTheHoleValue);
  } else {
    DCHECK(value().operand().IsStackSlot());
    __ CompareRoot(masm->ToMemOperand(value()), RootIndex::kTheHoleValue);
  }
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Label* return_label,
         ThrowSuperNotCalledIfHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowSuperNotCalled, 0);
        masm->DefineLazyDeoptPoint(node->lazy_deopt_info());
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

void ThrowSuperAlreadyCalledIfNotHole::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseAny(value());
}
void ThrowSuperAlreadyCalledIfNotHole::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  if (value().operand().IsRegister()) {
    __ CompareRoot(ToRegister(value()), RootIndex::kTheHoleValue);
  } else {
    DCHECK(value().operand().IsStackSlot());
    __ CompareRoot(masm->ToMemOperand(value()), RootIndex::kTheHoleValue);
  }
  __ JumpToDeferredIf(
      not_equal,
      [](MaglevAssembler* masm, Label* return_label,
         ThrowSuperAlreadyCalledIfNotHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowSuperAlreadyCalledError, 0);
        masm->DefineLazyDeoptPoint(node->lazy_deopt_info());
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

void ThrowIfNotSuperConstructor::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(constructor());
  UseRegister(function());
}
void ThrowIfNotSuperConstructor::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  __ LoadMap(kScratchRegister, ToRegister(constructor()));
  __ testl(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsConstructorBit::kMask));
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Label* return_label,
         ThrowIfNotSuperConstructor* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ Push(ToRegister(node->constructor()));
        __ Push(ToRegister(node->function()));
        __ CallRuntime(Runtime::kThrowNotSuperConstructor, 2);
        masm->DefineLazyDeoptPoint(node->lazy_deopt_info());
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

// ---
// Control nodes
// ---
void Return::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseFixed(value_input(), kReturnRegister0);
}
void Return::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  DCHECK_EQ(ToRegister(value_input()), kReturnRegister0);

  // Read the formal number of parameters from the top level compilation unit
  // (i.e. the outermost, non inlined function).
  int formal_params_size =
      masm->compilation_info()->toplevel_compilation_unit()->parameter_count();

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
void Deopt::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  __ EmitEagerDeopt(this, reason());
}
void Deopt::PrintParams(std::ostream& os,
                        MaglevGraphLabeller* graph_labeller) const {
  os << "(" << DeoptimizeReasonToString(reason()) << ")";
}

void Switch::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(value());
}
void Switch::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  std::unique_ptr<Label*[]> labels = std::make_unique<Label*[]>(size());
  for (int i = 0; i < size(); i++) {
    labels[i] = (targets())[i].block_ptr()->label();
  }
  __ Switch(kScratchRegister, ToRegister(value()), value_base(), labels.get(),
            size());
  if (has_fallthrough()) {
    DCHECK_EQ(fallthrough(), state.next_block());
  } else {
    __ Trap();
  }
}

void Jump::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void Jump::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ jmp(target()->label());
  }
}

void JumpToInlined::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void JumpToInlined::GenerateCode(MaglevAssembler* masm,
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
void JumpFromInlined::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  // Avoid emitting a jump to the next block.
  if (target() != state.next_block()) {
    __ jmp(target()->label());
  }
}

namespace {

void AttemptOnStackReplacement(MaglevAssembler* masm, Label* return_label,
                               JumpLoopPrologue* node, Register scratch0,
                               Register scratch1, int32_t loop_depth,
                               FeedbackSlot feedback_slot,
                               BytecodeOffset osr_offset) {
  // Two cases may cause us to attempt OSR, in the following order:
  //
  // 1) Presence of cached OSR Turbofan code.
  // 2) The OSR urgency exceeds the current loop depth - in that case, trigger
  //    a Turbofan OSR compilation.
  //
  // See also: InterpreterAssembler::OnStackReplacement.

  baseline::BaselineAssembler basm(masm);

  // Case 1).
  Label deopt;
  Register maybe_target_code = scratch1;
  {
    basm.TryLoadOptimizedOsrCode(maybe_target_code, scratch0, feedback_slot,
                                 &deopt, Label::kFar);
  }

  // Case 2).
  {
    __ AssertFeedbackVector(scratch0);
    __ movb(scratch0, FieldOperand(scratch0, FeedbackVector::kOsrStateOffset));
    __ DecodeField<FeedbackVector::OsrUrgencyBits>(scratch0);
    basm.JumpIfByte(baseline::Condition::kUnsignedLessThanEqual, scratch0,
                    loop_depth, return_label, Label::kNear);

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
      detail::DeepForEachInput(
          node->eager_deopt_info(),
          [&](ValueNode* node, interpreter::Register reg,
              InputLocation* input) {
            if (!input->IsAnyRegister()) return;
            if (input->IsDoubleRegister()) {
              snapshot.live_double_registers.set(
                  input->AssignedDoubleRegister());
            } else {
              snapshot.live_registers.set(input->AssignedGeneralRegister());
              if (node->is_tagged()) {
                snapshot.live_tagged_registers.set(
                    input->AssignedGeneralRegister());
              }
            }
          });
      SaveRegisterStateForCall save_register_state(masm, snapshot);
      __ Move(kContextRegister, masm->native_context().object());
      __ Push(Smi::FromInt(osr_offset.ToInt()));
      __ CallRuntime(Runtime::kCompileOptimizedOSRFromMaglev, 1);
      save_register_state.DefineSafepoint();
      __ Move(scratch0, rax);
    }

    // A `0` return value means there is no OSR code available yet. Fall
    // through for now, OSR code will be picked up once it exists and is
    // cached on the feedback vector.
    __ testq(scratch0, scratch0);
    __ j(equal, return_label, Label::kNear);
  }

  __ bind(&deopt);
  if (V8_LIKELY(FLAG_turbofan)) {
    __ EmitEagerDeopt(node, DeoptimizeReason::kPrepareForOnStackReplacement);
  } else {
    // Fall through. With TF disabled we cannot OSR and thus it doesn't make
    // sense to start the process. We do still perform all remaining
    // bookkeeping above though, to keep Maglev code behavior roughly the same
    // in both configurations.
  }
}

}  // namespace

void JumpLoopPrologue::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  set_temporaries_needed(2);
}
void JumpLoopPrologue::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register scratch0 = temporaries().PopFirst();
  Register scratch1 = temporaries().PopFirst();

  const Register osr_state = scratch1;
  __ Move(scratch0, unit_->feedback().object());
  __ AssertFeedbackVector(scratch0);
  __ movb(osr_state, FieldOperand(scratch0, FeedbackVector::kOsrStateOffset));

  // The quick initial OSR check. If it passes, we proceed on to more
  // expensive OSR logic.
  static_assert(FeedbackVector::MaybeHasOptimizedOsrCodeBit::encode(true) >
                FeedbackVector::kMaxOsrUrgency);
  __ cmpl(osr_state, Immediate(loop_depth_));
  __ JumpToDeferredIf(above, AttemptOnStackReplacement, this, scratch0,
                      scratch1, loop_depth_, feedback_slot_, osr_offset_);
}

void JumpLoop::AllocateVreg(MaglevVregAllocationState* vreg_state) {}
void JumpLoop::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  __ jmp(target()->label());
}

void BranchIfRootConstant::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(condition_input());
}
void BranchIfRootConstant::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  __ CompareRoot(ToRegister(condition_input()), root_index());
  __ Branch(equal, if_true(), if_false(), state.next_block());
}
void BranchIfRootConstant::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << RootsTable::name(root_index_) << ")";
}

void BranchIfUndefinedOrNull::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
  UseRegister(condition_input());
}
void BranchIfUndefinedOrNull::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(condition_input());
  __ JumpIfRoot(value, RootIndex::kUndefinedValue, if_true()->label());
  __ JumpIfRoot(value, RootIndex::kNullValue, if_true()->label());
  auto* next_block = state.next_block();
  if (if_false() != next_block) {
    __ jmp(if_false()->label());
  }
}

void BranchIfJSReceiver::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(condition_input());
}
void BranchIfJSReceiver::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register value = ToRegister(condition_input());
  __ JumpIfSmi(value, if_false()->label());
  __ LoadMap(kScratchRegister, value);
  __ CmpInstanceType(kScratchRegister, FIRST_JS_RECEIVER_TYPE);
  __ Branch(above_equal, if_true(), if_false(), state.next_block());
}

void BranchIfInt32Compare::AllocateVreg(MaglevVregAllocationState* vreg_state) {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfInt32Compare::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ cmpl(left, right);
  __ Branch(ConditionFor(operation_), if_true(), if_false(),
            state.next_block());
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
void BranchIfFloat64Compare::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Ucomisd(left, right);
  __ Branch(ConditionFor(operation_), if_true(), if_false(),
            state.next_block());
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
void BranchIfReferenceCompare::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ cmp_tagged(left, right);
  __ Branch(ConditionFor(operation_), if_true(), if_false(),
            state.next_block());
}
void BranchIfReferenceCompare::PrintParams(
    std::ostream& os, MaglevGraphLabeller* graph_labeller) const {
  os << "(" << operation_ << ")";
}

void BranchIfToBooleanTrue::AllocateVreg(
    MaglevVregAllocationState* vreg_state) {
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
  ToBoolean(masm, ToRegister(condition_input()), true_label, false_label,
            fallthrough_when_true);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
