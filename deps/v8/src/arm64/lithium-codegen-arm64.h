// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_LITHIUM_CODEGEN_ARM64_H_
#define V8_ARM64_LITHIUM_CODEGEN_ARM64_H_

#include "src/arm64/lithium-arm64.h"

#include "src/arm64/lithium-gap-resolver-arm64.h"
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
class BranchGenerator;

class LCodeGen: public LCodeGenBase {
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

  // Simple accessors.
  Scope* scope() const { return scope_; }

  int LookupDestination(int block_id) const {
    return chunk()->LookupDestination(block_id);
  }

  bool IsNextEmittedBlock(int block_id) const {
    return LookupDestination(block_id) == GetNextEmittedBlock();
  }

  bool NeedsEagerFrame() const {
    return GetStackSlotCount() > 0 ||
        info()->is_non_deferred_calling() ||
        !info()->IsStub() ||
        info()->requires_frame();
  }
  bool NeedsDeferredFrame() const {
    return !NeedsEagerFrame() && info()->is_deferred_calling();
  }

  LinkRegisterStatus GetLinkRegisterState() const {
    return frame_is_built_ ? kLRHasBeenSaved : kLRHasNotBeenSaved;
  }

  // Try to generate code for the entire chunk, but it may fail if the
  // chunk contains constructs we cannot handle. Returns true if the
  // code generation attempt succeeded.
  bool GenerateCode();

  // Finish the code by setting stack height, safepoint, and bailout
  // information on it.
  void FinishCode(Handle<Code> code);

  enum IntegerSignedness { SIGNED_INT32, UNSIGNED_INT32 };
  // Support for converting LOperands to assembler types.
  Register ToRegister(LOperand* op) const;
  Register ToRegister32(LOperand* op) const;
  Operand ToOperand(LOperand* op);
  Operand ToOperand32(LOperand* op);
  MemOperand ToMemOperand(LOperand* op) const;
  Handle<Object> ToHandle(LConstantOperand* op) const;

  template <class LI>
  Operand ToShiftedRightOperand32(LOperand* right, LI* shift_info);

  int JSShiftAmountFromLConstant(LOperand* constant) {
    return ToInteger32(LConstantOperand::cast(constant)) & 0x1f;
  }

  // TODO(jbramley): Examine these helpers and check that they make sense.
  // IsInteger32Constant returns true for smi constants, for example.
  bool IsInteger32Constant(LConstantOperand* op) const;
  bool IsSmi(LConstantOperand* op) const;

