// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_CODE_GENERATOR_H_
#define V8_COMPILER_BACKEND_CODE_GENERATOR_H_

#include <memory>

#include "src/base/optional.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/safepoint-table.h"
#include "src/codegen/source-position-table.h"
#include "src/compiler/backend/gap-resolver.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/backend/unwinding-info-writer.h"
#include "src/compiler/osr.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {

class OptimizedCompilationInfo;

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

enum class DeoptimizationLiteralKind { kObject, kNumber, kString };

// Either a non-null Handle<Object>, a double or a StringConstantBase.
class DeoptimizationLiteral {
 public:
  DeoptimizationLiteral() : object_(), number_(0), string_(nullptr) {}
  explicit DeoptimizationLiteral(Handle<Object> object)
      : kind_(DeoptimizationLiteralKind::kObject), object_(object) {
    CHECK(!object_.is_null());
  }
  explicit DeoptimizationLiteral(double number)
      : kind_(DeoptimizationLiteralKind::kNumber), number_(number) {}
  explicit DeoptimizationLiteral(const StringConstantBase* string)
      : kind_(DeoptimizationLiteralKind::kString), string_(string) {}

  Handle<Object> object() const { return object_; }
  const StringConstantBase* string() const { return string_; }

  bool operator==(const DeoptimizationLiteral& other) const {
    return kind_ == other.kind_ && object_.equals(other.object_) &&
           bit_cast<uint64_t>(number_) == bit_cast<uint64_t>(other.number_) &&
           bit_cast<intptr_t>(string_) == bit_cast<intptr_t>(other.string_);
  }

  Handle<Object> Reify(Isolate* isolate) const;

  DeoptimizationLiteralKind kind() const { return kind_; }

 private:
  DeoptimizationLiteralKind kind_;

  Handle<Object> object_;
  double number_ = 0;
  const StringConstantBase* string_ = nullptr;
};

// These structs hold pc offsets for generated instructions and is only used
// when tracing for turbolizer is enabled.
struct TurbolizerCodeOffsetsInfo {
  int code_start_register_check = -1;
  int deopt_check = -1;
  int init_poison = -1;
  int blocks_start = -1;
  int out_of_line_code = -1;
  int deoptimization_exits = -1;
  int pools = -1;
  int jump_tables = -1;
};

struct TurbolizerInstructionStartInfo {
  int gap_pc_offset = -1;
  int arch_instr_pc_offset = -1;
  int condition_pc_offset = -1;
};

// Generates native code for a sequence of instructions.
class V8_EXPORT_PRIVATE CodeGenerator final : public GapResolver::Assembler {
 public:
  explicit CodeGenerator(
      Zone* codegen_zone, Frame* frame, Linkage* linkage,
      InstructionSequence* instructions, OptimizedCompilationInfo* info,
      Isolate* isolate, base::Optional<OsrHelper> osr_helper,
      int start_source_position, JumpOptimizationInfo* jump_opt,
      PoisoningMitigationLevel poisoning_level, const AssemblerOptions& options,
      int32_t builtin_index, size_t max_unoptimized_frame_height,
      size_t max_pushed_argument_count, std::unique_ptr<AssemblerBuffer> = {});

  // Generate native code. After calling AssembleCode, call FinalizeCode to
  // produce the actual code object. If an error occurs during either phase,
  // FinalizeCode returns an empty MaybeHandle.
  void AssembleCode();  // Does not need to run on main thread.
  MaybeHandle<Code> FinalizeCode();

  OwnedVector<byte> GetSourcePositionTable();
  OwnedVector<byte> GetProtectedInstructionsData();

  InstructionSequence* instructions() const { return instructions_; }
  FrameAccessState* frame_access_state() const { return frame_access_state_; }
  const Frame* frame() const { return frame_access_state_->frame(); }
  Isolate* isolate() const { return isolate_; }
  Linkage* linkage() const { return linkage_; }

  Label* GetLabel(RpoNumber rpo) { return &labels_[rpo.ToSize()]; }

  void AddProtectedInstructionLanding(uint32_t instr_offset,
                                      uint32_t landing_offset);

  bool wasm_runtime_exception_support() const;

  SourcePosition start_source_position() const {
    return start_source_position_;
  }

  void AssembleSourcePosition(Instruction* instr);
  void AssembleSourcePosition(SourcePosition source_position);

  // Record a safepoint with the given pointer map.
  void RecordSafepoint(ReferenceMap* references,
                       Safepoint::DeoptMode deopt_mode);

  Zone* zone() const { return zone_; }
  TurboAssembler* tasm() { return &tasm_; }
  SafepointTableBuilder* safepoint_table_builder() { return &safepoints_; }
  size_t GetSafepointTableOffset() const { return safepoints_.GetCodeOffset(); }
  size_t GetHandlerTableOffset() const { return handler_table_offset_; }

  const ZoneVector<int>& block_starts() const { return block_starts_; }
  const ZoneVector<TurbolizerInstructionStartInfo>& instr_starts() const {
    return instr_starts_;
  }

  const TurbolizerCodeOffsetsInfo& offsets_info() const {
    return offsets_info_;
  }

  static constexpr int kBinarySearchSwitchMinimalCases = 4;

  // Returns true if an offset should be applied to the given stack check. There
  // are two reasons that this could happen:
  // 1. The optimized frame is smaller than the corresponding deoptimized frames
  //    and an offset must be applied in order to be able to deopt safely.
  // 2. The current function pushes a large number of arguments to the stack.
  //    These are not accounted for by the initial frame setup.
  bool ShouldApplyOffsetToStackCheck(Instruction* instr, uint32_t* offset);
  uint32_t GetStackCheckOffset();

