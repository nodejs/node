// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_GENERATOR_H_
#define V8_COMPILER_CODE_GENERATOR_H_

#include "src/base/optional.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/instruction.h"
#include "src/compiler/osr.h"
#include "src/compiler/unwinding-info-writer.h"
#include "src/deoptimizer.h"
#include "src/macro-assembler.h"
#include "src/safepoint-table.h"
#include "src/source-position-table.h"

namespace v8 {
namespace internal {

class CompilationInfo;

namespace trap_handler {
struct ProtectedInstructionData;
}  // namespace trap_handler

namespace compiler {

// Forward declarations.
class DeoptimizationExit;
class FrameAccessState;
class Linkage;
class OutOfLineCode;

struct BranchInfo {
  FlagsCondition condition;
  Label* true_label;
  Label* false_label;
  bool fallthru;
};


class InstructionOperandIterator {
 public:
  InstructionOperandIterator(Instruction* instr, size_t pos)
      : instr_(instr), pos_(pos) {}

  Instruction* instruction() const { return instr_; }
  InstructionOperand* Advance() { return instr_->InputAt(pos_++); }

 private:
  Instruction* instr_;
  size_t pos_;
};

// Either a non-null Handle<Object> or a double.
class DeoptimizationLiteral {
 public:
  DeoptimizationLiteral() : object_(), number_(0) {}
  explicit DeoptimizationLiteral(Handle<Object> object)
      : object_(object), number_(0) {
    DCHECK(!object_.is_null());
  }
  explicit DeoptimizationLiteral(double number) : object_(), number_(number) {}

  Handle<Object> object() const { return object_; }

  bool operator==(const DeoptimizationLiteral& other) const {
    return object_.equals(other.object_) &&
           bit_cast<uint64_t>(number_) == bit_cast<uint64_t>(other.number_);
  }

  Handle<Object> Reify(Isolate* isolate) const;

 private:
  Handle<Object> object_;
  double number_;
};

// Generates native code for a sequence of instructions.
class CodeGenerator final : public GapResolver::Assembler {
 public:
  explicit CodeGenerator(Zone* codegen_zone, Frame* frame, Linkage* linkage,
                         InstructionSequence* code, CompilationInfo* info,
                         Isolate* isolate, base::Optional<OsrHelper> osr_helper,
                         int start_source_position,
                         JumpOptimizationInfo* jump_opt,
                         std::vector<trap_handler::ProtectedInstructionData>*
                             protected_instructions);

  // Generate native code. After calling AssembleCode, call FinalizeCode to
  // produce the actual code object. If an error occurs during either phase,
  // FinalizeCode returns a null handle.
  void AssembleCode();  // Does not need to run on main thread.
  Handle<Code> FinalizeCode();

  Handle<ByteArray> GetSourcePositionTable();
  MaybeHandle<HandlerTable> GetHandlerTable() const;

  InstructionSequence* code() const { return code_; }
  FrameAccessState* frame_access_state() const { return frame_access_state_; }
  const Frame* frame() const { return frame_access_state_->frame(); }
  Isolate* isolate() const { return isolate_; }
  Linkage* linkage() const { return linkage_; }

  Label* GetLabel(RpoNumber rpo) { return &labels_[rpo.ToSize()]; }

  void AddProtectedInstructionLanding(uint32_t instr_offset,
                                      uint32_t landing_offset);

  SourcePosition start_source_position() const {
    return start_source_position_;
  }

  void AssembleSourcePosition(Instruction* instr);
  void AssembleSourcePosition(SourcePosition source_position);

  // Record a safepoint with the given pointer map.
  void RecordSafepoint(ReferenceMap* references, Safepoint::Kind kind,
                       int arguments, Safepoint::DeoptMode deopt_mode);

  Zone* zone() const { return zone_; }
  TurboAssembler* tasm() { return &tasm_; }
  size_t GetSafepointTableOffset() const { return safepoints_.GetCodeOffset(); }

 private:
  GapResolver* resolver() { return &resolver_; }
  SafepointTableBuilder* safepoints() { return &safepoints_; }
  CompilationInfo* info() const { return info_; }
  OsrHelper* osr_helper() { return &(*osr_helper_); }

  // Create the FrameAccessState object. The Frame is immutable from here on.
  void CreateFrameAccessState(Frame* frame);

  // Architecture - specific frame finalization.
  void FinishFrame(Frame* frame);

  // Checks if {block} will appear directly after {current_block_} when
  // assembling code, in which case, a fall-through can be used.
  bool IsNextInAssemblyOrder(RpoNumber block) const;

