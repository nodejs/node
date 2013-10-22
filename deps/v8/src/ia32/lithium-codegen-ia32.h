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

#ifndef V8_IA32_LITHIUM_CODEGEN_IA32_H_
#define V8_IA32_LITHIUM_CODEGEN_IA32_H_

#include "ia32/lithium-ia32.h"

#include "checks.h"
#include "deoptimizer.h"
#include "ia32/lithium-gap-resolver-ia32.h"
#include "safepoint-table.h"
#include "scopes.h"
#include "v8utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LDeferredCode;
class LGapNode;
class SafepointGenerator;

class LCodeGen V8_FINAL BASE_EMBEDDED {
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
        jump_table_(4, info->zone()),
        deoptimization_literals_(8, info->zone()),
        inlined_function_count_(0),
        scope_(info->scope()),
        status_(UNUSED),
        translations_(info->zone()),
        deferred_(8, info->zone()),
        dynamic_frame_alignment_(false),
        support_aligned_spilled_doubles_(false),
        osr_pc_offset_(-1),
        last_lazy_deopt_pc_(0),
        frame_is_built_(false),
        x87_stack_(assembler),
        safepoints_(info->zone()),
        resolver_(this),
        expected_safepoint_kind_(Safepoint::kSimple),
        old_position_(RelocInfo::kNoPosition) {
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

  // Support for converting LOperands to assembler types.
  Operand ToOperand(LOperand* op) const;
  Register ToRegister(LOperand* op) const;
  XMMRegister ToDoubleRegister(LOperand* op) const;
  X87Register ToX87Register(LOperand* op) const;

  bool IsInteger32(LConstantOperand* op) const;
  bool IsSmi(LConstantOperand* op) const;
  Immediate ToImmediate(LOperand* op, const Representation& r) const {
    return Immediate(ToRepresentation(LConstantOperand::cast(op), r));
  }
  double ToDouble(LConstantOperand* op) const;

  // Support for non-sse2 (x87) floating point stack handling.
  // These functions maintain the mapping of physical stack registers to our
  // virtual registers between instructions.
  enum X87OperandType { kX87DoubleOperand, kX87FloatOperand, kX87IntOperand };

  void X87Mov(X87Register reg, Operand src,
      X87OperandType operand = kX87DoubleOperand);
  void X87Mov(Operand src, X87Register reg,
      X87OperandType operand = kX87DoubleOperand);

  void X87PrepareBinaryOp(
      X87Register left, X87Register right, X87Register result);

  void X87LoadForUsage(X87Register reg);
  void X87PrepareToWrite(X87Register reg) { x87_stack_.PrepareToWrite(reg); }
  void X87CommitWrite(X87Register reg) { x87_stack_.CommitWrite(reg); }

  void X87Fxch(X87Register reg, int other_slot = 0) {
    x87_stack_.Fxch(reg, other_slot);
  }

  bool X87StackEmpty() {
    return x87_stack_.depth() == 0;
  }

  Handle<Object> ToHandle(LConstantOperand* op) const;

  // The operand denoting the second word (the one with a higher address) of
  // a double stack slot.
  Operand HighOperand(LOperand* op);

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

  void DoDeferredTaggedToI(LTaggedToI* instr, Label* done);
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

  // Emit frame translation commands for an environment.
  void WriteTranslation(LEnvironment* environment, Translation* translation);

  void EnsureRelocSpaceForDeoptimization();

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

  int GetNextEmittedBlock() const;

  void EmitClassOfTest(Label* if_true,
                       Label* if_false,
                       Handle<String> class_name,
                       Register input,
                       Register temporary,
                       Register temporary2);

  int GetStackSlotCount() const { return chunk()->spill_slot_count(); }

  void Abort(BailoutReason reason);
  void FPRINTF_CHECKING Comment(const char* format, ...);

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }

  // Code generation passes.  Returns true if code generation should
  // continue.
  bool GeneratePrologue();
  bool GenerateBody();
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

  void CallRuntime(const Runtime::Function* fun,
                   int argc,
                   LInstruction* instr);

