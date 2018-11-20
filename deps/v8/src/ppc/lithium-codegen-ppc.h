// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PPC_LITHIUM_CODEGEN_PPC_H_
#define V8_PPC_LITHIUM_CODEGEN_PPC_H_

#include "src/ppc/lithium-ppc.h"

#include "src/ppc/lithium-gap-resolver-ppc.h"
#include "src/deoptimizer.h"
#include "src/lithium-codegen.h"
#include "src/safepoint-table.h"
#include "src/scopes.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LDeferredCode;
class SafepointGenerator;

class LCodeGen : public LCodeGenBase {
 public:
  LCodeGen(LChunk* chunk, MacroAssembler* assembler, CompilationInfo* info)
      : LCodeGenBase(chunk, assembler, info),
        deoptimizations_(4, info->zone()),
        jump_table_(4, info->zone()),
        deoptimization_literals_(8, info->zone()),
        inlined_function_count_(0),
        scope_(info->scope()),
        translations_(info->zone()),
        deferred_(8, info->zone()),
        osr_pc_offset_(-1),
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
    return GetStackSlotCount() > 0 || info()->is_non_deferred_calling() ||
           !info()->IsStub() || info()->requires_frame();
  }
  bool NeedsDeferredFrame() const {
    return !NeedsEagerFrame() && info()->is_deferred_calling();
  }

  LinkRegisterStatus GetLinkRegisterState() const {
    return frame_is_built_ ? kLRHasBeenSaved : kLRHasNotBeenSaved;
  }

  // Support for converting LOperands to assembler types.
  // LOperand must be a register.
  Register ToRegister(LOperand* op) const;

  // LOperand is loaded into scratch, unless already a register.
  Register EmitLoadRegister(LOperand* op, Register scratch);

  // LConstantOperand must be an Integer32 or Smi
  void EmitLoadIntegerConstant(LConstantOperand* const_op, Register dst);

  // LOperand must be a double register.
  DoubleRegister ToDoubleRegister(LOperand* op) const;

  intptr_t ToRepresentation(LConstantOperand* op,
                            const Representation& r) const;
  int32_t ToInteger32(LConstantOperand* op) const;
  Smi* ToSmi(LConstantOperand* op) const;
  double ToDouble(LConstantOperand* op) const;
  Operand ToOperand(LOperand* op);
  MemOperand ToMemOperand(LOperand* op) const;
  // Returns a MemOperand pointing to the high word of a DoubleStackSlot.
  MemOperand ToHighMemOperand(LOperand* op) const;

  bool IsInteger32(LConstantOperand* op) const;
  bool IsSmi(LConstantOperand* op) const;
  Handle<Object> ToHandle(LConstantOperand* op) const;

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
  void DoDeferredNumberTagIU(LInstruction* instr, LOperand* value,
                             LOperand* temp1, LOperand* temp2,
                             IntegerSignedness signedness);

  void DoDeferredTaggedToI(LTaggedToI* instr);
  void DoDeferredMathAbsTaggedHeapNumber(LMathAbs* instr);
  void DoDeferredStackCheck(LStackCheck* instr);
  void DoDeferredStringCharCodeAt(LStringCharCodeAt* instr);
  void DoDeferredStringCharFromCode(LStringCharFromCode* instr);
  void DoDeferredAllocate(LAllocate* instr);
  void DoDeferredInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr,
                                       Label* map_check);
  void DoDeferredInstanceMigration(LCheckMaps* instr, Register object);
  void DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr, Register result,
                                   Register object, Register index);

  // Parallel move support.
  void DoParallelMove(LParallelMove* move);
  void DoGap(LGap* instr);

  MemOperand PrepareKeyedOperand(Register key, Register base,
                                 bool key_is_constant, bool key_is_tagged,
                                 int constant_key, int element_size_shift,
                                 int base_offset);

  // Emit frame translation commands for an environment.
  void WriteTranslation(LEnvironment* environment, Translation* translation);

// Declare methods that deal with the individual node types.
#define DECLARE_DO(type) void Do##type(L##type* node);
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  LanguageMode language_mode() const { return info()->language_mode(); }

  Scope* scope() const { return scope_; }

  Register scratch0() { return r11; }
  DoubleRegister double_scratch0() { return kScratchDoubleReg; }

  LInstruction* GetNextInstruction();

  void EmitClassOfTest(Label* if_true, Label* if_false,
                       Handle<String> class_name, Register input,
                       Register temporary, Register temporary2);

  int GetStackSlotCount() const { return chunk()->spill_slot_count(); }

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }

  void SaveCallerDoubles();
  void RestoreCallerDoubles();

  // Code generation passes.  Returns true if code generation should
  // continue.
  void GenerateBodyInstructionPre(LInstruction* instr) OVERRIDE;
  bool GeneratePrologue();
  bool GenerateDeferredCode();
  bool GenerateJumpTable();
  bool GenerateSafepointTable();

  // Generates the custom OSR entrypoint and sets the osr_pc_offset.
  void GenerateOsrPrologue();

  enum SafepointMode {
    RECORD_SIMPLE_SAFEPOINT,
    RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS
  };

  void CallCode(Handle<Code> code, RelocInfo::Mode mode, LInstruction* instr);

  void CallCodeGeneric(Handle<Code> code, RelocInfo::Mode mode,
                       LInstruction* instr, SafepointMode safepoint_mode);

  void CallRuntime(const Runtime::Function* function, int num_arguments,
                   LInstruction* instr,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);

  void CallRuntime(Runtime::FunctionId id, int num_arguments,
                   LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, num_arguments, instr);
  }

  void LoadContextFromDeferred(LOperand* context);
  void CallRuntimeFromDeferred(Runtime::FunctionId id, int argc,
                               LInstruction* instr, LOperand* context);

  // Generate a direct call to a known function.  Expects the function
  // to be in r4.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count, int arity,
                         LInstruction* instr);

  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);
  void DeoptimizeIf(Condition condition, LInstruction* instr,
                    Deoptimizer::DeoptReason deopt_reason,
                    Deoptimizer::BailoutType bailout_type, CRegister cr = cr7);
  void DeoptimizeIf(Condition condition, LInstruction* instr,
                    Deoptimizer::DeoptReason deopt_reason, CRegister cr = cr7);

  void AddToTranslation(LEnvironment* environment, Translation* translation,
                        LOperand* op, bool is_tagged, bool is_uint32,
                        int* object_index_pointer,
                        int* dematerialized_index_pointer);
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);

  void PopulateDeoptimizationLiteralsWithInlinedFunctions();

  Register ToRegister(int index) const;
  DoubleRegister ToDoubleRegister(int index) const;

  MemOperand BuildSeqStringOperand(Register string, LOperand* index,
                                   String::Encoding encoding);

  void EmitMathAbs(LMathAbs* instr);
