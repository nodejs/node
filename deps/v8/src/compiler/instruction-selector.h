// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_SELECTOR_H_
#define V8_COMPILER_INSTRUCTION_SELECTOR_H_

#include <map>

#include "src/compiler/common-operator.h"
#include "src/compiler/instruction-scheduler.h"
#include "src/compiler/instruction.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/globals.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class BasicBlock;
struct CallBuffer;  // TODO(bmeurer): Remove this.
class Linkage;
class OperandGenerator;
class SwitchInfo;
class StateObjectDeduplicator;

// The flags continuation is a way to combine a branch or a materialization
// of a boolean value with an instruction that sets the flags register.
// The whole instruction is treated as a unit by the register allocator, and
// thus no spills or moves can be introduced between the flags-setting
// instruction and the branch or set it should be combined with.
class FlagsContinuation final {
 public:
  FlagsContinuation() : mode_(kFlags_none) {}

  // Creates a new flags continuation from the given condition and true/false
  // blocks.
  static FlagsContinuation ForBranch(FlagsCondition condition,
                                     BasicBlock* true_block,
                                     BasicBlock* false_block) {
    return FlagsContinuation(kFlags_branch, condition, true_block, false_block);
  }

  static FlagsContinuation ForBranchAndPoison(FlagsCondition condition,
                                              BasicBlock* true_block,
                                              BasicBlock* false_block) {
    return FlagsContinuation(kFlags_branch_and_poison, condition, true_block,
                             false_block);
  }

  // Creates a new flags continuation for an eager deoptimization exit.
  static FlagsContinuation ForDeoptimize(FlagsCondition condition,
                                         DeoptimizeKind kind,
                                         DeoptimizeReason reason,
                                         VectorSlotPair const& feedback,
                                         Node* frame_state) {
    return FlagsContinuation(kFlags_deoptimize, condition, kind, reason,
                             feedback, frame_state);
  }

  // Creates a new flags continuation for an eager deoptimization exit.
  static FlagsContinuation ForDeoptimizeAndPoison(
      FlagsCondition condition, DeoptimizeKind kind, DeoptimizeReason reason,
      VectorSlotPair const& feedback, Node* frame_state) {
    return FlagsContinuation(kFlags_deoptimize_and_poison, condition, kind,
                             reason, feedback, frame_state);
  }

  // Creates a new flags continuation for a boolean value.
  static FlagsContinuation ForSet(FlagsCondition condition, Node* result) {
    return FlagsContinuation(condition, result);
  }

  // Creates a new flags continuation for a wasm trap.
  static FlagsContinuation ForTrap(FlagsCondition condition, TrapId trap_id,
                                   Node* result) {
    return FlagsContinuation(condition, trap_id, result);
  }

  bool IsNone() const { return mode_ == kFlags_none; }
  bool IsBranch() const {
    return mode_ == kFlags_branch || mode_ == kFlags_branch_and_poison;
  }
  bool IsDeoptimize() const {
    return mode_ == kFlags_deoptimize || mode_ == kFlags_deoptimize_and_poison;
  }
  bool IsPoisoned() const {
    return mode_ == kFlags_branch_and_poison ||
           mode_ == kFlags_deoptimize_and_poison;
  }
  bool IsSet() const { return mode_ == kFlags_set; }
  bool IsTrap() const { return mode_ == kFlags_trap; }
  FlagsCondition condition() const {
    DCHECK(!IsNone());
    return condition_;
  }
  DeoptimizeKind kind() const {
    DCHECK(IsDeoptimize());
    return kind_;
  }
  DeoptimizeReason reason() const {
    DCHECK(IsDeoptimize());
    return reason_;
  }
  VectorSlotPair const& feedback() const {
    DCHECK(IsDeoptimize());
    return feedback_;
  }
  Node* frame_state() const {
    DCHECK(IsDeoptimize());
    return frame_state_or_result_;
  }
  Node* result() const {
    DCHECK(IsSet());
    return frame_state_or_result_;
  }
  TrapId trap_id() const {
    DCHECK(IsTrap());
    return trap_id_;
  }
  BasicBlock* true_block() const {
    DCHECK(IsBranch());
    return true_block_;
  }
  BasicBlock* false_block() const {
    DCHECK(IsBranch());
    return false_block_;
  }