  void CallRuntime(Runtime::FunctionId id,
                   int argc,
                   LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, argc, instr);
  }

  void CallRuntimeFromDeferred(Runtime::FunctionId id,
                               int argc,
                               LInstruction* instr,
                               LOperand* context);

  void LoadContextFromDeferred(LOperand* context);

  enum EDIState {
    EDI_UNINITIALIZED,
    EDI_CONTAINS_TARGET
  };

  // Generate a direct call to a known function.  Expects the function
  // to be in edi.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count,
                         int arity,
                         LInstruction* instr,
                         CallKind call_kind,
                         EDIState edi_state);

  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

  void RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                            Safepoint::DeoptMode mode);
  void DeoptimizeIf(Condition cc,
                    LEnvironment* environment,
                    Deoptimizer::BailoutType bailout_type);
  void DeoptimizeIf(Condition cc, LEnvironment* environment);
  void ApplyCheckIf(Condition cc, LBoundsCheck* check);

  void AddToTranslation(LEnvironment* environment,
                        Translation* translation,
                        LOperand* op,
                        bool is_tagged,
                        bool is_uint32,
                        int* object_index_pointer,
                        int* dematerialized_index_pointer);
  void RegisterDependentCodeForEmbeddedMaps(Handle<Code> code);
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);

  void PopulateDeoptimizationLiteralsWithInlinedFunctions();

  Register ToRegister(int index) const;
  XMMRegister ToDoubleRegister(int index) const;
  X87Register ToX87Register(int index) const;
  int32_t ToRepresentation(LConstantOperand* op, const Representation& r) const;
  int32_t ToInteger32(LConstantOperand* op) const;
  ExternalReference ToExternalReference(LConstantOperand* op) const;

  Operand BuildFastArrayOperand(LOperand* elements_pointer,
                                LOperand* key,
                                Representation key_representation,
                                ElementsKind elements_kind,
                                uint32_t offset,
                                uint32_t additional_index = 0);

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
  void RecordPosition(int position);

  void RecordAndUpdatePosition(int position);

  static Condition TokenToCondition(Token::Value op, bool is_unsigned);
  void EmitGoto(int block);
  template<class InstrType>
  void EmitBranch(InstrType instr, Condition cc);
  template<class InstrType>
  void EmitFalseBranch(InstrType instr, Condition cc);
  void EmitNumberUntagD(
      Register input,
      Register temp,
      XMMRegister result,
      bool allow_undefined_as_nan,
      bool deoptimize_on_minus_zero,
      LEnvironment* env,
      NumberUntagDMode mode = NUMBER_CANDIDATE_IS_ANY_TAGGED);

  void EmitNumberUntagDNoSSE2(
      Register input,
      Register temp,
      X87Register res_reg,
      bool allow_undefined_as_nan,
      bool deoptimize_on_minus_zero,
      LEnvironment* env,
      NumberUntagDMode mode = NUMBER_CANDIDATE_IS_ANY_TAGGED);

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
  void EmitIsConstructCall(Register temp);

  // Emits optimized code to deep-copy the contents of statically known
  // object graphs (e.g. object literal boilerplate).
  void EmitDeepCopy(Handle<JSObject> object,
                    Register result,
                    Register source,
                    int* offset,
                    AllocationSiteMode mode);

  void EnsureSpaceForLazyDeopt();
  void DoLoadKeyedExternalArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr);
  void DoLoadKeyedFixedArray(LLoadKeyed* instr);
  void DoStoreKeyedExternalArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr);
  void DoStoreKeyedFixedArray(LStoreKeyed* instr);

  void EmitReturn(LReturn* instr, bool dynamic_frame_alignment);

  // Emits code for pushing either a tagged constant, a (non-double)
  // register, or a stack slot operand.
  void EmitPushTaggedOperand(LOperand* operand);

  void X87Fld(Operand src, X87OperandType opts);

  void EmitFlushX87ForDeopt();
  void FlushX87StackIfNecessary(LInstruction* instr) {
    x87_stack_.FlushIfNecessary(instr, this);
  }
  friend class LGapResolver;

#ifdef _MSC_VER
  // On windows, you may not access the stack more than one page below
  // the most recently mapped page. To make the allocated area randomly
  // accessible, we write an arbitrary value to each page in range
  // esp + offset - page_size .. esp in turn.
  void MakeSureStackPagesMapped(int offset);
