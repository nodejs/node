// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_ARM64_LITHIUM_CODEGEN_ARM64_H_
#define V8_ARM64_LITHIUM_CODEGEN_ARM64_H_

#include "arm64/lithium-arm64.h"

#include "arm64/lithium-gap-resolver-arm64.h"
#include "deoptimizer.h"
#include "lithium-codegen.h"
#include "safepoint-table.h"
#include "scopes.h"
#include "v8utils.h"

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
        deopt_jump_table_(4, info->zone()),
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

  // Support for converting LOperands to assembler types.
  // LOperand must be a register.
  Register ToRegister(LOperand* op) const;
  Register ToRegister32(LOperand* op) const;
  Operand ToOperand(LOperand* op);
  Operand ToOperand32I(LOperand* op);
  Operand ToOperand32U(LOperand* op);
  MemOperand ToMemOperand(LOperand* op) const;
  Handle<Object> ToHandle(LConstantOperand* op) const;

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

  enum IntegerSignedness { SIGNED_INT32, UNSIGNED_INT32 };
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

  Operand ToOperand32(LOperand* op, IntegerSignedness signedness);

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
  void DeoptimizeBranch(
      LEnvironment* environment,
      BranchType branch_type, Register reg = NoReg, int bit = -1,
      Deoptimizer::BailoutType* override_bailout_type = NULL);
  void Deoptimize(LEnvironment* environment,
                  Deoptimizer::BailoutType* override_bailout_type = NULL);
  void DeoptimizeIf(Condition cc, LEnvironment* environment);
  void DeoptimizeIfZero(Register rt, LEnvironment* environment);
  void DeoptimizeIfNotZero(Register rt, LEnvironment* environment);
  void DeoptimizeIfNegative(Register rt, LEnvironment* environment);
  void DeoptimizeIfSmi(Register rt, LEnvironment* environment);
  void DeoptimizeIfNotSmi(Register rt, LEnvironment* environment);
  void DeoptimizeIfRoot(Register rt,
                        Heap::RootListIndex index,
                        LEnvironment* environment);
  void DeoptimizeIfNotRoot(Register rt,
                           Heap::RootListIndex index,
                           LEnvironment* environment);
  void DeoptimizeIfMinusZero(DoubleRegister input, LEnvironment* environment);
  void DeoptimizeIfBitSet(Register rt, int bit, LEnvironment* environment);
  void DeoptimizeIfBitClear(Register rt, int bit, LEnvironment* environment);
  void ApplyCheckIf(Condition cc, LBoundsCheck* check);

  MemOperand PrepareKeyedExternalArrayOperand(Register key,
                                              Register base,
                                              Register scratch,
                                              bool key_is_smi,
                                              bool key_is_constant,
                                              int constant_key,
                                              ElementsKind elements_kind,
                                              int additional_index);
  void CalcKeyedArrayBaseRegister(Register base,
                                  Register elements,
                                  Register key,
                                  bool key_is_tagged,
                                  ElementsKind elements_kind);

  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);

  int GetStackSlotCount() const { return chunk()->spill_slot_count(); }

  void Abort(BailoutReason reason);

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
  void GenerateBodyInstructionPre(LInstruction* instr) V8_OVERRIDE;
  bool GeneratePrologue();
  bool GenerateDeferredCode();
  bool GenerateDeoptJumpTable();
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

  // Generate a direct call to a known function.
  // If the function is already loaded into x1 by the caller, function_reg may
  // be set to x1. Otherwise, it must be NoReg, and CallKnownFunction will
  // automatically load it.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count,
                         int arity,
                         LInstruction* instr,
                         Register function_reg = NoReg);

  // Support for recording safepoint and position information.
  void RecordAndWritePosition(int position) V8_OVERRIDE;
  void RecordSafepoint(LPointerMap* pointers,
                       Safepoint::Kind kind,
                       int arguments,
                       Safepoint::DeoptMode mode);
  void RecordSafepoint(LPointerMap* pointers, Safepoint::DeoptMode mode);
  void RecordSafepoint(Safepoint::DeoptMode mode);
  void RecordSafepointWithRegisters(LPointerMap* pointers,
                                    int arguments,
                                    Safepoint::DeoptMode mode);
  void RecordSafepointWithRegistersAndDoubles(LPointerMap* pointers,
                                              int arguments,
                                              Safepoint::DeoptMode mode);
  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

  void EnsureSpaceForLazyDeopt(int space_needed) V8_OVERRIDE;

  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<Deoptimizer::JumpTableEntry*> deopt_jump_table_;
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
    PushSafepointRegistersScope(LCodeGen* codegen,
                                Safepoint::Kind kind)
        : codegen_(codegen) {
      ASSERT(codegen_->info()->is_calling());
      ASSERT(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->expected_safepoint_kind_ = kind;

      UseScratchRegisterScope temps(codegen_->masm_);
      // Preserve the value of lr which must be saved on the stack (the call to
      // the stub will clobber it).
      Register to_be_pushed_lr =
          temps.UnsafeAcquire(StoreRegistersStateStub::to_be_pushed_lr());
      codegen_->masm_->Mov(to_be_pushed_lr, lr);
      switch (codegen_->expected_safepoint_kind_) {
        case Safepoint::kWithRegisters: {
          StoreRegistersStateStub stub(kDontSaveFPRegs);
          codegen_->masm_->CallStub(&stub);
          break;
        }
        case Safepoint::kWithRegistersAndDoubles: {
          StoreRegistersStateStub stub(kSaveFPRegs);
          codegen_->masm_->CallStub(&stub);
          break;
        }
        default:
          UNREACHABLE();
      }
    }

    ~PushSafepointRegistersScope() {
      Safepoint::Kind kind = codegen_->expected_safepoint_kind_;
      ASSERT((kind & Safepoint::kWithRegisters) != 0);
      switch (kind) {
        case Safepoint::kWithRegisters: {
          RestoreRegistersStateStub stub(kDontSaveFPRegs);
          codegen_->masm_->CallStub(&stub);
          break;
        }
        case Safepoint::kWithRegistersAndDoubles: {
          RestoreRegistersStateStub stub(kSaveFPRegs);
          codegen_->masm_->CallStub(&stub);
          break;
        }
        default:
          UNREACHABLE();
      }
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