 private:
  GapResolver* resolver() { return &resolver_; }
  SafepointTableBuilder* safepoints() { return &safepoints_; }
  OptimizedCompilationInfo* info() const { return info_; }
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
                                RootIndex* index_return);

  enum CodeGenResult { kSuccess, kTooManyDeoptimizationBailouts };

  // Assemble instructions for the specified block.
  CodeGenResult AssembleBlock(const InstructionBlock* block);

  // Inserts mask update at the beginning of an instruction block if the
  // predecessor blocks ends with a masking branch.
  void TryInsertBranchPoisoning(const InstructionBlock* block);

  // Initializes the masking register in the prologue of a function.
  void InitializeSpeculationPoison();
  // Reset the masking register during execution of a function.
  void ResetSpeculationPoison();
  // Generates a mask from the pc passed in {kJavaScriptCallCodeStartRegister}.
  void GenerateSpeculationPoisonFromCodeStartRegister();

  // Assemble code for the specified instruction.
  CodeGenResult AssembleInstruction(int instruction_index,
                                    const InstructionBlock* block);
  void AssembleGaps(Instruction* instr);

  // Compute branch info from given instruction. Returns a valid rpo number
  // if the branch is redundant, the returned rpo number point to the target
  // basic block.
  RpoNumber ComputeBranchInfo(BranchInfo* branch, Instruction* instr);

  // Returns true if a instruction is a tail call that needs to adjust the stack
  // pointer before execution. The stack slot index to the empty slot above the
  // adjusted stack pointer is returned in |slot|.
  bool GetSlotAboveSPBeforeTailCall(Instruction* instr, int* slot);

  // Determines how to call helper stubs depending on the code kind.
  StubCallMode DetermineStubCallMode() const;

  CodeGenResult AssembleDeoptimizerCall(DeoptimizationExit* exit);

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
  void AssembleArchBinarySearchSwitchRange(Register input, RpoNumber def_block,
                                           std::pair<int32_t, Label*>* begin,
                                           std::pair<int32_t, Label*>* end);
  void AssembleArchBinarySearchSwitch(Instruction* instr);
  void AssembleArchTableSwitch(Instruction* instr);

  // Generates code that checks whether the {kJavaScriptCallCodeStartRegister}
  // contains the expected pointer to the start of the instruction stream.
  void AssembleCodeStartRegisterCheck();

  void AssembleBranchPoisoning(FlagsCondition condition, Instruction* instr);

  // When entering a code that is marked for deoptimization, rather continuing
  // with its execution, we jump to a lazy compiled code. We need to do this
  // because this code has already been deoptimized and needs to be unlinked
  // from the JS functions referring it.
  void BailoutIfDeoptimized();

  // Generates code to poison the stack pointer and implicit register arguments
  // like the context register and the function register.
  void AssembleRegisterArgumentPoisoning();

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

  using PushTypeFlags = base::Flags<PushTypeFlag>;

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

  class MoveType {
   public:
    enum Type {
      kRegisterToRegister,
      kRegisterToStack,
      kStackToRegister,
      kStackToStack,
      kConstantToRegister,
      kConstantToStack
    };

    // Detect what type of move or swap needs to be performed. Note that these
    // functions do not take into account the representation (Tagged, FP,
    // ...etc).

    static Type InferMove(InstructionOperand* source,
                          InstructionOperand* destination);
    static Type InferSwap(InstructionOperand* source,
                          InstructionOperand* destination);
  };
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
  void MaybeEmitOutOfLineConstantPool();

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
  DeoptimizationExit* BuildTranslation(Instruction* instr, int pc_offset,
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
                                             Translation* translation);
  void AddTranslationForOperand(Translation* translation, Instruction* instr,
                                InstructionOperand* op, MachineType type);
  void MarkLazyDeoptSite();

  void PrepareForDeoptimizationExits(int deopt_count);
  DeoptimizationExit* AddDeoptimizationExit(Instruction* instr,
                                            size_t frame_state_offset);

  // ===========================================================================

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
  InstructionSequence* const instructions_;
  UnwindingInfoWriter unwinding_info_writer_;
  OptimizedCompilationInfo* const info_;
  Label* const labels_;
  Label return_label_;
  RpoNumber current_block_;
  SourcePosition start_source_position_;
  SourcePosition current_source_position_;
  TurboAssembler tasm_;
  GapResolver resolver_;
  SafepointTableBuilder safepoints_;
  ZoneVector<HandlerInfo> handlers_;
  int next_deoptimization_id_ = 0;
  int deopt_exit_start_offset_ = 0;
  int non_lazy_deopt_count_ = 0;
  ZoneDeque<DeoptimizationExit*> deoptimization_exits_;
  ZoneDeque<DeoptimizationLiteral> deoptimization_literals_;
  size_t inlined_function_count_ = 0;
  TranslationBuffer translations_;
  int handler_table_offset_ = 0;
  int last_lazy_deopt_pc_ = 0;

  // The maximal combined height of all frames produced upon deoptimization, and
  // the maximal number of pushed arguments for function calls. Applied as an
  // offset to the first stack check of an optimized function.
  const size_t max_unoptimized_frame_height_;
  const size_t max_pushed_argument_count_;

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
  ZoneVector<trap_handler::ProtectedInstructionData> protected_instructions_;
  CodeGenResult result_;
  PoisoningMitigationLevel poisoning_level_;
  ZoneVector<int> block_starts_;
  TurbolizerCodeOffsetsInfo offsets_info_;
  ZoneVector<TurbolizerInstructionStartInfo> instr_starts_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_CODE_GENERATOR_H_
