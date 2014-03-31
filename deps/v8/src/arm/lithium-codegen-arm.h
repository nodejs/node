// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_ARM_LITHIUM_CODEGEN_ARM_H_
#define V8_ARM_LITHIUM_CODEGEN_ARM_H_

#include "arm/lithium-arm.h"

#include "arm/lithium-gap-resolver-arm.h"
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

  // Support for converting LOperands to assembler types.
  // LOperand must be a register.
  Register ToRegister(LOperand* op) const;

  // LOperand is loaded into scratch, unless already a register.
  Register EmitLoadRegister(LOperand* op, Register scratch);

  // LOperand must be a double register.
  DwVfpRegister ToDoubleRegister(LOperand* op) const;

  // LOperand is loaded into dbl_scratch, unless already a double register.
  DwVfpRegister EmitLoadDoubleRegister(LOperand* op,
                                       SwVfpRegister flt_scratch,
                                       DwVfpRegister dbl_scratch);
  int32_t ToRepresentation(LConstantOperand* op, const Representation& r) const;
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
  void DoDeferredNumberTagIU(LInstruction* instr,
                             LOperand* value,
                             LOperand* temp1,
                             LOperand* temp2,
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

  // Parallel move support.
  void DoParallelMove(LParallelMove* move);
  void DoGap(LGap* instr);

  MemOperand PrepareKeyedOperand(Register key,
                                 Register base,
                                 bool key_is_constant,
                                 int constant_key,
                                 int element_size,
                                 int shift_size,
                                 int additional_index,
                                 int additional_offset);

  // Emit frame translation commands for an environment.
  void WriteTranslation(LEnvironment* environment, Translation* translation);

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) void Do##type(L##type* node);
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  StrictMode strict_mode() const { return info()->strict_mode(); }

  Scope* scope() const { return scope_; }

  Register scratch0() { return r9; }
  LowDwVfpRegister double_scratch0() { return kScratchDoubleReg; }

  LInstruction* GetNextInstruction();

  void EmitClassOfTest(Label* if_true,
                       Label* if_false,
                       Handle<String> class_name,
                       Register input,
                       Register temporary,
                       Register temporary2);

  int GetStackSlotCount() const { return chunk()->spill_slot_count(); }

  void Abort(BailoutReason reason);

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }

  void SaveCallerDoubles();
  void RestoreCallerDoubles();

  // Code generation passes.  Returns true if code generation should
  // continue.
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

  void CallCode(
      Handle<Code> code,
      RelocInfo::Mode mode,
      LInstruction* instr,
      TargetAddressStorageMode storage_mode = CAN_INLINE_TARGET_ADDRESS);

  void CallCodeGeneric(
      Handle<Code> code,
      RelocInfo::Mode mode,
      LInstruction* instr,
      SafepointMode safepoint_mode,
      TargetAddressStorageMode storage_mode = CAN_INLINE_TARGET_ADDRESS);

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

  enum R1State {
    R1_UNINITIALIZED,
    R1_CONTAINS_TARGET
  };

  // Generate a direct call to a known function.  Expects the function
  // to be in r1.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count,
                         int arity,
                         LInstruction* instr,
                         R1State r1_state);

  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);
  void DeoptimizeIf(Condition condition,
                    LEnvironment* environment,
                    Deoptimizer::BailoutType bailout_type);
  void DeoptimizeIf(Condition condition, LEnvironment* environment);
  void ApplyCheckIf(Condition condition, LBoundsCheck* check);

  void AddToTranslation(LEnvironment* environment,
                        Translation* translation,
                        LOperand* op,
                        bool is_tagged,
                        bool is_uint32,
                        int* object_index_pointer,
                        int* dematerialized_index_pointer);
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);

  void PopulateDeoptimizationLiteralsWithInlinedFunctions();

  Register ToRegister(int index) const;
  DwVfpRegister ToDoubleRegister(int index) const;

  MemOperand BuildSeqStringOperand(Register string,
                                   LOperand* index,
                                   String::Encoding encoding);

  void EmitIntegerMathAbs(LMathAbs* instr);

  // Support for recording safepoint and position information.
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

  void RecordAndWritePosition(int position) V8_OVERRIDE;

  static Condition TokenToCondition(Token::Value op, bool is_unsigned);
  void EmitGoto(int block);

  // EmitBranch expects to be the last instruction of a block.
  template<class InstrType>
  void EmitBranch(InstrType instr, Condition condition);
  template<class InstrType>
  void EmitFalseBranch(InstrType instr, Condition condition);
  void EmitNumberUntagD(Register input,
                        DwVfpRegister result,
                        bool allow_undefined_as_nan,
                        bool deoptimize_on_minus_zero,
                        LEnvironment* env,
                        NumberUntagDMode mode);

  // Emits optimized code for typeof x == "y".  Modifies input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitTypeofIs(Label* true_label,
                         Label* false_label,
                         Register input,
                         Handle<String> type_name);

  // Emits optimized code for %_IsObject(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsObject(Register input,
                         Register temp1,
                         Label* is_not_object,
                         Label* is_object);

  // Emits optimized code for %_IsString(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsString(Register input,
                         Register temp1,
                         Label* is_not_string,
                         SmiCheck check_needed);

  // Emits optimized code for %_IsConstructCall().
  // Caller should branch on equal condition.
  void EmitIsConstructCall(Register temp1, Register temp2);

  // Emits optimized code to deep-copy the contents of statically known
  // object graphs (e.g. object literal boilerplate).
  void EmitDeepCopy(Handle<JSObject> object,
                    Register result,
                    Register source,
                    int* offset,
                    AllocationSiteMode mode);

  void EnsureSpaceForLazyDeopt(int space_needed) V8_OVERRIDE;
  void DoLoadKeyedExternalArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedArray(LLoadKeyed* instr);
  void DoStoreKeyedExternalArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedArray(LStoreKeyed* instr);

  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<Deoptimizer::JumpTableEntry> deopt_jump_table_;
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

  class PushSafepointRegistersScope V8_FINAL BASE_EMBEDDED {
   public:
    PushSafepointRegistersScope(LCodeGen* codegen,
                                Safepoint::Kind kind)
        : codegen_(codegen) {
      ASSERT(codegen_->info()->is_calling());
      ASSERT(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->expected_safepoint_kind_ = kind;

      switch (codegen_->expected_safepoint_kind_) {
        case Safepoint::kWithRegisters:
          codegen_->masm_->PushSafepointRegisters();
          break;
        case Safepoint::kWithRegistersAndDoubles:
          codegen_->masm_->PushSafepointRegistersAndDoubles();
          break;
        default:
          UNREACHABLE();
      }
    }

    ~PushSafepointRegistersScope() {
      Safepoint::Kind kind = codegen_->expected_safepoint_kind_;
      ASSERT((kind & Safepoint::kWithRegisters) != 0);
      switch (kind) {
        case Safepoint::kWithRegisters:
          codegen_->masm_->PopSafepointRegisters();
          break;
        case Safepoint::kWithRegistersAndDoubles:
          codegen_->masm_->PopSafepointRegistersAndDoubles();
          break;
        default:
          UNREACHABLE();
      }
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

} }  // namespace v8::internal

#endif  // V8_ARM_LITHIUM_CODEGEN_ARM_H_
