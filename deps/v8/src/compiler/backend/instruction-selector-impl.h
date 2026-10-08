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

struct CaseInfo {
  int32_t value;  // The case value.
  int32_t order;  // The order for lowering to comparisons (less means earlier).
  turboshaft::Block*
      branch;  // The basic blocks corresponding to the case value.
};

inline bool operator<(const CaseInfo& l, const CaseInfo& r) {
  return l.order < r.order;
}

// Helper struct containing data about a table or lookup switch.
class SwitchInfo {
 public:
  SwitchInfo(ZoneVector<CaseInfo> const& cases, int32_t min_value,
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
class OperandGenerator : public turboshaft::OperationMatcher {
 public:
  explicit OperandGenerator(InstructionSelector* selector)
      : turboshaft::OperationMatcher(*selector->schedule()),
        selector_(selector) {}

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

  InstructionOperand DefineSameAsInputForVreg(int vreg, int input_index) {
    return UnallocatedOperand(vreg, input_index);
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

  InstructionOperand TempImmediate(int32_t imm) {
    return sequence()->AddImmediate(Constant(imm));
  }

  InstructionOperand TempLocation(LinkageLocation location) {
    return ToUnallocatedOperand(location, sequence()->NextVirtualRegister());
  }

  InstructionOperand Label(turboshaft::Block* block) {
    return sequence()->AddImmediate(Constant(selector_->rpo_number(block)));
  }

  turboshaft::Graph* turboshaft_graph() const {
    return selector()->turboshaft_graph();
  }

 protected:
  InstructionSelector* selector() const { return selector_; }
  InstructionSequence* sequence() const { return selector()->sequence(); }
  Zone* zone() const { return selector()->instruction_zone(); }

 private:
  int GetVReg(turboshaft::OpIndex node) const {
    return selector_->GetVirtualRegister(node);
  }

  Constant ToConstant(turboshaft::OpIndex node) {
    using Kind = turboshaft::ConstantOp::Kind;
    if (const turboshaft::ConstantOp* constant =
            selector_->TryCast<turboshaft::ConstantOp>(node)) {
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
        case Kind::kRelocatableWasmCodePointer: {
          using constant_type = std::conditional_t<Is64(), int64_t, int32_t>;
          return Constant(RelocatablePtrConstantInfo(
              base::checked_cast<constant_type>(constant->integral()),
              RelocInfo::WASM_CODE_POINTER));
        }
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
        selector()->Cast<turboshaft::ConstantOp>(node);
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

  InstructionSelector* selector_;
};

// Compare chain support for conditional compare (ccmp) instruction selection.
// Shared between ARM64 and x64 (APX) backends.
namespace compare_chain {

class CompareChainNode final : public ZoneObject {
 public:
  enum class NodeKind : uint8_t { kFlagSetting, kLogicalCombine };

  CompareChainNode(turboshaft::OpIndex n, FlagsCondition condition,
                   bool is_test = false)
      : node_kind_(NodeKind::kFlagSetting),
        user_condition_(condition),
        is_test_(is_test),
        node_(n) {}

  CompareChainNode(turboshaft::OpIndex n, CompareChainNode* l,
                   CompareChainNode* r)
      : node_kind_(NodeKind::kLogicalCombine), node_(n), lhs_(l), rhs_(r) {
    // Canonicalise the chain with cmps on the right.
    if (lhs_->IsFlagSetting() && !rhs_->IsFlagSetting()) {
      std::swap(lhs_, rhs_);
    }
  }
  void SetCondition(FlagsCondition condition) {
    DCHECK(IsLogicalCombine());
    user_condition_ = condition;
    if (requires_negation_) {
      NegateFlags();
    }
  }
  void MarkRequiresNegation() {
    if (IsFlagSetting()) {
      NegateFlags();
    } else {
      requires_negation_ = !requires_negation_;
    }
  }
  void NegateFlags() {
    user_condition_ = NegateFlagsCondition(user_condition_);
    requires_negation_ = false;
  }
  bool IsLegalFirstCombine() const {
    DCHECK(IsLogicalCombine());
    // We need two cmps feeding the first logic op.
    return lhs_->IsFlagSetting() && rhs_->IsFlagSetting();
  }
  bool IsFlagSetting() const { return node_kind_ == NodeKind::kFlagSetting; }
  bool IsLogicalCombine() const {
    return node_kind_ == NodeKind::kLogicalCombine;
  }
  bool IsTest() const { return is_test_; }
  turboshaft::OpIndex node() const { return node_; }
  FlagsCondition user_condition() const { return user_condition_; }
  CompareChainNode* lhs() const {
    DCHECK(IsLogicalCombine());
    return lhs_;
  }
  CompareChainNode* rhs() const {
    DCHECK(IsLogicalCombine());
    return rhs_;
  }

 private:
  NodeKind node_kind_;
  FlagsCondition user_condition_;
  bool requires_negation_ = false;
  bool is_test_ = false;
  turboshaft::OpIndex node_;
  CompareChainNode* lhs_ = nullptr;
  CompareChainNode* rhs_ = nullptr;
};

// `supports_float_cmp` indicates whether the target architecture has a
// conditional compare that can consume floating-point comparisons (e.g.
// ARM64's fccmp). x64 has no float ccmp and must pass false, otherwise a
// matched float chain would reach the arch get_opcode with a Float rep and
// hit UNREACHABLE().
std::optional<FlagsCondition> GetFlagsCondition(turboshaft::OpIndex node,
                                                InstructionSelector* selector,
                                                bool supports_float_cmp);

// Search through AND, OR and comparisons to find a chain of comparisons
// that can be combined into conditional compare instructions.
// `supports_test_pattern` indicates whether the target architecture can fold an
// `(x & mask) == 0` comparison into a TEST-style conditional compare (x64's
// ctest). ARM64 has no TEST in its ccmp chain and must pass false, otherwise a
// matched TEST node would reach the arch get_opcode with is_test=true and hit
// its DCHECK (debug) or silently emit a wrong cmp (release).
std::optional<CompareChainNode*> FindCompareChain(
    turboshaft::OpIndex user, turboshaft::OpIndex node,
    InstructionSelector* selector, Zone* zone,
    ZoneVector<CompareChainNode*>& nodes, bool supports_float_cmp,
    bool supports_test_pattern);

// Extract the operands for a flag-setting node. For TEST nodes,
// the operands come from the inner BitwiseAnd; for CMP nodes,
// they come directly from the ComparisonOp.
void GetFlagSettingOperands(const CompareChainNode* node,
                            InstructionSelector* selector,
                            turboshaft::OpIndex* out_lhs,
                            turboshaft::OpIndex* out_rhs,
                            turboshaft::RegisterRepresentation* out_rep);

class CompareSequence {
 public:
  void InitialCompare(turboshaft::OpIndex op, turboshaft::OpIndex l,
                      turboshaft::OpIndex r, InstructionCode opcode) {
    DCHECK(!HasCompare());
    cmp_ = op;
    left_ = l;
    right_ = r;
    opcode_ = opcode;
  }
  bool HasCompare() const { return cmp_.valid(); }
  turboshaft::OpIndex cmp() const { return cmp_; }
  turboshaft::OpIndex left() const { return left_; }
  turboshaft::OpIndex right() const { return right_; }
  InstructionCode opcode() const { return opcode_; }
  uint32_t num_ccmps() const { return num_ccmps_; }
  FlagsContinuation::compare_chain_t& ccmps() { return ccmps_; }
  void AddConditionalCompare(InstructionCode code,
                             FlagsCondition ccmp_condition,
                             FlagsCondition default_flags,
                             turboshaft::OpIndex ccmp_lhs,
                             turboshaft::OpIndex ccmp_rhs) {
    ccmps_.at(num_ccmps_) = FlagsContinuation::ConditionalCompare{
        code, ccmp_condition, default_flags, ccmp_lhs, ccmp_rhs};
    ++num_ccmps_;
  }

 private:
  turboshaft::OpIndex cmp_;
  turboshaft::OpIndex left_;
  turboshaft::OpIndex right_;
  InstructionCode opcode_;
  FlagsContinuation::compare_chain_t ccmps_;
  uint32_t num_ccmps_ = 0;
};

using GetOpcodeFunc =
    InstructionCode (*)(turboshaft::RegisterRepresentation rep, bool is_test);

// Callback to optionally reorder the initial cmp/ccmp pair based on
// architecture-specific immediate range constraints.
// Called when sequence->HasCompare() is false (i.e., the first logic node).
// May swap lhs and rhs if doing so is beneficial for immediate encoding.
using AdjustInitialOrderFunc = void (*)(const CompareChainNode*& lhs,
                                        const CompareChainNode*& rhs,
                                        InstructionSelector* selector);

// Callback to optionally adjust ccmp operands (e.g., swap lhs/rhs when
// lhs fits in a smaller immediate range on ARM64).
// Called for each conditional compare added to the chain.
// May swap ccmp_lhs/ccmp_rhs and commute user_condition/default_flags.
using AdjustCcmpOperandsFunc = void (*)(turboshaft::OpIndex& ccmp_lhs,
                                        turboshaft::OpIndex& ccmp_rhs,
                                        FlagsCondition& user_condition,
                                        FlagsCondition& default_flags,
                                        InstructionSelector* selector);

void CombineFlagSettingOps(CompareChainNode* logic_node,
                           InstructionSelector* selector,
                           CompareSequence* sequence, GetOpcodeFunc get_opcode,
                           AdjustInitialOrderFunc adjust_initial_order,
                           AdjustCcmpOperandsFunc adjust_ccmp_operands);

// Build a conditional compare chain from the given node.
// Finds the compare chain, validates it, runs CombineFlagSettingOps on all
// logic nodes, and computes the final condition. The caller is responsible
// for emitting the actual instructions via an arch-specific VisitCompareChain.
bool TryBuildConditionalCompareChain(
    InstructionSelector* selector, Zone* zone, turboshaft::OpIndex node,
    FlagsContinuation* cont, CompareSequence* sequence,
    FlagsCondition* condition, GetOpcodeFunc get_opcode,
    bool supports_float_cmp, bool supports_test_pattern,
    AdjustInitialOrderFunc adjust_initial_order,
    AdjustCcmpOperandsFunc adjust_ccmp_operands);

}  // namespace compare_chain

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_IMPL_H_
