// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_IMPL_H_
#define V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_IMPL_H_

#include "src/codegen/macro-assembler.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/linkage.h"
#include "src/objects/tagged-index.h"

namespace v8 {
namespace internal {
namespace compiler {

struct CaseInfoT {
  int32_t value;  // The case value.
  int32_t order;  // The order for lowering to comparisons (less means earlier).
  turboshaft::Block*
      branch;  // The basic blocks corresponding to the case value.
};

inline bool operator<(const CaseInfoT& l, const CaseInfoT& r) {
  return l.order < r.order;
}

// Helper struct containing data about a table or lookup switch.
class SwitchInfoT {
 public:
  using CaseInfo = CaseInfoT;
  SwitchInfoT(ZoneVector<CaseInfo> const& cases, int32_t min_value,
              int32_t max_value, turboshaft::Block* default_branch)
      : cases_(cases),
        min_value_(min_value),
        max_value_(max_value),
        default_branch_(default_branch) {
    if (cases.size() != 0) {
      DCHECK_LE(min_value, max_value);
      // Note that {value_range} can be 0 if {min_value} is -2^31 and
      // {max_value} is 2^31-1, so don't assume that it's non-zero below.
      value_range_ = 1u + base::bit_cast<uint32_t>(max_value) -
                     base::bit_cast<uint32_t>(min_value);
    } else {
      value_range_ = 0;
    }
  }

  std::vector<CaseInfo> CasesSortedByValue() const {
    std::vector<CaseInfo> result(cases_.begin(), cases_.end());
    std::stable_sort(result.begin(), result.end(),
                     [](CaseInfo a, CaseInfo b) { return a.value < b.value; });
    return result;
  }
  const ZoneVector<CaseInfo>& CasesUnsorted() const { return cases_; }
  int32_t min_value() const { return min_value_; }
  int32_t max_value() const { return max_value_; }
  size_t value_range() const { return value_range_; }
  size_t case_count() const { return cases_.size(); }
  turboshaft::Block* default_branch() const { return default_branch_; }

 private:
  const ZoneVector<CaseInfo>& cases_;
  int32_t min_value_;   // minimum value of {cases_}
  int32_t max_value_;   // maximum value of {cases_}
  size_t value_range_;  // |max_value - min_value| + 1
  turboshaft::Block* default_branch_;
};

// A helper class for the instruction selector that simplifies construction of
// Operands. This class implements a base for architecture-specific helpers.
class OperandGeneratorT : public TurboshaftAdapter {
 public:
  explicit OperandGeneratorT(InstructionSelectorT* selector)
      : TurboshaftAdapter(selector->schedule()), selector_(selector) {}

  InstructionOperand NoOutput() {
    return InstructionOperand();  // Generates an invalid operand.
  }

  InstructionOperand DefineAsRegister(turboshaft::OpIndex node) {
    return Define(node,
                  UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                     GetVReg(node)));
  }

  InstructionOperand DefineSameAsInput(turboshaft::OpIndex node,
                                       int input_index) {
    return Define(node, UnallocatedOperand(GetVReg(node), input_index));
  }

  InstructionOperand DefineSameAsFirst(turboshaft::OpIndex node) {
    return DefineSameAsInput(node, 0);
  }

  InstructionOperand DefineAsFixed(turboshaft::OpIndex node, Register reg) {
    return Define(node, UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                           reg.code(), GetVReg(node)));
  }

  template <typename FPRegType>
  InstructionOperand DefineAsFixed(turboshaft::OpIndex node, FPRegType reg) {
    return Define(node,
                  UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER,
                                     reg.code(), GetVReg(node)));
  }

  InstructionOperand DefineAsConstant(turboshaft::OpIndex node) {
    selector()->MarkAsDefined(node);
    int virtual_register = GetVReg(node);
    sequence()->AddConstant(virtual_register, ToConstant(node));
    return ConstantOperand(virtual_register);
  }

  InstructionOperand DefineAsLocation(turboshaft::OpIndex node,
                                      LinkageLocation location) {
    return Define(node, ToUnallocatedOperand(location, GetVReg(node)));
  }

  InstructionOperand DefineAsDualLocation(turboshaft::OpIndex node,
                                          LinkageLocation primary_location,
                                          LinkageLocation secondary_location) {
    return Define(node,
                  ToDualLocationUnallocatedOperand(
                      primary_location, secondary_location, GetVReg(node)));
  }

