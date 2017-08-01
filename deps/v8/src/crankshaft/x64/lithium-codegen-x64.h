// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_X64_LITHIUM_CODEGEN_X64_H_
#define V8_CRANKSHAFT_X64_LITHIUM_CODEGEN_X64_H_


#include "src/ast/scopes.h"
#include "src/base/logging.h"
#include "src/crankshaft/lithium-codegen.h"
#include "src/crankshaft/x64/lithium-gap-resolver-x64.h"
#include "src/crankshaft/x64/lithium-x64.h"
#include "src/deoptimizer.h"
#include "src/safepoint-table.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LDeferredCode;
class SafepointGenerator;

class LCodeGen: public LCodeGenBase {
 public:
  LCodeGen(LChunk* chunk, MacroAssembler* assembler, CompilationInfo* info)
      : LCodeGenBase(chunk, assembler, info),
        jump_table_(4, info->zone()),
        scope_(info->scope()),
        deferred_(8, info->zone()),
        frame_is_built_(false),
        safepoints_(info->zone()),
        resolver_(this),
        expected_safepoint_kind_(Safepoint::kSimple) {
    PopulateDeoptimizationLiteralsWithInlinedFunctions();
  }

  int LookupDestination(int block_id) const {
    return chunk()->LookupDestination(block_id);
  }

  bool IsNextEmittedBlock(int block_id) const {
    return LookupDestination(block_id) == GetNextEmittedBlock();
  }

  bool NeedsEagerFrame() const {
    return HasAllocatedStackSlots() || info()->is_non_deferred_calling() ||
           !info()->IsStub() || info()->requires_frame();
  }
  bool NeedsDeferredFrame() const {
    return !NeedsEagerFrame() && info()->is_deferred_calling();
  }

  // Support for converting LOperands to assembler types.
  Register ToRegister(LOperand* op) const;
  XMMRegister ToDoubleRegister(LOperand* op) const;
  bool IsInteger32Constant(LConstantOperand* op) const;
  bool IsExternalConstant(LConstantOperand* op) const;
  bool IsDehoistedKeyConstant(LConstantOperand* op) const;
  bool IsSmiConstant(LConstantOperand* op) const;
  int32_t ToRepresentation(LConstantOperand* op, const Representation& r) const;
  int32_t ToInteger32(LConstantOperand* op) const;
  Smi* ToSmi(LConstantOperand* op) const;
  double ToDouble(LConstantOperand* op) const;
  ExternalReference ToExternalReference(LConstantOperand* op) const;
  Handle<Object> ToHandle(LConstantOperand* op) const;
  Operand ToOperand(LOperand* op) const;

  // Try to generate code for the entire chunk, but it may fail if the
  // chunk contains constructs we cannot handle. Returns true if the
  // code generation attempt succeeded.
  bool GenerateCode();

  // Finish the code by setting stack height, safepoint, and bailout
  // information on it.
  void FinishCode(Handle<Code> code);

  // Deferred code support.
  void DoDeferredNumberTagD(LNumberTagD* instr);

  enum IntegerSignedness { SIGNED_INT32, UNSIGNED_INT32 };
  void DoDeferredNumberTagIU(LInstruction* instr,
                             LOperand* value,
                             LOperand* temp1,
                             LOperand* temp2,
                             IntegerSignedness signedness);

  void DoDeferredTaggedToI(LTaggedToI* instr, Label* done);
  void DoDeferredMathAbsTaggedHeapNumber(LMathAbs* instr);
  void DoDeferredStackCheck(LStackCheck* instr);
  void DoDeferredMaybeGrowElements(LMaybeGrowElements* instr);
  void DoDeferredStringCharCodeAt(LStringCharCodeAt* instr);
  void DoDeferredStringCharFromCode(LStringCharFromCode* instr);
  void DoDeferredAllocate(LAllocate* instr);
  void DoDeferredInstanceMigration(LCheckMaps* instr, Register object);
  void DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr,
                                   Register object,
                                   Register index);

