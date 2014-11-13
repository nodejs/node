// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_GENERATOR_H_
#define V8_COMPILER_CODE_GENERATOR_H_

#include <deque>

#include "src/compiler/gap-resolver.h"
#include "src/compiler/instruction.h"
#include "src/deoptimizer.h"
#include "src/macro-assembler.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {
namespace compiler {

class Linkage;

// Generates native code for a sequence of instructions.
class CodeGenerator FINAL : public GapResolver::Assembler {
 public:
  explicit CodeGenerator(Frame* frame, Linkage* linkage,
                         InstructionSequence* code, CompilationInfo* info);

  // Generate native code.
  Handle<Code> GenerateCode();

  InstructionSequence* code() const { return code_; }
  Frame* frame() const { return frame_; }
  Isolate* isolate() const { return zone()->isolate(); }
  Linkage* linkage() const { return linkage_; }

  Label* GetLabel(BasicBlock::RpoNumber rpo) { return &labels_[rpo.ToSize()]; }

 private:
  MacroAssembler* masm() { return &masm_; }
  GapResolver* resolver() { return &resolver_; }
  SafepointTableBuilder* safepoints() { return &safepoints_; }
  Zone* zone() const { return code()->zone(); }
  CompilationInfo* info() const { return info_; }

  // Checks if {block} will appear directly after {current_block_} when
  // assembling code, in which case, a fall-through can be used.
  bool IsNextInAssemblyOrder(BasicBlock::RpoNumber block) const;

  // Record a safepoint with the given pointer map.
  void RecordSafepoint(PointerMap* pointers, Safepoint::Kind kind,
                       int arguments, Safepoint::DeoptMode deopt_mode);

  // Assemble code for the specified instruction.
  void AssembleInstruction(Instruction* instr);
  void AssembleSourcePosition(SourcePositionInstruction* instr);
  void AssembleGap(GapInstruction* gap);

  // ===========================================================================
  // ============= Architecture-specific code generation methods. ==============
  // ===========================================================================

  void AssembleArchInstruction(Instruction* instr);
  void AssembleArchBranch(Instruction* instr, FlagsCondition condition);
  void AssembleArchBoolean(Instruction* instr, FlagsCondition condition);

  void AssembleDeoptimizerCall(int deoptimization_id);

  // Generates an architecture-specific, descriptor-specific prologue
  // to set up a stack frame.
  void AssemblePrologue();
  // Generates an architecture-specific, descriptor-specific return sequence
  // to tear down a stack frame.
  void AssembleReturn();

  // ===========================================================================
  // ============== Architecture-specific gap resolver methods. ================
  // ===========================================================================

  // Interface used by the gap resolver to emit moves and swaps.
  virtual void AssembleMove(InstructionOperand* source,
                            InstructionOperand* destination) OVERRIDE;
  virtual void AssembleSwap(InstructionOperand* source,
                            InstructionOperand* destination) OVERRIDE;

  // ===========================================================================
  // Deoptimization table construction
  void AddSafepointAndDeopt(Instruction* instr);
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);
  FrameStateDescriptor* GetFrameStateDescriptor(Instruction* instr,
                                                size_t frame_state_offset);
  int BuildTranslation(Instruction* instr, int pc_offset,
                       size_t frame_state_offset,
                       OutputFrameStateCombine state_combine);
  void BuildTranslationForFrameStateDescriptor(
      FrameStateDescriptor* descriptor, Instruction* instr,
      Translation* translation, size_t frame_state_offset,
      OutputFrameStateCombine state_combine);
  void AddTranslationForOperand(Translation* translation, Instruction* instr,
                                InstructionOperand* op, MachineType type);
  void AddNopForSmiCodeInlining();
  void EnsureSpaceForLazyDeopt();
  void MarkLazyDeoptSite();

  // ===========================================================================
  struct DeoptimizationState : ZoneObject {
   public:
    BailoutId bailout_id() const { return bailout_id_; }
    int translation_id() const { return translation_id_; }
    int pc_offset() const { return pc_offset_; }

    DeoptimizationState(BailoutId bailout_id, int translation_id, int pc_offset)
        : bailout_id_(bailout_id),
          translation_id_(translation_id),
          pc_offset_(pc_offset) {}

   private:
    BailoutId bailout_id_;
    int translation_id_;
    int pc_offset_;
  };

  Frame* const frame_;
  Linkage* const linkage_;
  InstructionSequence* const code_;
  CompilationInfo* const info_;
  Label* const labels_;
  BasicBlock::RpoNumber current_block_;
  SourcePosition current_source_position_;
  MacroAssembler masm_;
  GapResolver resolver_;
  SafepointTableBuilder safepoints_;
  ZoneDeque<DeoptimizationState*> deoptimization_states_;
  ZoneDeque<Handle<Object> > deoptimization_literals_;
  TranslationBuffer translations_;
  int last_lazy_deopt_pc_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_GENERATOR_H