  InstructionOperand Use(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::NONE,
                                        UnallocatedOperand::USED_AT_START,
                                        GetVReg(node)));
  }

  InstructionOperand UseAnyAtEnd(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::REGISTER_OR_SLOT,
                                        UnallocatedOperand::USED_AT_END,
                                        GetVReg(node)));
  }

  InstructionOperand UseAny(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::REGISTER_OR_SLOT,
                                        UnallocatedOperand::USED_AT_START,
                                        GetVReg(node)));
  }

  InstructionOperand UseRegisterOrSlotOrConstant(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(
                         UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
                         UnallocatedOperand::USED_AT_START, GetVReg(node)));
  }

  InstructionOperand UseUniqueRegisterOrSlotOrConstant(
      turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(
                         UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
                         GetVReg(node)));
  }

  InstructionOperand UseRegister(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                        UnallocatedOperand::USED_AT_START,
                                        GetVReg(node)));
  }

  InstructionOperand UseRegisterAtEnd(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                        UnallocatedOperand::USED_AT_END,
                                        GetVReg(node)));
  }

  InstructionOperand UseUniqueSlot(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::MUST_HAVE_SLOT,
                                        GetVReg(node)));
  }

  // Use register or operand for the node. If a register is chosen, it won't
  // alias any temporary or output registers.
  InstructionOperand UseUnique(turboshaft::OpIndex node) {
    return Use(node,
               UnallocatedOperand(UnallocatedOperand::NONE, GetVReg(node)));
  }

  // Use a unique register for the node that does not alias any temporary or
  // output registers.
  InstructionOperand UseUniqueRegister(turboshaft::OpIndex node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                        GetVReg(node)));
  }

  enum class RegisterUseKind { kUseRegister, kUseUniqueRegister };
  InstructionOperand UseRegister(turboshaft::OpIndex node,
                                 RegisterUseKind unique_reg) {
    if (V8_LIKELY(unique_reg == RegisterUseKind::kUseRegister)) {
      return UseRegister(node);
    } else {
      DCHECK_EQ(unique_reg, RegisterUseKind::kUseUniqueRegister);
      return UseUniqueRegister(node);
    }
  }

  InstructionOperand UseFixed(turboshaft::OpIndex node, Register reg) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                        reg.code(), GetVReg(node)));
  }

  template <typename FPRegType>
  InstructionOperand UseFixed(turboshaft::OpIndex node, FPRegType reg) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER,
                                        reg.code(), GetVReg(node)));
  }

  InstructionOperand UseImmediate(int immediate) {
    return sequence()->AddImmediate(Constant(immediate));
  }

  InstructionOperand UseImmediate64(int64_t immediate) {
    return sequence()->AddImmediate(Constant(immediate));
  }

  InstructionOperand UseImmediate(turboshaft::OpIndex node) {
    return sequence()->AddImmediate(ToConstant(node));
  }

  InstructionOperand UseNegatedImmediate(turboshaft::OpIndex node) {
    return sequence()->AddImmediate(ToNegatedConstant(node));
  }

  InstructionOperand UseLocation(turboshaft::OpIndex node,
                                 LinkageLocation location) {
    return Use(node, ToUnallocatedOperand(location, GetVReg(node)));
  }

  // Used to force gap moves from the from_location to the to_location
  // immediately before an instruction.
  InstructionOperand UsePointerLocation(LinkageLocation to_location,
                                        LinkageLocation from_location) {
    UnallocatedOperand casted_from_operand =
        UnallocatedOperand::cast(TempLocation(from_location));
    selector_->Emit(kArchNop, casted_from_operand);
    return ToUnallocatedOperand(to_location,
                                casted_from_operand.virtual_register());
  }

  InstructionOperand TempRegister() {
    return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                              UnallocatedOperand::USED_AT_START,
                              sequence()->NextVirtualRegister());
  }

  int AllocateVirtualRegister() { return sequence()->NextVirtualRegister(); }

  InstructionOperand DefineSameAsFirstForVreg(int vreg) {
    return UnallocatedOperand(UnallocatedOperand::SAME_AS_INPUT, vreg);
  }

  InstructionOperand DefineAsRegistertForVreg(int vreg) {
    return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg);
  }

  InstructionOperand UseRegisterForVreg(int vreg) {
    return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                              UnallocatedOperand::USED_AT_START, vreg);
  }

  // The kind of register generated for memory operands. kRegister is alive
  // until the start of the operation, kUniqueRegister until the end.
  enum RegisterMode {
    kRegister,
    kUniqueRegister,
  };

  InstructionOperand UseRegisterWithMode(turboshaft::OpIndex node,
                                         RegisterMode register_mode) {
    return register_mode == kRegister ? UseRegister(node)
                                      : UseUniqueRegister(node);
  }

  InstructionOperand TempDoubleRegister() {
    UnallocatedOperand op = UnallocatedOperand(
        UnallocatedOperand::MUST_HAVE_REGISTER,
        UnallocatedOperand::USED_AT_START, sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(MachineRepresentation::kFloat64,
                                     op.virtual_register());
    return op;
  }

  InstructionOperand TempSimd128Register() {
    UnallocatedOperand op = UnallocatedOperand(
        UnallocatedOperand::MUST_HAVE_REGISTER,
        UnallocatedOperand::USED_AT_START, sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(MachineRepresentation::kSimd128,
                                     op.virtual_register());
    return op;
  }

  InstructionOperand TempSimd256Register() {
    UnallocatedOperand op = UnallocatedOperand(
        UnallocatedOperand::MUST_HAVE_REGISTER,
        UnallocatedOperand::USED_AT_START, sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(MachineRepresentation::kSimd256,
                                     op.virtual_register());
    return op;
  }

  InstructionOperand TempRegister(Register reg) {
    return UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER, reg.code(),
                              InstructionOperand::kInvalidVirtualRegister);
  }

  InstructionOperand TempRegister(int code) {
    return UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER, code,
                              sequence()->NextVirtualRegister());
  }

  template <typename FPRegType>
  InstructionOperand TempFpRegister(FPRegType reg) {
    UnallocatedOperand op =
        UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER, reg.code(),
                           sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(MachineRepresentation::kSimd128,
                                     op.virtual_register());
    return op;
  }

  InstructionOperand TempImmediate(int32_t imm) {
    return sequence()->AddImmediate(Constant(imm));
  }

  InstructionOperand TempLocation(LinkageLocation location) {
    return ToUnallocatedOperand(location, sequence()->NextVirtualRegister());
  }

  InstructionOperand Label(turboshaft::Block* block) {
    return sequence()->AddImmediate(Constant(this->rpo_number(block)));
  }

 protected:
  InstructionSelectorT* selector() const { return selector_; }
  InstructionSequence* sequence() const { return selector()->sequence(); }
  Zone* zone() const { return selector()->instruction_zone(); }

 private:
  int GetVReg(turboshaft::OpIndex node) const {
    return selector_->GetVirtualRegister(node);
  }

  Constant ToConstant(turboshaft::OpIndex node) {
    using Kind = turboshaft::ConstantOp::Kind;
    if (const turboshaft::ConstantOp* constant =
            this->turboshaft_graph()
                ->Get(node)
                .template TryCast<turboshaft::ConstantOp>()) {
      switch (constant->kind) {
        case Kind::kWord32:
          return Constant(static_cast<int32_t>(constant->word32()));
        case Kind::kWord64:
          return Constant(static_cast<int64_t>(constant->word64()));
        case Kind::kSmi:
          if constexpr (Is64()) {
            return Constant(static_cast<int64_t>(constant->smi().ptr()));
          } else {
            return Constant(static_cast<int32_t>(constant->smi().ptr()));
          }
        case Kind::kHeapObject:
        case Kind::kCompressedHeapObject:
        case Kind::kTrustedHeapObject:
          return Constant(constant->handle(),
                          constant->kind == Kind::kCompressedHeapObject);
        case Kind::kExternal:
          return Constant(constant->external_reference());
        case Kind::kNumber:
          return Constant(constant->number());
        case Kind::kFloat32:
          return Constant(constant->float32());
        case Kind::kFloat64:
          return Constant(constant->float64());
        case Kind::kTaggedIndex: {
          // Unencoded index value.
          intptr_t value = static_cast<intptr_t>(constant->tagged_index());
          DCHECK(TaggedIndex::IsValid(value));
          // Generate it as 32/64-bit constant in a tagged form.
          Address tagged_index = TaggedIndex::FromIntptr(value).ptr();
          if (kSystemPointerSize == kInt32Size) {
            return Constant(static_cast<int32_t>(tagged_index));
          } else {
            return Constant(static_cast<int64_t>(tagged_index));
          }
        }
        case Kind::kRelocatableWasmCall:
        case Kind::kRelocatableWasmStubCall: {
          uint64_t value = constant->integral();
          auto mode = constant->kind == Kind::kRelocatableWasmCall
                          ? RelocInfo::WASM_CALL
                          : RelocInfo::WASM_STUB_CALL;
          using constant_type = std::conditional_t<Is64(), int64_t, int32_t>;
          return Constant(RelocatablePtrConstantInfo(
              base::checked_cast<constant_type>(value), mode));
        }
        case Kind::kRelocatableWasmCanonicalSignatureId:
          return Constant(RelocatablePtrConstantInfo(
              base::checked_cast<int32_t>(constant->integral()),
              RelocInfo::WASM_CANONICAL_SIG_ID));
        case Kind::kRelocatableWasmIndirectCallTarget:
          uint64_t value = constant->integral();
          return Constant(RelocatablePtrConstantInfo(
              base::checked_cast<int32_t>(value),
              RelocInfo::WASM_CODE_POINTER_TABLE_ENTRY));
      }
    }
    UNREACHABLE();
  }

  Constant ToNegatedConstant(turboshaft::OpIndex node) {
    const turboshaft::ConstantOp& constant =
        Get(node).Cast<turboshaft::ConstantOp>();
    switch (constant.kind) {
      case turboshaft::ConstantOp::Kind::kWord32:
        return Constant(-static_cast<int32_t>(constant.word32()));
      case turboshaft::ConstantOp::Kind::kWord64:
        return Constant(-static_cast<int64_t>(constant.word64()));
      case turboshaft::ConstantOp::Kind::kSmi:
        if (Is64()) {
          return Constant(-static_cast<int64_t>(constant.smi().ptr()));
        } else {
          return Constant(-static_cast<int32_t>(constant.smi().ptr()));
        }
      default:
        UNREACHABLE();
    }
  }

  UnallocatedOperand Define(turboshaft::OpIndex node,
                            UnallocatedOperand operand) {
    DCHECK(node.valid());
    DCHECK_EQ(operand.virtual_register(), GetVReg(node));
    selector()->MarkAsDefined(node);
    return operand;
  }

  UnallocatedOperand Use(turboshaft::OpIndex node, UnallocatedOperand operand) {
    DCHECK(node.valid());
    DCHECK_EQ(operand.virtual_register(), GetVReg(node));
    selector()->MarkAsUsed(node);
    return operand;
  }

  UnallocatedOperand ToDualLocationUnallocatedOperand(
      LinkageLocation primary_location, LinkageLocation secondary_location,
      int virtual_register) {
    // We only support the primary location being a register and the secondary
    // one a slot.
    DCHECK(primary_location.IsRegister() &&
           secondary_location.IsCalleeFrameSlot());
    int reg_id = primary_location.AsRegister();
    int slot_id = secondary_location.AsCalleeFrameSlot();
    return UnallocatedOperand(reg_id, slot_id, virtual_register);
  }

  UnallocatedOperand ToUnallocatedOperand(LinkageLocation location,
                                          int virtual_register) {
    if (location.IsAnyRegister() || location.IsNullRegister()) {
      // any machine register.
      return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                virtual_register);
    }
    if (location.IsCallerFrameSlot()) {
      // a location on the caller frame.
      return UnallocatedOperand(UnallocatedOperand::FIXED_SLOT,
                                location.AsCallerFrameSlot(), virtual_register);
    }
    if (location.IsCalleeFrameSlot()) {
      // a spill location on this (callee) frame.
      return UnallocatedOperand(UnallocatedOperand::FIXED_SLOT,
                                location.AsCalleeFrameSlot(), virtual_register);
    }
    // a fixed register.
    if (IsFloatingPoint(location.GetType().representation())) {
      return UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER,
                                location.AsRegister(), virtual_register);
    }
    return UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                              location.AsRegister(), virtual_register);
  }

  InstructionSelectorT* selector_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_IMPL_H_