  void Negate() {
    DCHECK(!IsNone());
    condition_ = NegateFlagsCondition(condition_);
  }

  void Commute() {
    DCHECK(!IsNone());
    condition_ = CommuteFlagsCondition(condition_);
  }

  void Overwrite(FlagsCondition condition) { condition_ = condition; }

  void OverwriteAndNegateIfEqual(FlagsCondition condition) {
    DCHECK(condition_ == kEqual || condition_ == kNotEqual);
    bool negate = condition_ == kEqual;
    condition_ = condition;
    if (negate) Negate();
  }

  void OverwriteUnsignedIfSigned() {
    switch (condition_) {
      case kSignedLessThan:
        condition_ = kUnsignedLessThan;
        break;
      case kSignedLessThanOrEqual:
        condition_ = kUnsignedLessThanOrEqual;
        break;
      case kSignedGreaterThan:
        condition_ = kUnsignedGreaterThan;
        break;
      case kSignedGreaterThanOrEqual:
        condition_ = kUnsignedGreaterThanOrEqual;
        break;
      default:
        break;
    }
  }

  // Encodes this flags continuation into the given opcode.
  InstructionCode Encode(InstructionCode opcode) {
    opcode |= FlagsModeField::encode(mode_);
    if (mode_ != kFlags_none) {
      opcode |= FlagsConditionField::encode(condition_);
    }
    return opcode;
  }

 private:
  FlagsContinuation(FlagsMode mode, FlagsCondition condition,
                    BasicBlock* true_block, BasicBlock* false_block)
      : mode_(mode),
        condition_(condition),
        true_block_(true_block),
        false_block_(false_block) {
    DCHECK(mode == kFlags_branch || mode == kFlags_branch_and_poison);
    DCHECK_NOT_NULL(true_block);
    DCHECK_NOT_NULL(false_block);
  }

  FlagsContinuation(FlagsMode mode, FlagsCondition condition,
                    DeoptimizeKind kind, DeoptimizeReason reason,
                    VectorSlotPair const& feedback, Node* frame_state)
      : mode_(mode),
        condition_(condition),
        kind_(kind),
        reason_(reason),
        feedback_(feedback),
        frame_state_or_result_(frame_state) {
    DCHECK(mode == kFlags_deoptimize || mode == kFlags_deoptimize_and_poison);
    DCHECK_NOT_NULL(frame_state);
  }

  FlagsContinuation(FlagsCondition condition, Node* result)
      : mode_(kFlags_set),
        condition_(condition),
        frame_state_or_result_(result) {
    DCHECK_NOT_NULL(result);
  }

  FlagsContinuation(FlagsCondition condition, TrapId trap_id, Node* result)
      : mode_(kFlags_trap),
        condition_(condition),
        frame_state_or_result_(result),
        trap_id_(trap_id) {
    DCHECK_NOT_NULL(result);
  }

  FlagsMode const mode_;
  FlagsCondition condition_;
  DeoptimizeKind kind_;          // Only valid if mode_ == kFlags_deoptimize*
  DeoptimizeReason reason_;      // Only valid if mode_ == kFlags_deoptimize*
  VectorSlotPair feedback_;      // Only valid if mode_ == kFlags_deoptimize*
  Node* frame_state_or_result_;  // Only valid if mode_ == kFlags_deoptimize*
                                 // or mode_ == kFlags_set.
  BasicBlock* true_block_;       // Only valid if mode_ == kFlags_branch*.
  BasicBlock* false_block_;      // Only valid if mode_ == kFlags_branch*.
  TrapId trap_id_;               // Only valid if mode_ == kFlags_trap.
};

// This struct connects nodes of parameters which are going to be pushed on the
// call stack with their parameter index in the call descriptor of the callee.
struct PushParameter {
  PushParameter(Node* n = nullptr,
                LinkageLocation l = LinkageLocation::ForAnyRegister())
      : node(n), location(l) {}

  Node* node;
  LinkageLocation location;
};

enum class FrameStateInputKind { kAny, kStackSlot };

// Instruction selection generates an InstructionSequence for a given Schedule.
class V8_EXPORT_PRIVATE InstructionSelector final {
 public:
  // Forward declarations.
  class Features;