// Parallel move support.
  void DoParallelMove(LParallelMove* move);
  void DoGap(LGap* instr);

  // Emit frame translation commands for an environment.
  void WriteTranslation(LEnvironment* environment, Translation* translation);

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) void Do##type(L##type* node);
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  LPlatformChunk* chunk() const { return chunk_; }
  Scope* scope() const { return scope_; }
  HGraph* graph() const { return chunk()->graph(); }

  XMMRegister double_scratch0() const { return kScratchDoubleReg; }

  void EmitClassOfTest(Label* if_true, Label* if_false,
                       Handle<String> class_name, Register input,
                       Register temporary, Register scratch);

  bool HasAllocatedStackSlots() const {
    return chunk()->HasAllocatedStackSlots();
  }
  int GetStackSlotCount() const { return chunk()->GetSpillSlotCount(); }
  int GetTotalFrameSlotCount() const {
    return chunk()->GetTotalFrameSlotCount();
  }

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }


  void SaveCallerDoubles();
  void RestoreCallerDoubles();

  // Code generation passes.  Returns true if code generation should
  // continue.
  void GenerateBodyInstructionPre(LInstruction* instr) override;
  void GenerateBodyInstructionPost(LInstruction* instr) override;
  bool GeneratePrologue();
  bool GenerateDeferredCode();
  bool GenerateJumpTable();
  bool GenerateSafepointTable();

  // Generates the custom OSR entrypoint and sets the osr_pc_offset.
  void GenerateOsrPrologue();

  enum SafepointMode {
    RECORD_SIMPLE_SAFEPOINT,
    RECORD_SAFEPOINT_WITH_REGISTERS
  };

  void CallCodeGeneric(Handle<Code> code,
                       RelocInfo::Mode mode,
                       LInstruction* instr,
                       SafepointMode safepoint_mode,
                       int argc);


  void CallCode(Handle<Code> code,
                RelocInfo::Mode mode,
                LInstruction* instr);

  void CallRuntime(const Runtime::Function* function,
                   int num_arguments,
                   LInstruction* instr,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);

  void CallRuntime(Runtime::FunctionId id,
                   int num_arguments,
                   LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, num_arguments, instr);
  }

  void CallRuntime(Runtime::FunctionId id, LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, function->nargs, instr);
  }

  void CallRuntimeFromDeferred(Runtime::FunctionId id,
                               int argc,
                               LInstruction* instr,
                               LOperand* context);

  void LoadContextFromDeferred(LOperand* context);

  void PrepareForTailCall(const ParameterCount& actual, Register scratch1,
                          Register scratch2, Register scratch3);

  // Generate a direct call to a known function.  Expects the function
  // to be in rdi.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count, int arity,
                         bool is_tail_call, LInstruction* instr);

  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode,
                                    int argc);
  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);
  void DeoptimizeIf(Condition cc, LInstruction* instr,
                    DeoptimizeReason deopt_reason,
                    Deoptimizer::BailoutType bailout_type);
  void DeoptimizeIf(Condition cc, LInstruction* instr,
                    DeoptimizeReason deopt_reason);

  bool DeoptEveryNTimes() {
    return FLAG_deopt_every_n_times != 0 && !info()->IsStub();
  }

  void AddToTranslation(LEnvironment* environment,
                        Translation* translation,
                        LOperand* op,
                        bool is_tagged,
                        bool is_uint32,
                        int* object_index_pointer,
                        int* dematerialized_index_pointer);

  Register ToRegister(int index) const;
  XMMRegister ToDoubleRegister(int index) const;
  Operand BuildFastArrayOperand(
      LOperand* elements_pointer,
      LOperand* key,
      Representation key_representation,
      ElementsKind elements_kind,
      uint32_t base_offset);

  Operand BuildSeqStringOperand(Register string,
                                LOperand* index,
                                String::Encoding encoding);

  void EmitIntegerMathAbs(LMathAbs* instr);
  void EmitSmiMathAbs(LMathAbs* instr);

  // Support for recording safepoint information.
  void RecordSafepoint(LPointerMap* pointers,
                       Safepoint::Kind kind,
                       int arguments,
                       Safepoint::DeoptMode mode);
  void RecordSafepoint(LPointerMap* pointers, Safepoint::DeoptMode mode);
  void RecordSafepoint(Safepoint::DeoptMode mode);
  void RecordSafepointWithRegisters(LPointerMap* pointers,
                                    int arguments,
                                    Safepoint::DeoptMode mode);

  static Condition TokenToCondition(Token::Value op, bool is_unsigned);
  void EmitGoto(int block);

  // EmitBranch expects to be the last instruction of a block.
  template<class InstrType>
  void EmitBranch(InstrType instr, Condition cc);
  template <class InstrType>
  void EmitTrueBranch(InstrType instr, Condition cc);
  template <class InstrType>
  void EmitFalseBranch(InstrType instr, Condition cc);
  void EmitNumberUntagD(LNumberUntagD* instr, Register input,
                        XMMRegister result, NumberUntagDMode mode);

  // Emits optimized code for typeof x == "y".  Modifies input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitTypeofIs(LTypeofIsAndBranch* instr, Register input);

  // Emits optimized code for %_IsString(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsString(Register input,
                         Register temp1,
                         Label* is_not_string,
                         SmiCheck check_needed);

  // Emits code for pushing either a tagged constant, a (non-double)
  // register, or a stack slot operand.
  void EmitPushTaggedOperand(LOperand* operand);

  // Emits optimized code to deep-copy the contents of statically known
  // object graphs (e.g. object literal boilerplate).
  void EmitDeepCopy(Handle<JSObject> object,
                    Register result,
                    Register source,
                    int* offset,
                    AllocationSiteMode mode);

  void EnsureSpaceForLazyDeopt(int space_needed) override;
  void DoLoadKeyedExternalArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedArray(LLoadKeyed* instr);
  void DoStoreKeyedExternalArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedArray(LStoreKeyed* instr);

  template <class T>
  void EmitVectorLoadICRegisters(T* instr);

