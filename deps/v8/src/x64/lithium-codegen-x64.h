// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_X64_LITHIUM_CODEGEN_X64_H_
#define V8_X64_LITHIUM_CODEGEN_X64_H_

#include "x64/lithium-x64.h"

#include "checks.h"
#include "deoptimizer.h"
#include "safepoint-table.h"
#include "scopes.h"
#include "x64/lithium-gap-resolver-x64.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LDeferredCode;
class SafepointGenerator;

class LCodeGen BASE_EMBEDDED {
 public:
  LCodeGen(LChunk* chunk, MacroAssembler* assembler, CompilationInfo* info)
      : chunk_(chunk),
        masm_(assembler),
        info_(info),
        current_block_(-1),
        current_instruction_(-1),
        instructions_(chunk->instructions()),
        deoptimizations_(4),
        jump_table_(4),
        deoptimization_literals_(8),
        inlined_function_count_(0),
        scope_(chunk->graph()->info()->scope()),
        status_(UNUSED),
        deferred_(8),
        osr_pc_offset_(-1),
        resolver_(this),
        expected_safepoint_kind_(Safepoint::kSimple) {
    PopulateDeoptimizationLiteralsWithInlinedFunctions();
  }

  // Simple accessors.
  MacroAssembler* masm() const { return masm_; }