  int32_t ToInteger32(LConstantOperand* op) const;
  Smi* ToSmi(LConstantOperand* op) const;
  double ToDouble(LConstantOperand* op) const;
  DoubleRegister ToDoubleRegister(LOperand* op) const;

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) void Do##type(L##type* node);
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  // Return a double scratch register which can be used locally
  // when generating code for a lithium instruction.
  DoubleRegister double_scratch() { return crankshaft_fp_scratch; }

  // Deferred code support.
  void DoDeferredNumberTagD(LNumberTagD* instr);
  void DoDeferredStackCheck(LStackCheck* instr);
  void DoDeferredStringCharCodeAt(LStringCharCodeAt* instr);
  void DoDeferredStringCharFromCode(LStringCharFromCode* instr);
  void DoDeferredMathAbsTagged(LMathAbsTagged* instr,
                               Label* exit,
                               Label* allocation_entry);

  void DoDeferredNumberTagU(LInstruction* instr,
                            LOperand* value,
                            LOperand* temp1,
                            LOperand* temp2);
  void DoDeferredTaggedToI(LTaggedToI* instr,
                           LOperand* value,
                           LOperand* temp1,
                           LOperand* temp2);
  void DoDeferredAllocate(LAllocate* instr);
  void DoDeferredInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr);
  void DoDeferredInstanceMigration(LCheckMaps* instr, Register object);
  void DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr,
                                   Register result,
                                   Register object,
                                   Register index);

  static Condition TokenToCondition(Token::Value op, bool is_unsigned);
  void EmitGoto(int block);
  void DoGap(LGap* instr);

  // Generic version of EmitBranch. It contains some code to avoid emitting a
  // branch on the next emitted basic block where we could just fall-through.
  // You shouldn't use that directly but rather consider one of the helper like
  // LCodeGen::EmitBranch, LCodeGen::EmitCompareAndBranch...
  template<class InstrType>
  void EmitBranchGeneric(InstrType instr,
                         const BranchGenerator& branch);

  template<class InstrType>
  void EmitBranch(InstrType instr, Condition condition);

  template<class InstrType>
  void EmitCompareAndBranch(InstrType instr,
                            Condition condition,
                            const Register& lhs,
                            const Operand& rhs);

  template<class InstrType>
  void EmitTestAndBranch(InstrType instr,
                         Condition condition,
                         const Register& value,
                         uint64_t mask);

  template<class InstrType>
  void EmitBranchIfNonZeroNumber(InstrType instr,
                                 const FPRegister& value,
                                 const FPRegister& scratch);

  template<class InstrType>
  void EmitBranchIfHeapNumber(InstrType instr,
                              const Register& value);

  template<class InstrType>
  void EmitBranchIfRoot(InstrType instr,
                        const Register& value,
                        Heap::RootListIndex index);

  // Emits optimized code to deep-copy the contents of statically known object
  // graphs (e.g. object literal boilerplate). Expects a pointer to the
  // allocated destination object in the result register, and a pointer to the
  // source object in the source register.
  void EmitDeepCopy(Handle<JSObject> object,
                    Register result,
                    Register source,
                    Register scratch,
                    int* offset,
                    AllocationSiteMode mode);

  template <class T>
  void EmitVectorLoadICRegisters(T* instr);

  // Emits optimized code for %_IsString(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsString(Register input, Register temp1, Label* is_not_string,
                         SmiCheck check_needed);

  int DefineDeoptimizationLiteral(Handle<Object> literal);
  void PopulateDeoptimizationData(Handle<Code> code);
  void PopulateDeoptimizationLiteralsWithInlinedFunctions();

  MemOperand BuildSeqStringOperand(Register string,
                                   Register temp,
                                   LOperand* index,
                                   String::Encoding encoding);
  void DeoptimizeBranch(LInstruction* instr,
                        Deoptimizer::DeoptReason deopt_reason,
                        BranchType branch_type, Register reg = NoReg,
                        int bit = -1,
                        Deoptimizer::BailoutType* override_bailout_type = NULL);
  void Deoptimize(LInstruction* instr, Deoptimizer::DeoptReason deopt_reason,
                  Deoptimizer::BailoutType* override_bailout_type = NULL);
  void DeoptimizeIf(Condition cond, LInstruction* instr,
                    Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfZero(Register rt, LInstruction* instr,
                        Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfNotZero(Register rt, LInstruction* instr,
                           Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfNegative(Register rt, LInstruction* instr,
                            Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfSmi(Register rt, LInstruction* instr,
                       Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfNotSmi(Register rt, LInstruction* instr,
                          Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfRoot(Register rt, Heap::RootListIndex index,
                        LInstruction* instr,
                        Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfNotRoot(Register rt, Heap::RootListIndex index,
                           LInstruction* instr,
                           Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfNotHeapNumber(Register object, LInstruction* instr);
  void DeoptimizeIfMinusZero(DoubleRegister input, LInstruction* instr,
                             Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfBitSet(Register rt, int bit, LInstruction* instr,
                          Deoptimizer::DeoptReason deopt_reason);
  void DeoptimizeIfBitClear(Register rt, int bit, LInstruction* instr,
                            Deoptimizer::DeoptReason deopt_reason);

  MemOperand PrepareKeyedExternalArrayOperand(Register key,
                                              Register base,
                                              Register scratch,
                                              bool key_is_smi,
                                              bool key_is_constant,
                                              int constant_key,
                                              ElementsKind elements_kind,
                                              int base_offset);
  MemOperand PrepareKeyedArrayOperand(Register base,
                                      Register elements,
                                      Register key,
                                      bool key_is_tagged,
                                      ElementsKind elements_kind,
                                      Representation representation,
                                      int base_offset);

  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);

  int GetStackSlotCount() const { return chunk()->spill_slot_count(); }

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }

  // Emit frame translation commands for an environment.
  void WriteTranslation(LEnvironment* environment, Translation* translation);

  void AddToTranslation(LEnvironment* environment,
                        Translation* translation,
                        LOperand* op,
                        bool is_tagged,
                        bool is_uint32,
                        int* object_index_pointer,
                        int* dematerialized_index_pointer);

  void SaveCallerDoubles();
  void RestoreCallerDoubles();

  // Code generation steps.  Returns true if code generation should continue.
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

  void CallCode(Handle<Code> code,
                RelocInfo::Mode mode,
                LInstruction* instr);

  void CallCodeGeneric(Handle<Code> code,
                       RelocInfo::Mode mode,
                       LInstruction* instr,
                       SafepointMode safepoint_mode);

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

  void LoadContextFromDeferred(LOperand* context);
  void CallRuntimeFromDeferred(Runtime::FunctionId id,
                               int argc,
                               LInstruction* instr,
                               LOperand* context);

  // Generate a direct call to a known function.  Expects the function
  // to be in x1.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count, int arity,
                         LInstruction* instr);

  // Support for recording safepoint and position information.
  void RecordAndWritePosition(int position) OVERRIDE;
  void RecordSafepoint(LPointerMap* pointers,
                       Safepoint::Kind kind,
                       int arguments,
                       Safepoint::DeoptMode mode);
  void RecordSafepoint(LPointerMap* pointers, Safepoint::DeoptMode mode);
  void RecordSafepoint(Safepoint::DeoptMode mode);
  void RecordSafepointWithRegisters(LPointerMap* pointers,
                                    int arguments,
                                    Safepoint::DeoptMode mode);
  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

  void EnsureSpaceForLazyDeopt(int space_needed) OVERRIDE;

  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<Deoptimizer::JumpTableEntry*> jump_table_;
  ZoneList<Handle<Object> > deoptimization_literals_;
  int inlined_function_count_;
  Scope* const scope_;
  TranslationBuffer translations_;
  ZoneList<LDeferredCode*> deferred_;
  int osr_pc_offset_;
  bool frame_is_built_;

  // Builder that keeps track of safepoints in the code. The table itself is
  // emitted at the end of the generated code.
  SafepointTableBuilder safepoints_;

  // Compiler from a set of parallel moves to a sequential list of moves.
  LGapResolver resolver_;

  Safepoint::Kind expected_safepoint_kind_;

  int old_position_;

  class PushSafepointRegistersScope BASE_EMBEDDED {
   public:
    explicit PushSafepointRegistersScope(LCodeGen* codegen)
        : codegen_(codegen) {
      DCHECK(codegen_->info()->is_calling());
      DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;

      UseScratchRegisterScope temps(codegen_->masm_);
      // Preserve the value of lr which must be saved on the stack (the call to
      // the stub will clobber it).
      Register to_be_pushed_lr =
          temps.UnsafeAcquire(StoreRegistersStateStub::to_be_pushed_lr());
      codegen_->masm_->Mov(to_be_pushed_lr, lr);
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

  virtual ~LDeferredCode() { }
  virtual void Generate() = 0;
  virtual LInstruction* instr() = 0;

  void SetExit(Label* exit) { external_exit_ = exit; }
  Label* entry() { return &entry_; }
  Label* exit() { return (external_exit_ != NULL) ? external_exit_ : &exit_; }
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


// This is the abstract class used by EmitBranchGeneric.
// It is used to emit code for conditional branching. The Emit() function
// emits code to branch when the condition holds and EmitInverted() emits
// the branch when the inverted condition is verified.
//
// For actual examples of condition see the concrete implementation in
// lithium-codegen-arm64.cc (e.g. BranchOnCondition, CompareAndBranch).
class BranchGenerator BASE_EMBEDDED {
 public:
  explicit BranchGenerator(LCodeGen* codegen)
    : codegen_(codegen) { }

  virtual ~BranchGenerator() { }

  virtual void Emit(Label* label) const = 0;
  virtual void EmitInverted(Label* label) const = 0;

 protected:
  MacroAssembler* masm() const { return codegen_->masm(); }

  LCodeGen* codegen_;
};

} }  // namespace v8::internal

#endif  // V8_ARM64_LITHIUM_CODEGEN_ARM64_H_