  enum SourcePositionMode { kCallSourcePositions, kAllSourcePositions };
  enum EnableScheduling { kDisableScheduling, kEnableScheduling };
  enum EnableRootsRelativeAddressing {
    kDisableRootsRelativeAddressing,
    kEnableRootsRelativeAddressing
  };
  enum EnableSwitchJumpTable {
    kDisableSwitchJumpTable,
    kEnableSwitchJumpTable
  };
  enum EnableTraceTurboJson { kDisableTraceTurboJson, kEnableTraceTurboJson };

  InstructionSelector(
      Zone* zone, size_t node_count, Linkage* linkage,
      InstructionSequence* sequence, Schedule* schedule,
      SourcePositionTable* source_positions, Frame* frame,
      EnableSwitchJumpTable enable_switch_jump_table,
      SourcePositionMode source_position_mode = kCallSourcePositions,
      Features features = SupportedFeatures(),
      EnableScheduling enable_scheduling = FLAG_turbo_instruction_scheduling
                                               ? kEnableScheduling
                                               : kDisableScheduling,
      EnableRootsRelativeAddressing enable_roots_relative_addressing =
          kDisableRootsRelativeAddressing,
      PoisoningMitigationLevel poisoning_level =
          PoisoningMitigationLevel::kDontPoison,
      EnableTraceTurboJson trace_turbo = kDisableTraceTurboJson);

  // Visit code for the entire graph with the included schedule.
  bool SelectInstructions();

  void StartBlock(RpoNumber rpo);
  void EndBlock(RpoNumber rpo);
  void AddInstruction(Instruction* instr);
  void AddTerminator(Instruction* instr);

