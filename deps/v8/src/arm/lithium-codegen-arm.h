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
#include "safepoint-table.h"
#include "scopes.h"
#include "v8utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LDeferredCode;
class SafepointGenerator;

class LCodeGen BASE_EMBEDDED {
 public:
  LCodeGen(LChunk* chunk, MacroAssembler* assembler, CompilationInfo* info)
      : zone_(info->zone()),
        chunk_(static_cast<LPlatformChunk*>(chunk)),
        masm_(assembler),
        info_(info),
        current_block_(-1),
        current_instruction_(-1),
        instructions_(chunk->instructions()),
        deoptimizations_(4, info->zone()),
        deopt_jump_table_(4, info->zone()),
        deoptimization_literals_(8, info->zone()),
        inlined_function_count_(0),
        scope_(info->scope()),
        status_(UNUSED),
        translations_(info->zone()),
        deferred_(8, info->zone()),
        osr_pc_offset_(-1),
        last_lazy_deopt_pc_(0),
        frame_is_built_(false),
        safepoints_(info->zone()),
        resolver_(this),
        expected_safepoint_kind_(Safepoint::kSimple) {
    PopulateDeoptimizationLiteralsWithInlinedFunctions();
  }


  // Simple accessors.
  MacroAssembler* masm() const { return masm_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return info_->isolate(); }
  Factory* factory() const { return isolate()->factory(); }
  Heap* heap() const { return isolate()->heap(); }
  Zone* zone() const { return zone_; }

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
  int ToInteger32(LConstantOperand* op) const;
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
  void DoDeferredNumberTagI(LInstruction* instr,
                            LOperand* value,
                            IntegerSignedness signedness);

  void DoDeferredTaggedToI(LTaggedToI* instr);
  void DoDeferredMathAbsTaggedHeapNumber(LMathAbs* instr);
  void DoDeferredStackCheck(LStackCheck* instr);
  void DoDeferredRandom(LRandom* instr);
  void DoDeferredStringCharCodeAt(LStringCharCodeAt* instr);
  void DoDeferredStringCharFromCode(LStringCharFromCode* instr);
  void DoDeferredAllocateObject(LAllocateObject* instr);
  void DoDeferredAllocate(LAllocate* instr);
  void DoDeferredInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr,
                                       Label* map_check);

  void DoCheckMapCommon(Register map_reg, Handle<Map> map, LEnvironment* env);

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
  enum Status {
    UNUSED,
    GENERATING,
    DONE,
    ABORTED
  };

  bool is_unused() const { return status_ == UNUSED; }
  bool is_generating() const { return status_ == GENERATING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }

  StrictModeFlag strict_mode_flag() const {
    return info()->is_classic_mode() ? kNonStrictMode : kStrictMode;
  }

  LPlatformChunk* chunk() const { return chunk_; }
  Scope* scope() const { return scope_; }
  HGraph* graph() const { return chunk()->graph(); }

  Register scratch0() { return r9; }
  DwVfpRegister double_scratch0() { return kScratchDoubleReg; }

  int GetNextEmittedBlock() const;
  LInstruction* GetNextInstruction();

  void EmitClassOfTest(Label* if_true,
                       Label* if_false,
                       Handle<String> class_name,
                       Register input,
                       Register temporary,
                       Register temporary2);

  int GetStackSlotCount() const { return chunk()->spill_slot_count(); }

  void Abort(const char* reason);
  void FPRINTF_CHECKING Comment(const char* format, ...);

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }

  // Code generation passes.  Returns true if code generation should
  // continue.
  bool GeneratePrologue();
  bool GenerateBody();
  bool GenerateDeferredCode();
  bool GenerateDeoptJumpTable();
  bool GenerateSafepointTable();

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
                   LInstruction* instr);

  void CallRuntime(Runtime::FunctionId id,
                   int num_arguments,
                   LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, num_arguments, instr);
  }

  void CallRuntimeFromDeferred(Runtime::FunctionId id,
                               int argc,
                               LInstruction* instr);

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
                         CallKind call_kind,
                         R1State r1_state);

  void LoadHeapObject(Register result, Handle<HeapObject> object);

  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);
  void DeoptimizeIf(Condition cc,
                    LEnvironment* environment,
                    Deoptimizer::BailoutType bailout_type);
  void DeoptimizeIf(Condition cc, LEnvironment* environment);
  void SoftDeoptimize(LEnvironment* environment);

  void AddToTranslation(Translation* translation,
                        LOperand* op,
                        bool is_tagged,
                        bool is_uint32);
  void RegisterDependentCodeForEmbeddedMaps(Handle<Code> code);
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);

  void PopulateDeoptimizationLiteralsWithInlinedFunctions();

  Register ToRegister(int index) const;
  DwVfpRegister ToDoubleRegister(int index) const;

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
  void RecordPosition(int position);

  static Condition TokenToCondition(Token::Value op, bool is_unsigned);
  void EmitGoto(int block);
  template<class InstrType>
  void EmitBranch(InstrType instr, Condition cc);
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

  void EmitLoadFieldOrConstantFunction(Register result,
                                       Register object,
                                       Handle<Map> type,
                                       Handle<String> name,
                                       LEnvironment* env);

  // Emits optimized code to deep-copy the contents of statically known
  // object graphs (e.g. object literal boilerplate).
  void EmitDeepCopy(Handle<JSObject> object,
                    Register result,
                    Register source,
                    int* offset,
                    AllocationSiteMode mode);

  // Emit optimized code for integer division.
  // Inputs are signed.
  // All registers are clobbered.
  // If 'remainder' is no_reg, it is not computed.
  void EmitSignedIntegerDivisionByConstant(Register result,
                                           Register dividend,
                                           int32_t divisor,
                                           Register remainder,
                                           Register scratch,
                                           LEnvironment* environment);

  void EnsureSpaceForLazyDeopt();
  void DoLoadKeyedExternalArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedArray(LLoadKeyed* instr);
  void DoStoreKeyedExternalArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedArray(LStoreKeyed* instr);

  Zone* zone_;
  LPlatformChunk* const chunk_;
  MacroAssembler* const masm_;
  CompilationInfo* const info_;

  int current_block_;
  int current_instruction_;
  const ZoneList<LInstruction*>* instructions_;
  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<Deoptimizer::JumpTableEntry> deopt_jump_table_;
  ZoneList<Handle<Object> > deoptimization_literals_;
  int inlined_function_count_;
  Scope* const scope_;
  Status status_;
  TranslationBuffer translations_;
  ZoneList<LDeferredCode*> deferred_;
  int osr_pc_offset_;
  int last_lazy_deopt_pc_;
  bool frame_is_built_;

  // Builder that keeps track of safepoints in the code. The table
  // itself is emitted at the end of the generated code.
  SafepointTableBuilder safepoints_;

  // Compiler from a set of parallel moves to a sequential list of moves.
  LGapResolver resolver_;

  Safepoint::Kind expected_safepoint_kind_;

  class PushSafepointRegistersScope BASE_EMBEDDED {
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