  // Support for converting LOperands to assembler types.
  Register ToRegister(LOperand* op) const;
  XMMRegister ToDoubleRegister(LOperand* op) const;
  bool IsInteger32Constant(LConstantOperand* op) const;
  int ToInteger32(LConstantOperand* op) const;
  bool IsTaggedConstant(LConstantOperand* op) const;
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
  void DoDeferredTaggedToI(LTaggedToI* instr);
  void DoDeferredMathAbsTaggedHeapNumber(LUnaryMathOperation* instr);
  void DoDeferredStackCheck(LGoto* instr);
  void DoDeferredStringCharCodeAt(LStringCharCodeAt* instr);
  void DoDeferredLInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr);

  // Parallel move support.
  void DoParallelMove(LParallelMove* move);

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

  int strict_mode_flag() const {
    return info_->is_strict() ? kStrictMode : kNonStrictMode;
  }

  LChunk* chunk() const { return chunk_; }
  Scope* scope() const { return scope_; }
  HGraph* graph() const { return chunk_->graph(); }

  int GetNextEmittedBlock(int block);
  LInstruction* GetNextInstruction();

  void EmitClassOfTest(Label* if_true,
                       Label* if_false,
                       Handle<String> class_name,
                       Register input,
                       Register temporary);

  int StackSlotCount() const { return chunk()->spill_slot_count(); }
  int ParameterCount() const { return scope()->num_parameters(); }

  void Abort(const char* format, ...);
  void Comment(const char* format, ...);

  void AddDeferredCode(LDeferredCode* code) { deferred_.Add(code); }

  // Code generation passes.  Returns true if code generation should
  // continue.
  bool GeneratePrologue();
  bool GenerateBody();
  bool GenerateDeferredCode();
  bool GenerateJumpTable();
  bool GenerateSafepointTable();

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

  void CallRuntime(Runtime::Function* function,
                   int num_arguments,
                   LInstruction* instr);

  void CallRuntime(Runtime::FunctionId id,
                   int num_arguments,
                   LInstruction* instr) {
    Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, num_arguments, instr);
  }

  void CallRuntimeFromDeferred(Runtime::FunctionId id,
                               int argc,
                               LInstruction* instr);


  // Generate a direct call to a known function.  Expects the function
  // to be in edi.
  void CallKnownFunction(Handle<JSFunction> function,
                         int arity,
                         LInstruction* instr);

  void LoadHeapObject(Register result, Handle<HeapObject> object);

  void RegisterLazyDeoptimization(LInstruction* instr,
                                  SafepointMode safepoint_mode,
                                  int argc);
  void RegisterEnvironmentForDeoptimization(LEnvironment* environment);
  void DeoptimizeIf(Condition cc, LEnvironment* environment);

  void AddToTranslation(Translation* translation,
                        LOperand* op,
                        bool is_tagged);
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);

  void PopulateDeoptimizationLiteralsWithInlinedFunctions();

  Register ToRegister(int index) const;
  XMMRegister ToDoubleRegister(int index) const;

  // Specific math operations - used from DoUnaryMathOperation.
  void EmitIntegerMathAbs(LUnaryMathOperation* instr);
  void DoMathAbs(LUnaryMathOperation* instr);
  void DoMathFloor(LUnaryMathOperation* instr);
  void DoMathRound(LUnaryMathOperation* instr);
  void DoMathSqrt(LUnaryMathOperation* instr);
  void DoMathPowHalf(LUnaryMathOperation* instr);
  void DoMathLog(LUnaryMathOperation* instr);
  void DoMathCos(LUnaryMathOperation* instr);
  void DoMathSin(LUnaryMathOperation* instr);

  // Support for recording safepoint and position information.
  void RecordSafepoint(LPointerMap* pointers,
                       Safepoint::Kind kind,
                       int arguments,
                       int deoptimization_index);
  void RecordSafepoint(LPointerMap* pointers, int deoptimization_index);
  void RecordSafepoint(int deoptimization_index);
  void RecordSafepointWithRegisters(LPointerMap* pointers,
                                    int arguments,
                                    int deoptimization_index);
  void RecordPosition(int position);

  static Condition TokenToCondition(Token::Value op, bool is_unsigned);
  void EmitGoto(int block, LDeferredCode* deferred_stack_check = NULL);
  void EmitBranch(int left_block, int right_block, Condition cc);
  void EmitCmpI(LOperand* left, LOperand* right);
  void EmitNumberUntagD(Register input, XMMRegister result, LEnvironment* env);

  // Emits optimized code for typeof x == "y".  Modifies input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitTypeofIs(Label* true_label, Label* false_label,
                         Register input, Handle<String> type_name);

  // Emits optimized code for %_IsObject(x).  Preserves input register.
  // Returns the condition on which a final split to
  // true and false label should be made, to optimize fallthrough.
  Condition EmitIsObject(Register input,
                         Label* is_not_object,
                         Label* is_object);

  // Emits optimized code for %_IsConstructCall().
  // Caller should branch on equal condition.
  void EmitIsConstructCall(Register temp);

  // Emits code for pushing a constant operand.
  void EmitPushConstantOperand(LOperand* operand);

  struct JumpTableEntry {
    inline JumpTableEntry(Address address)
        : label_(),
          address_(address) { }
    Label label_;
    Address address_;
  };

  LChunk* const chunk_;
  MacroAssembler* const masm_;
  CompilationInfo* const info_;

  int current_block_;
  int current_instruction_;
  const ZoneList<LInstruction*>* instructions_;
  ZoneList<LEnvironment*> deoptimizations_;
  ZoneList<JumpTableEntry*> jump_table_;
  ZoneList<Handle<Object> > deoptimization_literals_;
  int inlined_function_count_;
  Scope* const scope_;
  Status status_;
  TranslationBuffer translations_;
  ZoneList<LDeferredCode*> deferred_;
  int osr_pc_offset_;

  // Builder that keeps track of safepoints in the code. The table
  // itself is emitted at the end of the generated code.
  SafepointTableBuilder safepoints_;

  // Compiler from a set of parallel moves to a sequential list of moves.
  LGapResolver resolver_;

  Safepoint::Kind expected_safepoint_kind_;

  class PushSafepointRegistersScope BASE_EMBEDDED {
   public:
    explicit PushSafepointRegistersScope(LCodeGen* codegen)
        : codegen_(codegen) {
      ASSERT(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
      codegen_->masm_->PushSafepointRegisters();
      codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;
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


class LDeferredCode: public ZoneObject {
 public:
  explicit LDeferredCode(LCodeGen* codegen)
      : codegen_(codegen), external_exit_(NULL) {
    codegen->AddDeferredCode(this);
  }

  virtual ~LDeferredCode() { }
  virtual void Generate() = 0;

  void SetExit(Label *exit) { external_exit_ = exit; }
  Label* entry() { return &entry_; }
  Label* exit() { return external_exit_ != NULL ? external_exit_ : &exit_; }

 protected:
  LCodeGen* codegen() const { return codegen_; }
  MacroAssembler* masm() const { return codegen_->masm(); }

 private:
  LCodeGen* codegen_;
  Label entry_;
  Label exit_;
  Label* external_exit_;
};

} }  // namespace v8::internal

#endif  // V8_X64_LITHIUM_CODEGEN_X64_H_