  // ===========================================================================
  // ============= Architecture-independent code emission methods. =============
  // ===========================================================================

  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, InstructionOperand d,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, InstructionOperand d,
                    InstructionOperand e, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, InstructionOperand output,
                    InstructionOperand a, InstructionOperand b,
                    InstructionOperand c, InstructionOperand d,
                    InstructionOperand e, InstructionOperand f,
                    size_t temp_count = 0, InstructionOperand* temps = nullptr);
  Instruction* Emit(InstructionCode opcode, size_t output_count,
                    InstructionOperand* outputs, size_t input_count,
                    InstructionOperand* inputs, size_t temp_count = 0,
                    InstructionOperand* temps = nullptr);
  Instruction* Emit(Instruction* instr);

  // [0-3] operand instructions with no output, uses labels for true and false
  // blocks of the continuation.
  Instruction* EmitWithContinuation(InstructionCode opcode,
                                    FlagsContinuation* cont);
  Instruction* EmitWithContinuation(InstructionCode opcode,
                                    InstructionOperand a,
                                    FlagsContinuation* cont);
  Instruction* EmitWithContinuation(InstructionCode opcode,
                                    InstructionOperand a, InstructionOperand b,
                                    FlagsContinuation* cont);
  Instruction* EmitWithContinuation(InstructionCode opcode,
                                    InstructionOperand a, InstructionOperand b,
                                    InstructionOperand c,
                                    FlagsContinuation* cont);
  Instruction* EmitWithContinuation(InstructionCode opcode, size_t output_count,
                                    InstructionOperand* outputs,
                                    size_t input_count,
                                    InstructionOperand* inputs,
                                    FlagsContinuation* cont);

  // ===========================================================================
  // ===== Architecture-independent deoptimization exit emission methods. ======
  // ===========================================================================
  Instruction* EmitDeoptimize(InstructionCode opcode, size_t output_count,
                              InstructionOperand* outputs, size_t input_count,
                              InstructionOperand* inputs, DeoptimizeKind kind,
                              DeoptimizeReason reason,
                              VectorSlotPair const& feedback,
                              Node* frame_state);

  // ===========================================================================
  // ============== Architecture-independent CPU feature methods. ==============
  // ===========================================================================

  class Features final {
   public:
    Features() : bits_(0) {}
    explicit Features(unsigned bits) : bits_(bits) {}
    explicit Features(CpuFeature f) : bits_(1u << f) {}
    Features(CpuFeature f1, CpuFeature f2) : bits_((1u << f1) | (1u << f2)) {}

    bool Contains(CpuFeature f) const { return (bits_ & (1u << f)); }

   private:
    unsigned bits_;
  };

  bool IsSupported(CpuFeature feature) const {
    return features_.Contains(feature);
  }

  // Returns the features supported on the target platform.
  static Features SupportedFeatures() {
    return Features(CpuFeatures::SupportedFeatures());
  }

  // TODO(sigurds) This should take a CpuFeatures argument.
  static MachineOperatorBuilder::Flags SupportedMachineOperatorFlags();

  static MachineOperatorBuilder::AlignmentRequirements AlignmentRequirements();

  bool NeedsPoisoning(IsSafetyCheck safety_check) const;

  // ===========================================================================
  // ============ Architecture-independent graph covering methods. =============
  // ===========================================================================

  // Used in pattern matching during code generation.
  // Check if {node} can be covered while generating code for the current
  // instruction. A node can be covered if the {user} of the node has the only
  // edge and the two are in the same basic block.
  bool CanCover(Node* user, Node* node) const;

  // Used in pattern matching during code generation.
  // This function checks that {node} and {user} are in the same basic block,
  // and that {user} is the only user of {node} in this basic block.  This
  // check guarantees that there are no users of {node} scheduled between
  // {node} and {user}, and thus we can select a single instruction for both
  // nodes, if such an instruction exists. This check can be used for example
  // when selecting instructions for:
  //   n = Int32Add(a, b)
  //   c = Word32Compare(n, 0, cond)
  //   Branch(c, true_label, false_label)
  // Here we can generate a flag-setting add instruction, even if the add has
  // uses in other basic blocks, since the flag-setting add instruction will
  // still generate the result of the addition and not just set the flags.
  // However, if we had uses of the add in the same basic block, we could have:
  //   n = Int32Add(a, b)
  //   o = OtherOp(n, ...)
  //   c = Word32Compare(n, 0, cond)
  //   Branch(c, true_label, false_label)
  // where we cannot select the add and the compare together.  If we were to
  // select a flag-setting add instruction for Word32Compare and Int32Add while
  // visiting Word32Compare, we would then have to select an instruction for
  // OtherOp *afterwards*, which means we would attempt to use the result of
  // the add before we have defined it.
  bool IsOnlyUserOfNodeInSameBlock(Node* user, Node* node) const;

  // Checks if {node} was already defined, and therefore code was already
  // generated for it.
  bool IsDefined(Node* node) const;

  // Checks if {node} has any uses, and therefore code has to be generated for
  // it.
  bool IsUsed(Node* node) const;

  // Checks if {node} is currently live.
  bool IsLive(Node* node) const { return !IsDefined(node) && IsUsed(node); }

  // Gets the effect level of {node}.
  int GetEffectLevel(Node* node) const;

  int GetVirtualRegister(const Node* node);
  const std::map<NodeId, int> GetVirtualRegistersForTesting() const;

  // Check if we can generate loads and stores of ExternalConstants relative
  // to the roots register.
  bool CanAddressRelativeToRootsRegister() const;
  // Check if we can use the roots register to access GC roots.
  bool CanUseRootsRegister() const;

  Isolate* isolate() const { return sequence()->isolate(); }

  const ZoneVector<std::pair<int, int>>& instr_origins() const {
    return instr_origins_;
  }

  // Expose these SIMD helper functions for testing.
  static void CanonicalizeShuffleForTesting(bool inputs_equal, uint8_t* shuffle,
                                            bool* needs_swap,
                                            bool* is_swizzle) {
    CanonicalizeShuffle(inputs_equal, shuffle, needs_swap, is_swizzle);
  }

  static bool TryMatchIdentityForTesting(const uint8_t* shuffle) {
    return TryMatchIdentity(shuffle);
  }
  template <int LANES>
  static bool TryMatchDupForTesting(const uint8_t* shuffle, int* index) {
    return TryMatchDup<LANES>(shuffle, index);
  }
  static bool TryMatch32x4ShuffleForTesting(const uint8_t* shuffle,
                                            uint8_t* shuffle32x4) {
    return TryMatch32x4Shuffle(shuffle, shuffle32x4);
  }
  static bool TryMatch16x8ShuffleForTesting(const uint8_t* shuffle,
                                            uint8_t* shuffle16x8) {
    return TryMatch16x8Shuffle(shuffle, shuffle16x8);
  }
  static bool TryMatchConcatForTesting(const uint8_t* shuffle,
                                       uint8_t* offset) {
    return TryMatchConcat(shuffle, offset);
  }
  static bool TryMatchBlendForTesting(const uint8_t* shuffle) {
    return TryMatchBlend(shuffle);
  }

 private:
  friend class OperandGenerator;

  bool UseInstructionScheduling() const {
    return (enable_scheduling_ == kEnableScheduling) &&
           InstructionScheduler::SchedulerSupported();
  }

  void AppendDeoptimizeArguments(InstructionOperandVector* args,
                                 DeoptimizeKind kind, DeoptimizeReason reason,
                                 VectorSlotPair const& feedback,
                                 Node* frame_state);

  void EmitTableSwitch(const SwitchInfo& sw, InstructionOperand& index_operand);
  void EmitLookupSwitch(const SwitchInfo& sw,
                        InstructionOperand& value_operand);
  void EmitBinarySearchSwitch(const SwitchInfo& sw,
                              InstructionOperand& value_operand);

  void TryRename(InstructionOperand* op);
  int GetRename(int virtual_register);
  void SetRename(const Node* node, const Node* rename);
  void UpdateRenames(Instruction* instruction);
  void UpdateRenamesInPhi(PhiInstruction* phi);

  // Inform the instruction selection that {node} was just defined.
  void MarkAsDefined(Node* node);

  // Inform the instruction selection that {node} has at least one use and we
  // will need to generate code for it.
  void MarkAsUsed(Node* node);

  // Sets the effect level of {node}.
  void SetEffectLevel(Node* node, int effect_level);

  // Inform the register allocation of the representation of the value produced
  // by {node}.
  void MarkAsRepresentation(MachineRepresentation rep, Node* node);
  void MarkAsWord32(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kWord32, node);
  }
  void MarkAsWord64(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kWord64, node);
  }
  void MarkAsFloat32(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kFloat32, node);
  }
  void MarkAsFloat64(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kFloat64, node);
  }
  void MarkAsSimd128(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kSimd128, node);
  }
  void MarkAsReference(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kTagged, node);
  }

  // Inform the register allocation of the representation of the unallocated
  // operand {op}.
  void MarkAsRepresentation(MachineRepresentation rep,
                            const InstructionOperand& op);

  enum CallBufferFlag {
    kCallCodeImmediate = 1u << 0,
    kCallAddressImmediate = 1u << 1,
    kCallTail = 1u << 2,
    kCallFixedTargetRegister = 1u << 3,
  };
  typedef base::Flags<CallBufferFlag> CallBufferFlags;

  // Initialize the call buffer with the InstructionOperands, nodes, etc,
  // corresponding
  // to the inputs and outputs of the call.
  // {call_code_immediate} to generate immediate operands to calls of code.
  // {call_address_immediate} to generate immediate operands to address calls.
  void InitializeCallBuffer(Node* call, CallBuffer* buffer,
                            CallBufferFlags flags, bool is_tail_call,
                            int stack_slot_delta = 0);
  bool IsTailCallAddressImmediate();
  int GetTempsCountForTailCallFromJSFunction();

  FrameStateDescriptor* GetFrameStateDescriptor(Node* node);
  size_t AddInputsToFrameStateDescriptor(FrameStateDescriptor* descriptor,
                                         Node* state, OperandGenerator* g,
                                         StateObjectDeduplicator* deduplicator,
                                         InstructionOperandVector* inputs,
                                         FrameStateInputKind kind, Zone* zone);
  size_t AddOperandToStateValueDescriptor(StateValueList* values,
                                          InstructionOperandVector* inputs,
                                          OperandGenerator* g,
                                          StateObjectDeduplicator* deduplicator,
                                          Node* input, MachineType type,
                                          FrameStateInputKind kind, Zone* zone);

  // ===========================================================================
  // ============= Architecture-specific graph covering methods. ===============
  // ===========================================================================

  // Visit nodes in the given block and generate code.
  void VisitBlock(BasicBlock* block);

  // Visit the node for the control flow at the end of the block, generating
  // code if necessary.
  void VisitControl(BasicBlock* block);

  // Visit the node and generate code, if any.
  void VisitNode(Node* node);

  // Visit the node and generate code for IEEE 754 functions.
  void VisitFloat64Ieee754Binop(Node*, InstructionCode code);
  void VisitFloat64Ieee754Unop(Node*, InstructionCode code);

