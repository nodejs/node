// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_X87_LITHIUM_CODEGEN_X87_H_
#define V8_CRANKSHAFT_X87_LITHIUM_CODEGEN_X87_H_

#include <map>

#include "src/ast/scopes.h"
#include "src/base/logging.h"
#include "src/crankshaft/lithium-codegen.h"
#include "src/crankshaft/x87/lithium-gap-resolver-x87.h"
#include "src/crankshaft/x87/lithium-x87.h"
#include "src/deoptimizer.h"
#include "src/safepoint-table.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LDeferredCode;
class LGapNode;
class SafepointGenerator;

class LCodeGen: public LCodeGenBase {
 public:
  LCodeGen(LChunk* chunk, MacroAssembler* assembler, CompilationInfo* info)
      : LCodeGenBase(chunk, assembler, info),
        jump_table_(4, info->zone()),
        scope_(info->scope()),
        deferred_(8, info->zone()),
        frame_is_built_(false),
        x87_stack_(assembler),
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
  Operand ToOperand(LOperand* op) const;
  Register ToRegister(LOperand* op) const;
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
  void X87Mov(X87Register reg, X87Register src,
              X87OperandType operand = kX87DoubleOperand);

  void X87PrepareBinaryOp(
      X87Register left, X87Register right, X87Register result);

  void X87LoadForUsage(X87Register reg);
  void X87LoadForUsage(X87Register reg1, X87Register reg2);
  void X87PrepareToWrite(X87Register reg) { x87_stack_.PrepareToWrite(reg); }
  void X87CommitWrite(X87Register reg) { x87_stack_.CommitWrite(reg); }

  void X87Fxch(X87Register reg, int other_slot = 0) {
    x87_stack_.Fxch(reg, other_slot);
  }
  void X87Free(X87Register reg) {
    x87_stack_.Free(reg);
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
  void DoDeferredNumberTagIU(LInstruction* instr,
                             LOperand* value,
                             LOperand* temp,
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

  void EnsureRelocSpaceForDeoptimization();

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) void Do##type(L##type* node);
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  Scope* scope() const { return scope_; }

  void EmitClassOfTest(Label* if_true, Label* if_false,
                       Handle<String> class_name, Register input,
                       Register temporary, Register temporary2);

  bool HasAllocatedStackSlots() const {
    return chunk()->HasAllocatedStackSlots();
  }
  int GetStackSlotCount() const { return chunk()->GetSpillSlotCount(); }
  int GetTotalFrameSlotCount() const {
    return chunk()->GetTotalFrameSlotCount();
  }

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code, zone()); }

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
    RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS
  };

  void CallCode(Handle<Code> code,
                RelocInfo::Mode mode,
                LInstruction* instr);

  void CallCodeGeneric(Handle<Code> code,
                       RelocInfo::Mode mode,
                       LInstruction* instr,
                       SafepointMode safepoint_mode);

  void CallRuntime(const Runtime::Function* fun, int argc, LInstruction* instr,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);

  void CallRuntime(Runtime::FunctionId id,
                   int argc,
                   LInstruction* instr) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, argc, instr);
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

  // Generate a direct call to a known function. Expects the function
  // to be in edi.
  void CallKnownFunction(Handle<JSFunction> function,
                         int formal_parameter_count, int arity,
                         bool is_tail_call, LInstruction* instr);

  void RecordSafepointWithLazyDeopt(LInstruction* instr,
                                    SafepointMode safepoint_mode);

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
  X87Register ToX87Register(int index) const;
  int32_t ToRepresentation(LConstantOperand* op, const Representation& r) const;
  int32_t ToInteger32(LConstantOperand* op) const;
  ExternalReference ToExternalReference(LConstantOperand* op) const;

  Operand BuildFastArrayOperand(LOperand* elements_pointer,
                                LOperand* key,
                                Representation key_representation,
                                ElementsKind elements_kind,
                                uint32_t base_offset);

  Operand BuildSeqStringOperand(Register string,
                                LOperand* index,
                                String::Encoding encoding);

  void EmitIntegerMathAbs(LMathAbs* instr);

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
  void EmitNumberUntagDNoSSE2(LNumberUntagD* instr, Register input,
                              Register temp, X87Register res_reg,
                              NumberUntagDMode mode);

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

  void EmitReturn(LReturn* instr);

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

  ZoneList<Deoptimizer::JumpTableEntry> jump_table_;
  Scope* const scope_;
  ZoneList<LDeferredCode*> deferred_;
  bool frame_is_built_;

  class X87Stack : public ZoneObject {
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
    X87Stack& operator=(const X87Stack& other) {
      stack_depth_ = other.stack_depth_;
      for (int i = 0; i < stack_depth_; i++) {
        stack_[i] = other.stack_[i];
      }
      return *this;
    }
    bool Contains(X87Register reg);
    void Fxch(X87Register reg, int other_slot = 0);
    void Free(X87Register reg);
    void PrepareToWrite(X87Register reg);
    void CommitWrite(X87Register reg);
    void FlushIfNecessary(LInstruction* instr, LCodeGen* cgen);
    void LeavingBlock(int current_block_id, LGoto* goto_instr, LCodeGen* cgen);
    int depth() const { return stack_depth_; }
    int GetLayout();
    int st(X87Register reg) { return st2idx(ArrayIndex(reg)); }
    void pop() {
      DCHECK(is_mutable_);
      USE(is_mutable_);
      stack_depth_--;
    }
    void push(X87Register reg) {
      DCHECK(is_mutable_);
      DCHECK(stack_depth_ < X87Register::kMaxNumAllocatableRegisters);
      stack_[stack_depth_] = reg;
      stack_depth_++;
    }

    MacroAssembler* masm() const { return masm_; }
    Isolate* isolate() const { return masm_->isolate(); }

   private:
    int ArrayIndex(X87Register reg);
    int st2idx(int pos);

    X87Register stack_[X87Register::kMaxNumAllocatableRegisters];
    int stack_depth_;
    bool is_mutable_;
    MacroAssembler* masm_;
  };
  X87Stack x87_stack_;
  // block_id -> X87Stack*;
  typedef std::map<int, X87Stack*> X87StackMap;
  X87StackMap x87_stack_map_;

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
      DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->masm_->PushSafepointRegisters();
      codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;
      DCHECK(codegen_->info()->is_calling());
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
  friend class X87Stack;
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

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_X87_LITHIUM_CODEGEN_X87_H_
