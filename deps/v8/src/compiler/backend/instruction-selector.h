// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_H_
#define V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_H_

#include <map>

#include "src/codegen/cpu-features.h"
#include "src/compiler/backend/instruction-scheduler.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/utils/bit-vector.h"
#include "src/zone/zone-containers.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/simd-shuffle.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

class TickCounter;

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

  // Creates a new flags continuation for an eager deoptimization exit.
  static FlagsContinuation ForDeoptimize(FlagsCondition condition,
                                         DeoptimizeReason reason,
                                         NodeId node_id,
                                         FeedbackSource const& feedback,
                                         FrameState frame_state) {
    return FlagsContinuation(kFlags_deoptimize, condition, reason, node_id,
                             feedback, frame_state);
  }
  static FlagsContinuation ForDeoptimizeForTesting(
      FlagsCondition condition, DeoptimizeReason reason, NodeId node_id,
      FeedbackSource const& feedback, Node* frame_state) {
    // test-instruction-scheduler.cc passes a dummy Node* as frame_state.
    // Contents don't matter as long as it's not nullptr.
    return FlagsContinuation(kFlags_deoptimize, condition, reason, node_id,
                             feedback, frame_state);
  }

  // Creates a new flags continuation for a boolean value.
  static FlagsContinuation ForSet(FlagsCondition condition, Node* result) {
    return FlagsContinuation(condition, result);
  }

  // Creates a new flags continuation for a wasm trap.
  static FlagsContinuation ForTrap(FlagsCondition condition, TrapId trap_id) {
    return FlagsContinuation(condition, trap_id);
  }

  static FlagsContinuation ForSelect(FlagsCondition condition, Node* result,
                                     Node* true_value, Node* false_value) {
    return FlagsContinuation(condition, result, true_value, false_value);
  }

  bool IsNone() const { return mode_ == kFlags_none; }
  bool IsBranch() const { return mode_ == kFlags_branch; }
  bool IsDeoptimize() const { return mode_ == kFlags_deoptimize; }
  bool IsSet() const { return mode_ == kFlags_set; }
  bool IsTrap() const { return mode_ == kFlags_trap; }
  bool IsSelect() const { return mode_ == kFlags_select; }
  FlagsCondition condition() const {
    DCHECK(!IsNone());
    return condition_;
  }
  DeoptimizeReason reason() const {
    DCHECK(IsDeoptimize());
    return reason_;
  }
  NodeId node_id() const {
    DCHECK(IsDeoptimize());
    return node_id_;
  }
  FeedbackSource const& feedback() const {
    DCHECK(IsDeoptimize());
    return feedback_;
  }
  Node* frame_state() const {
    DCHECK(IsDeoptimize());
    return frame_state_or_result_;
  }
  Node* result() const {
    DCHECK(IsSet() || IsSelect());
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
  Node* true_value() const {
    DCHECK(IsSelect());
    return true_value_;
  }
  Node* false_value() const {
    DCHECK(IsSelect());
    return false_value_;
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
    DCHECK(mode == kFlags_branch);
    DCHECK_NOT_NULL(true_block);
    DCHECK_NOT_NULL(false_block);
  }

  FlagsContinuation(FlagsMode mode, FlagsCondition condition,
                    DeoptimizeReason reason, NodeId node_id,
                    FeedbackSource const& feedback, Node* frame_state)
      : mode_(mode),
        condition_(condition),
        reason_(reason),
        node_id_(node_id),
        feedback_(feedback),
        frame_state_or_result_(frame_state) {
    DCHECK(mode == kFlags_deoptimize);
    DCHECK_NOT_NULL(frame_state);
  }

  FlagsContinuation(FlagsCondition condition, Node* result)
      : mode_(kFlags_set),
        condition_(condition),
        frame_state_or_result_(result) {
    DCHECK_NOT_NULL(result);
  }

  FlagsContinuation(FlagsCondition condition, TrapId trap_id)
      : mode_(kFlags_trap), condition_(condition), trap_id_(trap_id) {}

  FlagsContinuation(FlagsCondition condition, Node* result, Node* true_value,
                    Node* false_value)
      : mode_(kFlags_select),
        condition_(condition),
        frame_state_or_result_(result),
        true_value_(true_value),
        false_value_(false_value) {
    DCHECK_NOT_NULL(result);
    DCHECK_NOT_NULL(true_value);
    DCHECK_NOT_NULL(false_value);
  }

  FlagsMode const mode_;
  FlagsCondition condition_;
  DeoptimizeReason reason_;         // Only valid if mode_ == kFlags_deoptimize*
  NodeId node_id_;                  // Only valid if mode_ == kFlags_deoptimize*
  FeedbackSource feedback_;         // Only valid if mode_ == kFlags_deoptimize*
  Node* frame_state_or_result_;     // Only valid if mode_ == kFlags_deoptimize*
                                    // or mode_ == kFlags_set.
  BasicBlock* true_block_;          // Only valid if mode_ == kFlags_branch*.
  BasicBlock* false_block_;         // Only valid if mode_ == kFlags_branch*.
  TrapId trap_id_;                  // Only valid if mode_ == kFlags_trap.
  Node* true_value_;                // Only valid if mode_ == kFlags_select.
  Node* false_value_;               // Only valid if mode_ == kFlags_select.
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
      EnableSwitchJumpTable enable_switch_jump_table, TickCounter* tick_counter,
      JSHeapBroker* broker, size_t* max_unoptimized_frame_height,
      size_t* max_pushed_argument_count,
      SourcePositionMode source_position_mode = kCallSourcePositions,
      Features features = SupportedFeatures(),
      EnableScheduling enable_scheduling = v8_flags.turbo_instruction_scheduling
                                               ? kEnableScheduling
                                               : kDisableScheduling,
      EnableRootsRelativeAddressing enable_roots_relative_addressing =
          kDisableRootsRelativeAddressing,
      EnableTraceTurboJson trace_turbo = kDisableTraceTurboJson);

  // Visit code for the entire graph with the included schedule.
  base::Optional<BailoutReason> SelectInstructions();

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
  Instruction* EmitWithContinuation(
      InstructionCode opcode, size_t output_count, InstructionOperand* outputs,
      size_t input_count, InstructionOperand* inputs, size_t temp_count,
      InstructionOperand* temps, FlagsContinuation* cont);

  void EmitIdentity(Node* node);

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

  // ===========================================================================
  // ============ Architecture-independent graph covering methods. =============
  // ===========================================================================

  // Used in pattern matching during code generation.
  // Check if {node} can be covered while generating code for the current
  // instruction. A node can be covered if the {user} of the node has the only
  // edge, the two are in the same basic block, and there are no side-effects
  // in-between. The last check is crucial for soundness.
  // For pure nodes, CanCover(a,b) is checked to avoid duplicated execution:
  // If this is not the case, code for b must still be generated for other
  // users, and fusing is unlikely to improve performance.
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

  // Gets the effect level of {node}, appropriately adjusted based on
  // continuation flags if the node is a branch.
  int GetEffectLevel(Node* node, FlagsContinuation* cont) const;

  int GetVirtualRegister(const Node* node);
  const std::map<NodeId, int> GetVirtualRegistersForTesting() const;

  // Check if we can generate loads and stores of ExternalConstants relative
  // to the roots register.
  bool CanAddressRelativeToRootsRegister(
      const ExternalReference& reference) const;
  // Check if we can use the roots register to access GC roots.
  bool CanUseRootsRegister() const;

  Isolate* isolate() const { return sequence()->isolate(); }

  const ZoneVector<std::pair<int, int>>& instr_origins() const {
    return instr_origins_;
  }

 private:
  friend class OperandGenerator;

  bool UseInstructionScheduling() const {
    return (enable_scheduling_ == kEnableScheduling) &&
           InstructionScheduler::SchedulerSupported();
  }

  void AppendDeoptimizeArguments(InstructionOperandVector* args,
                                 DeoptimizeReason reason, NodeId node_id,
                                 FeedbackSource const& feedback,
                                 FrameState frame_state);

  void EmitTableSwitch(const SwitchInfo& sw,
                       InstructionOperand const& index_operand);
  void EmitBinarySearchSwitch(const SwitchInfo& sw,
                              InstructionOperand const& value_operand);

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
  void MarkAsSimd256(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kSimd256, node);
  }
  void MarkAsTagged(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kTagged, node);
  }
  void MarkAsCompressed(Node* node) {
    MarkAsRepresentation(MachineRepresentation::kCompressed, node);
  }

  // Inform the register allocation of the representation of the unallocated
  // operand {op}.
  void MarkAsRepresentation(MachineRepresentation rep,
                            const InstructionOperand& op);

  enum CallBufferFlag {
    kCallCodeImmediate = 1u << 0,
    kCallAddressImmediate = 1u << 1,
    kCallTail = 1u << 2,
    kCallFixedTargetRegister = 1u << 3
  };
  using CallBufferFlags = base::Flags<CallBufferFlag>;

  // Initialize the call buffer with the InstructionOperands, nodes, etc,
  // corresponding
  // to the inputs and outputs of the call.
  // {call_code_immediate} to generate immediate operands to calls of code.
  // {call_address_immediate} to generate immediate operands to address calls.
  void InitializeCallBuffer(Node* call, CallBuffer* buffer,
                            CallBufferFlags flags, int stack_slot_delta = 0);
  bool IsTailCallAddressImmediate();

  void UpdateMaxPushedArgumentCount(size_t count);

  FrameStateDescriptor* GetFrameStateDescriptor(FrameState node);
  size_t AddInputsToFrameStateDescriptor(FrameStateDescriptor* descriptor,
                                         FrameState state, OperandGenerator* g,
                                         StateObjectDeduplicator* deduplicator,
                                         InstructionOperandVector* inputs,
                                         FrameStateInputKind kind, Zone* zone);
  size_t AddInputsToFrameStateDescriptor(StateValueList* values,
                                         InstructionOperandVector* inputs,
                                         OperandGenerator* g,
                                         StateObjectDeduplicator* deduplicator,
                                         Node* node, FrameStateInputKind kind,
                                         Zone* zone);
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
  MACHINE_SIMD128_OP_LIST(DECLARE_GENERATOR)
  MACHINE_SIMD256_OP_LIST(DECLARE_GENERATOR)
