// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_GENERATOR_H_
#define V8_COMPILER_CODE_GENERATOR_H_

#include "src/compiler/gap-resolver.h"
#include "src/compiler/instruction.h"
#include "src/compiler/unwinding-info-writer.h"
#include "src/deoptimizer.h"
#include "src/macro-assembler.h"
#include "src/safepoint-table.h"
#include "src/source-position-table.h"

namespace v8 {
namespace internal {

class CompilationInfo;

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


// Generates native code for a sequence of instructions.
class CodeGenerator final : public GapResolver::Assembler {
 public:
  explicit CodeGenerator(Frame* frame, Linkage* linkage,
                         InstructionSequence* code, CompilationInfo* info);

  // Generate native code.
  Handle<Code> GenerateCode();

  InstructionSequence* code() const { return code_; }
  FrameAccessState* frame_access_state() const { return frame_access_state_; }
  const Frame* frame() const { return frame_access_state_->frame(); }
  Isolate* isolate() const;
  Linkage* linkage() const { return linkage_; }

  Label* GetLabel(RpoNumber rpo) { return &labels_[rpo.ToSize()]; }

 private:
  MacroAssembler* masm() { return &masm_; }
  GapResolver* resolver() { return &resolver_; }
  SafepointTableBuilder* safepoints() { return &safepoints_; }
  Zone* zone() const { return code()->zone(); }
  CompilationInfo* info() const { return info_; }

  // Create the FrameAccessState object. The Frame is immutable from here on.
  void CreateFrameAccessState(Frame* frame);

  // Architecture - specific frame finalization.
  void FinishFrame(Frame* frame);

  // Checks if {block} will appear directly after {current_block_} when
  // assembling code, in which case, a fall-through can be used.
  bool IsNextInAssemblyOrder(RpoNumber block) const;

  // Record a safepoint with the given pointer map.
  void RecordSafepoint(ReferenceMap* references, Safepoint::Kind kind,
                       int arguments, Safepoint::DeoptMode deopt_mode);

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
  void AssembleSourcePosition(Instruction* instr);
  void AssembleGaps(Instruction* instr);

  // Returns true if a instruction is a tail call that needs to adjust the stack
  // pointer before execution. The stack slot index to the empty slot above the
  // adjusted stack pointer is returned in |slot|.
  bool GetSlotAboveSPBeforeTailCall(Instruction* instr, int* slot);

  // ===========================================================================
  // ============= Architecture-specific code generation methods. ==============
  // ===========================================================================

  CodeGenResult AssembleArchInstruction(Instruction* instr);
  void AssembleArchJump(RpoNumber target);
  void AssembleArchBranch(Instruction* instr, BranchInfo* branch);
  void AssembleArchBoolean(Instruction* instr, FlagsCondition condition);
  void AssembleArchLookupSwitch(Instruction* instr);
  void AssembleArchTableSwitch(Instruction* instr);

  CodeGenResult AssembleDeoptimizerCall(int deoptimization_id,
                                        Deoptimizer::BailoutType bailout_type,
                                        SourcePosition pos);

  // Generates an architecture-specific, descriptor-specific prologue
  // to set up a stack frame.
  void AssembleConstructFrame();

  // Generates an architecture-specific, descriptor-specific return sequence
  // to tear down a stack frame.
  void AssembleReturn();

  void AssembleDeconstructFrame();

  // Generates code to manipulate the stack in preparation for a tail call.
  void AssemblePrepareTailCall();

  // Generates code to pop current frame if it is an arguments adaptor frame.
  void AssemblePopArgumentsAdaptorFrame(Register args_reg, Register scratch1,
                                        Register scratch2, Register scratch3);

  enum PushTypeFlag {
    kImmediatePush = 0x1,
    kScalarPush = 0x2,
    kFloat32Push = 0x4,
    kFloat64Push = 0x8,
    kFloatPush = kFloat32Push | kFloat64Push
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
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);
  DeoptimizationEntry const& GetDeoptimizationEntry(Instruction* instr,
                                                    size_t frame_state_offset);
  DeoptimizeReason GetDeoptimizationReason(int deoptimization_id) const;
  int BuildTranslation(Instruction* instr, int pc_offset,
                       size_t frame_state_offset,
                       OutputFrameStateCombine state_combine);
  void BuildTranslationForFrameStateDescriptor(
      FrameStateDescriptor* descriptor, InstructionOperandIterator* iter,
      Translation* translation, OutputFrameStateCombine state_combine);
  void TranslateStateValueDescriptor(StateValueDescriptor* desc,
                                     Translation* translation,
                                     InstructionOperandIterator* iter);
  void TranslateFrameStateDescriptorOperands(FrameStateDescriptor* desc,
                                             InstructionOperandIterator* iter,
                                             OutputFrameStateCombine combine,
                                             Translation* translation);
  void AddTranslationForOperand(Translation* translation, Instruction* instr,
                                InstructionOperand* op, MachineType type);
  void EnsureSpaceForLazyDeopt();
  void MarkLazyDeoptSite();

  DeoptimizationExit* AddDeoptimizationExit(Instruction* instr,
                                            size_t frame_state_offset);

  // ===========================================================================

  class DeoptimizationState final : public ZoneObject {
   public:
    DeoptimizationState(BailoutId bailout_id, int translation_id, int pc_offset,
                        DeoptimizeReason reason)
        : bailout_id_(bailout_id),
          translation_id_(translation_id),
          pc_offset_(pc_offset),
          reason_(reason) {}

    BailoutId bailout_id() const { return bailout_id_; }
    int translation_id() const { return translation_id_; }
    int pc_offset() const { return pc_offset_; }
    DeoptimizeReason reason() const { return reason_; }

   private:
    BailoutId bailout_id_;
    int translation_id_;
    int pc_offset_;
    DeoptimizeReason reason_;
  };

  struct HandlerInfo {
    Label* handler;
    int pc_offset;
  };

  friend class OutOfLineCode;

  FrameAccessState* frame_access_state_;
  Linkage* const linkage_;
  InstructionSequence* const code_;
  UnwindingInfoWriter unwinding_info_writer_;
  CompilationInfo* const info_;
  Label* const labels_;
  Label return_label_;
  RpoNumber current_block_;
  SourcePosition current_source_position_;
  MacroAssembler masm_;
  GapResolver resolver_;
  SafepointTableBuilder safepoints_;
  ZoneVector<HandlerInfo> handlers_;
  ZoneDeque<DeoptimizationExit*> deoptimization_exits_;
  ZoneDeque<DeoptimizationState*> deoptimization_states_;
  ZoneDeque<Handle<Object>> deoptimization_literals_;
  size_t inlined_function_count_;
  TranslationBuffer translations_;
  int last_lazy_deopt_pc_;
  JumpTable* jump_tables_;
  OutOfLineCode* ools_;
  int osr_pc_offset_;
  SourcePositionTableBuilder source_position_table_builder_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_GENERATOR_H