#ifdef _MSC_VER
  // On windows, you may not access the stack more than one page below
  // the most recently mapped page. To make the allocated area randomly
  // accessible, we write an arbitrary value to each page in range
  // rsp + offset - page_size .. rsp in turn.
  void MakeSureStackPagesMapped(int offset);
#endif

  ZoneList<Deoptimizer::JumpTableEntry> jump_table_;
  Scope* const scope_;
  ZoneList<LDeferredCode*> deferred_;
  bool frame_is_built_;

  // Builder that keeps track of safepoints in the code. The table
  // itself is emitted at the end of the generated code.
  SafepointTableBuilder safepoints_;

  // Compiler from a set of parallel moves to a sequential list of moves.
  LGapResolver resolver_;

  Safepoint::Kind expected_safepoint_kind_;

  class PushSafepointRegistersScope final BASE_EMBEDDED {
   public:
    explicit PushSafepointRegistersScope(LCodeGen* codegen)
        : codegen_(codegen) {
      DCHECK(codegen_->info()->is_calling());
      DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->masm_->PushSafepointRegisters();
      codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;
    }

    ~PushSafepointRegistersScope() {
      DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kWithRegisters);
      codegen_->masm_->PopSafepointRegisters();
      codegen_->expected_safepoint_kind_ = Safepoint::kSimple;
    }

   private:
    LCodeGen* codegen_;
  };

  friend class LDeferredCode;
  friend class LEnvironment;
  friend class SafepointGenerator;
  DISALLOW_COPY_AND_ASSIGN(LCodeGen);
};


class LDeferredCode: public ZoneObject {
 public:
  explicit LDeferredCode(LCodeGen* codegen)
      : codegen_(codegen),
        external_exit_(NULL),
        instruction_index_(codegen->current_instruction_) {
    codegen->AddDeferredCode(this);
  }

  virtual ~LDeferredCode() {}
  virtual void Generate() = 0;
  virtual LInstruction* instr() = 0;

  void SetExit(Label* exit) { external_exit_ = exit; }
  Label* entry() { return &entry_; }
  Label* exit() { return external_exit_ != NULL ? external_exit_ : &exit_; }
  Label* done() { return codegen_->NeedsDeferredFrame() ? &done_ : exit(); }
  int instruction_index() const { return instruction_index_; }

 protected:
  LCodeGen* codegen() const { return codegen_; }
  MacroAssembler* masm() const { return codegen_->masm(); }

 private:
  LCodeGen* codegen_;
  Label entry_;
  Label exit_;
  Label done_;
  Label* external_exit_;
  int instruction_index_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_X64_LITHIUM_CODEGEN_X64_H_