#define DECLARE_GENERATOR(x) void Visit##x(Node* node);
  MACHINE_OP_LIST(DECLARE_GENERATOR)
  MACHINE_SIMD_OP_LIST(DECLARE_GENERATOR)
#undef DECLARE_GENERATOR

  void VisitFinishRegion(Node* node);
  void VisitParameter(Node* node);
  void VisitIfException(Node* node);
  void VisitOsrValue(Node* node);
  void VisitPhi(Node* node);
  void VisitProjection(Node* node);
  void VisitConstant(Node* node);
  void VisitCall(Node* call, BasicBlock* handler = nullptr);
  void VisitCallWithCallerSavedRegisters(Node* call,
                                         BasicBlock* handler = nullptr);
  void VisitDeoptimizeIf(Node* node);
  void VisitDeoptimizeUnless(Node* node);
  void VisitTrapIf(Node* node, TrapId trap_id);
  void VisitTrapUnless(Node* node, TrapId trap_id);
  void VisitTailCall(Node* call);
  void VisitGoto(BasicBlock* target);
  void VisitBranch(Node* input, BasicBlock* tbranch, BasicBlock* fbranch);
  void VisitSwitch(Node* node, const SwitchInfo& sw);
  void VisitDeoptimize(DeoptimizeKind kind, DeoptimizeReason reason,
                       VectorSlotPair const& feedback, Node* value);
  void VisitReturn(Node* ret);
  void VisitThrow(Node* node);
  void VisitRetain(Node* node);
  void VisitUnreachable(Node* node);
  void VisitDeadValue(Node* node);

  void VisitWordCompareZero(Node* user, Node* value, FlagsContinuation* cont);

  void EmitWordPoisonOnSpeculation(Node* node);

  void EmitPrepareArguments(ZoneVector<compiler::PushParameter>* arguments,
                            const CallDescriptor* call_descriptor, Node* node);
  void EmitPrepareResults(ZoneVector<compiler::PushParameter>* results,
                          const CallDescriptor* call_descriptor, Node* node);

  void EmitIdentity(Node* node);
  bool CanProduceSignalingNaN(Node* node);

  // ===========================================================================
  // ============= Vector instruction (SIMD) helper fns. =======================
  // ===========================================================================

  // Converts a shuffle into canonical form, meaning that the first lane index
  // is in the range [0 .. 15]. Set |inputs_equal| true if this is an explicit
  // swizzle. Returns canonicalized |shuffle|, |needs_swap|, and |is_swizzle|.
  // If |needs_swap| is true, inputs must be swapped. If |is_swizzle| is true,
  // the second input can be ignored.
  static void CanonicalizeShuffle(bool inputs_equal, uint8_t* shuffle,
                                  bool* needs_swap, bool* is_swizzle);

  // Canonicalize shuffles to make pattern matching simpler. Returns the shuffle
  // indices, and a boolean indicating if the shuffle is a swizzle (one input).
  void CanonicalizeShuffle(Node* node, uint8_t* shuffle, bool* is_swizzle);

  // Swaps the two first input operands of the node, to help match shuffles
  // to specific architectural instructions.
  void SwapShuffleInputs(Node* node);

  // Tries to match an 8x16 byte shuffle to the identity shuffle, which is
  // [0 1 ... 15]. This should be called after canonicalizing the shuffle, so
  // the second identity shuffle, [16 17 .. 31] is converted to the first one.
  static bool TryMatchIdentity(const uint8_t* shuffle);

  // Tries to match a byte shuffle to a scalar splat operation. Returns the
  // index of the lane if successful.
  template <int LANES>
  static bool TryMatchDup(const uint8_t* shuffle, int* index) {
    const int kBytesPerLane = kSimd128Size / LANES;
    // Get the first lane's worth of bytes and check that indices start at a
    // lane boundary and are consecutive.
    uint8_t lane0[kBytesPerLane];
    lane0[0] = shuffle[0];
    if (lane0[0] % kBytesPerLane != 0) return false;
    for (int i = 1; i < kBytesPerLane; ++i) {
      lane0[i] = shuffle[i];
      if (lane0[i] != lane0[0] + i) return false;
    }
    // Now check that the other lanes are identical to lane0.
    for (int i = 1; i < LANES; ++i) {
      for (int j = 0; j < kBytesPerLane; ++j) {
        if (lane0[j] != shuffle[i * kBytesPerLane + j]) return false;
      }
    }
    *index = lane0[0] / kBytesPerLane;
    return true;
  }

  // Tries to match an 8x16 byte shuffle to an equivalent 32x4 shuffle. If
  // successful, it writes the 32x4 shuffle word indices. E.g.
  // [0 1 2 3 8 9 10 11 4 5 6 7 12 13 14 15] == [0 2 1 3]
  static bool TryMatch32x4Shuffle(const uint8_t* shuffle, uint8_t* shuffle32x4);

  // Tries to match an 8x16 byte shuffle to an equivalent 16x8 shuffle. If
  // successful, it writes the 16x8 shuffle word indices. E.g.
  // [0 1 8 9 2 3 10 11 4 5 12 13 6 7 14 15] == [0 4 1 5 2 6 3 7]
  static bool TryMatch16x8Shuffle(const uint8_t* shuffle, uint8_t* shuffle16x8);

  // Tries to match a byte shuffle to a concatenate operation, formed by taking
  // 16 bytes from the 32 byte concatenation of the inputs.  If successful, it
  // writes the byte offset. E.g. [4 5 6 7 .. 16 17 18 19] concatenates both
  // source vectors with offset 4. The shuffle should be canonicalized.
  static bool TryMatchConcat(const uint8_t* shuffle, uint8_t* offset);

  // Tries to match a byte shuffle to a blend operation, which is a shuffle
  // where no lanes change position. E.g. [0 9 2 11 .. 14 31] interleaves the
  // even lanes of the first source with the odd lanes of the second.  The
  // shuffle should be canonicalized.
  static bool TryMatchBlend(const uint8_t* shuffle);

  // Packs 4 bytes of shuffle into a 32 bit immediate.
  static int32_t Pack4Lanes(const uint8_t* shuffle);

  // ===========================================================================

  Schedule* schedule() const { return schedule_; }
  Linkage* linkage() const { return linkage_; }
  InstructionSequence* sequence() const { return sequence_; }
  Zone* instruction_zone() const { return sequence()->zone(); }
  Zone* zone() const { return zone_; }

  void set_instruction_selection_failed() {
    instruction_selection_failed_ = true;
  }
  bool instruction_selection_failed() { return instruction_selection_failed_; }

  void MarkPairProjectionsAsWord32(Node* node);
  bool IsSourcePositionUsed(Node* node);
  void VisitWord32AtomicBinaryOperation(Node* node, ArchOpcode int8_op,
                                        ArchOpcode uint8_op,
                                        ArchOpcode int16_op,
                                        ArchOpcode uint16_op,
                                        ArchOpcode word32_op);
  void VisitWord64AtomicBinaryOperation(Node* node, ArchOpcode uint8_op,
                                        ArchOpcode uint16_op,
                                        ArchOpcode uint32_op,
                                        ArchOpcode uint64_op);
  void VisitWord64AtomicNarrowBinop(Node* node, ArchOpcode uint8_op,
                                    ArchOpcode uint16_op, ArchOpcode uint32_op);

  // ===========================================================================

  Zone* const zone_;
  Linkage* const linkage_;
  InstructionSequence* const sequence_;
  SourcePositionTable* const source_positions_;
  SourcePositionMode const source_position_mode_;
  Features features_;
  Schedule* const schedule_;
  BasicBlock* current_block_;
  ZoneVector<Instruction*> instructions_;
  InstructionOperandVector continuation_inputs_;
  InstructionOperandVector continuation_outputs_;
  BoolVector defined_;
  BoolVector used_;
  IntVector effect_level_;
  IntVector virtual_registers_;
  IntVector virtual_register_rename_;
  InstructionScheduler* scheduler_;
  EnableScheduling enable_scheduling_;
  EnableRootsRelativeAddressing enable_roots_relative_addressing_;
  EnableSwitchJumpTable enable_switch_jump_table_;

  PoisoningMitigationLevel poisoning_level_;
  Frame* frame_;
  bool instruction_selection_failed_;
  ZoneVector<std::pair<int, int>> instr_origins_;
  EnableTraceTurboJson trace_turbo_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_SELECTOR_H_