#undef DECLARE_GENERATOR

  // Visit the load node with a value and opcode to replace with.
  void VisitLoad(Node* node, Node* value, InstructionCode opcode);
  void VisitLoadTransform(Node* node, Node* value, InstructionCode opcode);
  void VisitFinishRegion(Node* node);
  void VisitParameter(Node* node);
  void VisitIfException(Node* node);
  void VisitOsrValue(Node* node);
  void VisitPhi(Node* node);
  void VisitProjection(Node* node);
  void VisitConstant(Node* node);
  void VisitCall(Node* call, BasicBlock* handler = nullptr);
  void VisitDeoptimizeIf(Node* node);
  void VisitDeoptimizeUnless(Node* node);
  void VisitDynamicCheckMapsWithDeoptUnless(Node* node);
  void VisitTrapIf(Node* node, TrapId trap_id);
  void VisitTrapUnless(Node* node, TrapId trap_id);
  void VisitTailCall(Node* call);
  void VisitGoto(BasicBlock* target);
  void VisitBranch(Node* input, BasicBlock* tbranch, BasicBlock* fbranch);
  void VisitSwitch(Node* node, const SwitchInfo& sw);
  void VisitDeoptimize(DeoptimizeReason reason, NodeId node_id,
                       FeedbackSource const& feedback, FrameState frame_state);
  void VisitSelect(Node* node);
  void VisitReturn(Node* ret);
  void VisitThrow(Node* node);
  void VisitRetain(Node* node);
  void VisitUnreachable(Node* node);
  void VisitStaticAssert(Node* node);
  void VisitDeadValue(Node* node);

  void TryPrepareScheduleFirstProjection(Node* maybe_projection);

  void VisitStackPointerGreaterThan(Node* node, FlagsContinuation* cont);

  void VisitWordCompareZero(Node* user, Node* value, FlagsContinuation* cont);

  void EmitPrepareArguments(ZoneVector<compiler::PushParameter>* arguments,
                            const CallDescriptor* call_descriptor, Node* node);
  void EmitPrepareResults(ZoneVector<compiler::PushParameter>* results,
                          const CallDescriptor* call_descriptor, Node* node);

  // In LOONG64, calling convention uses free GP param register to pass
  // floating-point arguments when no FP param register is available. But
  // gap does not support moving from FPR to GPR, so we add EmitMoveFPRToParam
  // to complete movement.
  void EmitMoveFPRToParam(InstructionOperand* op, LinkageLocation location);
  // Moving floating-point param from GP param register to FPR to participate in
  // subsequent operations, whether CallCFunction or normal floating-point
  // operations.
  void EmitMoveParamToFPR(Node* node, int index);

  bool CanProduceSignalingNaN(Node* node);

  void AddOutputToSelectContinuation(OperandGenerator* g, int first_input_index,
                                     Node* node);

  // ===========================================================================
  // ============= Vector instruction (SIMD) helper fns. =======================
  // ===========================================================================