  // Check if a heap object can be materialized by loading from a heap root,
  // which is cheaper on some platforms than materializing the actual heap
  // object constant.
  bool IsMaterializableFromRoot(Handle<HeapObject> object,
                                Heap::RootListIndex* index_return);

  enum CodeGenResult { kSuccess, kTooManyDeoptimizationBailouts };

  // Assemble instructions for the specified block.
  CodeGenResult AssembleBlock(const InstructionBlock* block);

  // Assemble code for the specified instruction.
  CodeGenResult AssembleInstruction(Instruction* instr,
                                    const InstructionBlock* block);
  void AssembleGaps(Instruction* instr);

  // Returns true if a instruction is a tail call that needs to adjust the stack
  // pointer before execution. The stack slot index to the empty slot above the
  // adjusted stack pointer is returned in |slot|.
  bool GetSlotAboveSPBeforeTailCall(Instruction* instr, int* slot);

  CodeGenResult AssembleDeoptimizerCall(int deoptimization_id,
                                        SourcePosition pos);

  // ===========================================================================
  // ============= Architecture-specific code generation methods. ==============
  // ===========================================================================

  CodeGenResult AssembleArchInstruction(Instruction* instr);
  void AssembleArchJump(RpoNumber target);
  void AssembleArchBranch(Instruction* instr, BranchInfo* branch);

  // Generates special branch for deoptimization condition.
  void AssembleArchDeoptBranch(Instruction* instr, BranchInfo* branch);

  void AssembleArchBoolean(Instruction* instr, FlagsCondition condition);
  void AssembleArchTrap(Instruction* instr, FlagsCondition condition);
  void AssembleArchLookupSwitch(Instruction* instr);
  void AssembleArchTableSwitch(Instruction* instr);

  // When entering a code that is marked for deoptimization, rather continuing
  // with its execution, we jump to a lazy compiled code. We need to do this
  // because this code has already been deoptimized and needs to be unlinked
  // from the JS functions referring it.
  void BailoutIfDeoptimized();

  // Generates an architecture-specific, descriptor-specific prologue
  // to set up a stack frame.
  void AssembleConstructFrame();

  // Generates an architecture-specific, descriptor-specific return sequence
  // to tear down a stack frame.
  void AssembleReturn(InstructionOperand* pop);

  void AssembleDeconstructFrame();

  // Generates code to manipulate the stack in preparation for a tail call.
  void AssemblePrepareTailCall();

  // Generates code to pop current frame if it is an arguments adaptor frame.
  void AssemblePopArgumentsAdaptorFrame(Register args_reg, Register scratch1,
                                        Register scratch2, Register scratch3);

  enum PushTypeFlag {
    kImmediatePush = 0x1,
    kRegisterPush = 0x2,
    kStackSlotPush = 0x4,
    kScalarPush = kRegisterPush | kStackSlotPush
  };

  typedef base::Flags<PushTypeFlag> PushTypeFlags;

  static bool IsValidPush(InstructionOperand source, PushTypeFlags push_type);

  // Generate a list moves from an instruction that are candidates to be turned
  // into push instructions on platforms that support them. In general, the list
  // of push candidates are moves to a set of contiguous destination
  // InstructionOperand locations on the stack that don't clobber values that
  // are needed for resolve the gap or use values generated by the gap,
  // i.e. moves that can be hoisted together before the actual gap and assembled
  // together.
  static void GetPushCompatibleMoves(Instruction* instr,
                                     PushTypeFlags push_type,
                                     ZoneVector<MoveOperands*>* pushes);

  // Called before a tail call |instr|'s gap moves are assembled and allows
  // gap-specific pre-processing, e.g. adjustment of the sp for tail calls that
  // need it before gap moves or conversion of certain gap moves into pushes.
  void AssembleTailCallBeforeGap(Instruction* instr,
                                 int first_unused_stack_slot);
  // Called after a tail call |instr|'s gap moves are assembled and allows
  // gap-specific post-processing, e.g. adjustment of the sp for tail calls that
  // need it after gap moves.
  void AssembleTailCallAfterGap(Instruction* instr,
                                int first_unused_stack_slot);

  void FinishCode();

  // ===========================================================================
  // ============== Architecture-specific gap resolver methods. ================
  // ===========================================================================

  // Interface used by the gap resolver to emit moves and swaps.
  void AssembleMove(InstructionOperand* source,
                    InstructionOperand* destination) final;
  void AssembleSwap(InstructionOperand* source,
                    InstructionOperand* destination) final;

  // ===========================================================================
  // =================== Jump table construction methods. ======================
  // ===========================================================================