#endif

  Zone* zone_;
  LPlatformChunk* const chunk_;
  MacroAssembler* const masm_;
  CompilationInfo* const info_;

  int current_block_;
  int current_instruction_;
  const ZoneList<LInstruction*>* instructions_;
  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<Deoptimizer::JumpTableEntry> jump_table_;
  ZoneList<Handle<Object> > deoptimization_literals_;
  int inlined_function_count_;
  Scope* const scope_;
  Status status_;
  TranslationBuffer translations_;
  ZoneList<LDeferredCode*> deferred_;
  bool dynamic_frame_alignment_;
  bool support_aligned_spilled_doubles_;
  int osr_pc_offset_;
  int last_lazy_deopt_pc_;
  bool frame_is_built_;

  class X87Stack {
   public:
    explicit X87Stack(MacroAssembler* masm)
        : stack_depth_(0), is_mutable_(true), masm_(masm) { }
    explicit X87Stack(const X87Stack& other)
        : stack_depth_(other.stack_depth_), is_mutable_(false), masm_(masm()) {
      for (int i = 0; i < stack_depth_; i++) {
        stack_[i] = other.stack_[i];
      }
    }
    bool operator==(const X87Stack& other) const {
      if (stack_depth_ != other.stack_depth_) return false;
      for (int i = 0; i < stack_depth_; i++) {
        if (!stack_[i].is(other.stack_[i])) return false;
      }
      return true;
    }
    bool Contains(X87Register reg);
    void Fxch(X87Register reg, int other_slot = 0);
    void Free(X87Register reg);
    void PrepareToWrite(X87Register reg);
    void CommitWrite(X87Register reg);
    void FlushIfNecessary(LInstruction* instr, LCodeGen* cgen);
    void LeavingBlock(int current_block_id, LGoto* goto_instr);
    int depth() const { return stack_depth_; }
    void pop() {
      ASSERT(is_mutable_);
      stack_depth_--;
    }
    void push(X87Register reg) {
      ASSERT(is_mutable_);
      ASSERT(stack_depth_ < X87Register::kNumAllocatableRegisters);
      stack_[stack_depth_] = reg;
      stack_depth_++;
    }

    MacroAssembler* masm() const { return masm_; }

   private:
    int ArrayIndex(X87Register reg);
    int st2idx(int pos);

    X87Register stack_[X87Register::kNumAllocatableRegisters];
    int stack_depth_;
    bool is_mutable_;
    MacroAssembler* masm_;
  };
  X87Stack x87_stack_;

  // Builder that keeps track of safepoints in the code. The table
  // itself is emitted at the end of the generated code.
  SafepointTableBuilder safepoints_;

  // Compiler from a set of parallel moves to a sequential list of moves.
  LGapResolver resolver_;

  Safepoint::Kind expected_safepoint_kind_;

  int old_position_;

  class PushSafepointRegistersScope V8_FINAL  BASE_EMBEDDED {
   public:
    explicit PushSafepointRegistersScope(LCodeGen* codegen)
        : codegen_(codegen) {
      ASSERT(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->masm_->PushSafepointRegisters();
      codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;
      ASSERT(codegen_->info()->is_calling());
    }

    ~PushSafepointRegistersScope() {
      ASSERT(codegen_->expected_safepoint_kind_ == Safepoint::kWithRegisters);
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


class LDeferredCode : public ZoneObject {
 public:
  explicit LDeferredCode(LCodeGen* codegen, const LCodeGen::X87Stack& x87_stack)
      : codegen_(codegen),
        external_exit_(NULL),
        instruction_index_(codegen->current_instruction_),
        x87_stack_(x87_stack) {
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
  const LCodeGen::X87Stack& x87_stack() const { return x87_stack_; }

 protected:
  LCodeGen* codegen() const { return codegen_; }
  MacroAssembler* masm() const { return codegen_->masm(); }

 private:
  LCodeGen* codegen_;
  Label entry_;
  Label exit_;
  Label* external_exit_;
  Label done_;
  int instruction_index_;
  LCodeGen::X87Stack x87_stack_;
};

} }  // namespace v8::internal

#endif  // V8_IA32_LITHIUM_CODEGEN_IA32_H_