#if V8_ENABLE_WEBASSEMBLY
  // Canonicalize shuffles to make pattern matching simpler. Returns the shuffle
  // indices, and a boolean indicating if the shuffle is a swizzle (one input).
  void CanonicalizeShuffle(Node* node, uint8_t* shuffle, bool* is_swizzle);

  // Swaps the two first input operands of the node, to help match shuffles
  // to specific architectural instructions.
  void SwapShuffleInputs(Node* node);
#endif  // V8_ENABLE_WEBASSEMBLY

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

#if V8_TARGET_ARCH_64_BIT
  bool ZeroExtendsWord32ToWord64(Node* node, int recursion_depth = 0);
  bool ZeroExtendsWord32ToWord64NoPhis(Node* node);

  enum Upper32BitsState : uint8_t {
    kNotYetChecked,
    kUpperBitsGuaranteedZero,
    kNoGuarantee,
  };
#endif  // V8_TARGET_ARCH_64_BIT

  struct FrameStateInput {
    FrameStateInput(Node* node_, FrameStateInputKind kind_)
        : node(node_), kind(kind_) {}

    Node* node;
    FrameStateInputKind kind;

    struct Hash {
      size_t operator()(FrameStateInput const& source) const {
        return base::hash_combine(source.node,
                                  static_cast<size_t>(source.kind));
      }
    };

    struct Equal {
      bool operator()(FrameStateInput const& lhs,
                      FrameStateInput const& rhs) const {
        return lhs.node == rhs.node && lhs.kind == rhs.kind;
      }
    };
  };

  struct CachedStateValues;
  class CachedStateValuesBuilder;

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
  InstructionOperandVector continuation_temps_;
  BitVector defined_;
  BitVector used_;
  IntVector effect_level_;
  int current_effect_level_;
  IntVector virtual_registers_;
  IntVector virtual_register_rename_;
  InstructionScheduler* scheduler_;
  EnableScheduling enable_scheduling_;
  EnableRootsRelativeAddressing enable_roots_relative_addressing_;
  EnableSwitchJumpTable enable_switch_jump_table_;
  ZoneUnorderedMap<FrameStateInput, CachedStateValues*, FrameStateInput::Hash,
                   FrameStateInput::Equal>
      state_values_cache_;

  Frame* frame_;
  bool instruction_selection_failed_;
  ZoneVector<std::pair<int, int>> instr_origins_;
  EnableTraceTurboJson trace_turbo_;
  TickCounter* const tick_counter_;
  // The broker is only used for unparking the LocalHeap for diagnostic printing
  // for failed StaticAsserts.
  JSHeapBroker* const broker_;

  // Store the maximal unoptimized frame height and an maximal number of pushed
  // arguments (for calls). Later used to apply an offset to stack checks.
  size_t* max_unoptimized_frame_height_;
  size_t* max_pushed_argument_count_;

#if V8_TARGET_ARCH_64_BIT
  // Holds lazily-computed results for whether phi nodes guarantee their upper
  // 32 bits to be zero. Indexed by node ID; nobody reads or writes the values
  // for non-phi nodes.
  ZoneVector<Upper32BitsState> phi_states_;
#endif
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_H_