#if V8_TARGET_ARCH_PPC64
  void EmitInteger32MathAbs(LMathAbs* instr);
#endif

  // Support for recording safepoint and position information.
  void RecordSafepoint(LPointerMap* pointers, Safepoint::Kind kind,
                       int arguments, Safepoint::DeoptMode mode);
  void RecordSafepoint(LPointerMap* pointers, Safepoint::DeoptMode mode);
  void RecordSafepoint(Safepoint::DeoptMode mode);
  void RecordSafepointWithRegisters(LPointerMap* pointers, int arguments,
                                    Safepoint::DeoptMode mode);

  void RecordAndWritePosition(int position) OVERRIDE;

  static Condition TokenToCondition(Token::Value op);
  void EmitGoto(int block);

  // EmitBranch expects to be the last instruction of a block.
  template <class InstrType>
  void EmitBranch(InstrType instr, Condition condition, CRegister cr = cr7);
  template <class InstrType>
  void EmitFalseBranch(InstrType instr, Condition condition,
                       CRegister cr = cr7);
  void EmitNumberUntagD(LNumberUntagD* instr, Register input,
                        DoubleRegister result, NumberUntagDMode mode);

  // Emits optimized code for typeof x == "y".  Modifies input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitTypeofIs(Label* true_label, Label* false_label, Register input,
                         Handle<String> type_name);

  // Emits optimized code for %_IsObject(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsObject(Register input, Register temp1, Label* is_not_object,
                         Label* is_object);

  // Emits optimized code for %_IsString(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsString(Register input, Register temp1, Label* is_not_string,
                         SmiCheck check_needed);

  // Emits optimized code for %_IsConstructCall().
  // Caller should branch on equal condition.
  void EmitIsConstructCall(Register temp1, Register temp2);

  // Emits optimized code to deep-copy the contents of statically known
  // object graphs (e.g. object literal boilerplate).
  void EmitDeepCopy(Handle<JSObject> object, Register result, Register source,
                    int* offset, AllocationSiteMode mode);

  void EnsureSpaceForLazyDeopt(int space_needed) OVERRIDE;
  void DoLoadKeyedExternalArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedArray(LLoadKeyed* instr);
  void DoStoreKeyedExternalArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedArray(LStoreKeyed* instr);

  template <class T>
  void EmitVectorLoadICRegisters(T* instr);

  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<Deoptimizer::JumpTableEntry> jump_table_;
  ZoneList<Handle<Object> > deoptimization_literals_;
  int inlined_function_count_;
  Scope* const scope_;
  TranslationBuffer translations_;
  ZoneList<LDeferredCode*> deferred_;
  int osr_pc_offset_;
  bool frame_is_built_;

  // Builder that keeps track of safepoints in the code. The table
  // itself is emitted at the end of the generated code.
  SafepointTableBuilder safepoints_;

  // Compiler from a set of parallel moves to a sequential list of moves.
  LGapResolver resolver_;

  Safepoint::Kind expected_safepoint_kind_;

  class PushSafepointRegistersScope FINAL BASE_EMBEDDED {
   public:
    explicit PushSafepointRegistersScope(LCodeGen* codegen)
        : codegen_(codegen) {
      DCHECK(codegen_->info()->is_calling());
      DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;
      StoreRegistersStateStub stub(codegen_->isolate());
      codegen_->masm_->CallStub(&stub);
    }

    ~PushSafepointRegistersScope() {
      DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kWithRegisters);
      RestoreRegistersStateStub stub(codegen_->isolate());
      codegen_->masm_->CallStub(&stub);
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


class LDeferredCode : public ZoneObject {
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
  int instruction_index() const { return instruction_index_; }

 protected:
  LCodeGen* codegen() const { return codegen_; }
  MacroAssembler* masm() const { return codegen_->masm(); }

 private:
  LCodeGen* codegen_;
  Label entry_;
  Label exit_;
  Label* external_exit_;
  int instruction_index_;
};
}
}  // namespace v8::internal

#endif  // V8_PPC_LITHIUM_CODEGEN_PPC_H_