  class JumpTable;
  // Adds a jump table that is emitted after the actual code.  Returns label
  // pointing to the beginning of the table.  {targets} is assumed to be static
  // or zone allocated.
  Label* AddJumpTable(Label** targets, size_t target_count);
  // Emits a jump table.
  void AssembleJumpTable(Label** targets, size_t target_count);

  // ===========================================================================
  // ================== Deoptimization table construction. =====================
  // ===========================================================================

  void RecordCallPosition(Instruction* instr);
  Handle<DeoptimizationData> GenerateDeoptimizationData();
  int DefineDeoptimizationLiteral(DeoptimizationLiteral literal);
  DeoptimizationEntry const& GetDeoptimizationEntry(Instruction* instr,
                                                    size_t frame_state_offset);
  DeoptimizeKind GetDeoptimizationKind(int deoptimization_id) const;
  DeoptimizeReason GetDeoptimizationReason(int deoptimization_id) const;
  int BuildTranslation(Instruction* instr, int pc_offset,
                       size_t frame_state_offset,
                       OutputFrameStateCombine state_combine);
  void BuildTranslationForFrameStateDescriptor(
      FrameStateDescriptor* descriptor, InstructionOperandIterator* iter,
      Translation* translation, OutputFrameStateCombine state_combine);
  void TranslateStateValueDescriptor(StateValueDescriptor* desc,
                                     StateValueList* nested,
                                     Translation* translation,
                                     InstructionOperandIterator* iter);
  void TranslateFrameStateDescriptorOperands(FrameStateDescriptor* desc,
                                             InstructionOperandIterator* iter,
                                             OutputFrameStateCombine combine,
                                             Translation* translation);
  void AddTranslationForOperand(Translation* translation, Instruction* instr,
                                InstructionOperand* op, MachineType type);
  void MarkLazyDeoptSite();

  DeoptimizationExit* AddDeoptimizationExit(Instruction* instr,
                                            size_t frame_state_offset);

  // ===========================================================================

  class DeoptimizationState final : public ZoneObject {
   public:
    DeoptimizationState(BailoutId bailout_id, int translation_id, int pc_offset,
                        DeoptimizeKind kind, DeoptimizeReason reason)
        : bailout_id_(bailout_id),
          translation_id_(translation_id),
          pc_offset_(pc_offset),
          kind_(kind),
          reason_(reason) {}

    BailoutId bailout_id() const { return bailout_id_; }
    int translation_id() const { return translation_id_; }
    int pc_offset() const { return pc_offset_; }
    DeoptimizeKind kind() const { return kind_; }
    DeoptimizeReason reason() const { return reason_; }

   private:
    BailoutId bailout_id_;
    int translation_id_;
    int pc_offset_;
    DeoptimizeKind kind_;
    DeoptimizeReason reason_;
  };

  struct HandlerInfo {
    Label* handler;
    int pc_offset;
  };

  friend class OutOfLineCode;
  friend class CodeGeneratorTester;

  Zone* zone_;
  Isolate* isolate_;
  FrameAccessState* frame_access_state_;
  Linkage* const linkage_;
  InstructionSequence* const code_;
  UnwindingInfoWriter unwinding_info_writer_;
  CompilationInfo* const info_;
  Label* const labels_;
  Label return_label_;
  RpoNumber current_block_;
  SourcePosition start_source_position_;
  SourcePosition current_source_position_;
  TurboAssembler tasm_;
  GapResolver resolver_;
  SafepointTableBuilder safepoints_;
  ZoneVector<HandlerInfo> handlers_;
  ZoneDeque<DeoptimizationExit*> deoptimization_exits_;
  ZoneDeque<DeoptimizationState*> deoptimization_states_;
  ZoneDeque<DeoptimizationLiteral> deoptimization_literals_;
  size_t inlined_function_count_;
  TranslationBuffer translations_;
  int last_lazy_deopt_pc_;

  // kArchCallCFunction could be reached either:
  //   kArchCallCFunction;
  // or:
  //   kArchSaveCallerRegisters;
  //   kArchCallCFunction;
  //   kArchRestoreCallerRegisters;
  // The boolean is used to distinguish the two cases. In the latter case, we
  // also need to decide if FP registers need to be saved, which is controlled
  // by fp_mode_.
  bool caller_registers_saved_;
  SaveFPRegsMode fp_mode_;

  JumpTable* jump_tables_;
  OutOfLineCode* ools_;
  base::Optional<OsrHelper> osr_helper_;
  int osr_pc_offset_;
  int optimized_out_literal_id_;
  SourcePositionTableBuilder source_position_table_builder_;
  std::vector<trap_handler::ProtectedInstructionData>* protected_instructions_;
  CodeGenResult result_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_GENERATOR_H
