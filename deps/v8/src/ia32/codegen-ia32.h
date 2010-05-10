// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_IA32_CODEGEN_IA32_H_
#define V8_IA32_CODEGEN_IA32_H_

#include "ic-inl.h"

namespace v8 {
namespace internal {

// Forward declarations
class CompilationInfo;
class DeferredCode;
class RegisterAllocator;
class RegisterFile;

enum InitState { CONST_INIT, NOT_CONST_INIT };
enum TypeofState { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };


// -------------------------------------------------------------------------
// Reference support

// A reference is a C++ stack-allocated object that puts a
// reference on the virtual frame.  The reference may be consumed
// by GetValue, TakeValue and SetValue.
// When the lifetime (scope) of a valid reference ends, it must have
// been consumed, and be in state UNLOADED.
class Reference BASE_EMBEDDED {
 public:
  // The values of the types is important, see size().
  enum Type { UNLOADED = -2, ILLEGAL = -1, SLOT = 0, NAMED = 1, KEYED = 2 };
  Reference(CodeGenerator* cgen,
            Expression* expression,
            bool persist_after_get = false);
  ~Reference();

  Expression* expression() const { return expression_; }
  Type type() const { return type_; }
  void set_type(Type value) {
    ASSERT_EQ(ILLEGAL, type_);
    type_ = value;
  }

  void set_unloaded() {
    ASSERT_NE(ILLEGAL, type_);
    ASSERT_NE(UNLOADED, type_);
    type_ = UNLOADED;
  }
  // The size the reference takes up on the stack.
  int size() const {
    return (type_ < SLOT) ? 0 : type_;
  }

  bool is_illegal() const { return type_ == ILLEGAL; }
  bool is_slot() const { return type_ == SLOT; }
  bool is_property() const { return type_ == NAMED || type_ == KEYED; }
  bool is_unloaded() const { return type_ == UNLOADED; }

  // Return the name.  Only valid for named property references.
  Handle<String> GetName();

  // Generate code to push the value of the reference on top of the
  // expression stack.  The reference is expected to be already on top of
  // the expression stack, and it is consumed by the call unless the
  // reference is for a compound assignment.
  // If the reference is not consumed, it is left in place under its value.
  void GetValue();

  // Like GetValue except that the slot is expected to be written to before
  // being read from again.  The value of the reference may be invalidated,
  // causing subsequent attempts to read it to fail.
  void TakeValue();

  // Generate code to store the value on top of the expression stack in the
  // reference.  The reference is expected to be immediately below the value
  // on the expression stack.  The  value is stored in the location specified
  // by the reference, and is left on top of the stack, after the reference
  // is popped from beneath it (unloaded).
  void SetValue(InitState init_state);

 private:
  CodeGenerator* cgen_;
  Expression* expression_;
  Type type_;
  // Keep the reference on the stack after get, so it can be used by set later.
  bool persist_after_get_;
};


// -------------------------------------------------------------------------
// Control destinations.

// A control destination encapsulates a pair of jump targets and a
// flag indicating which one is the preferred fall-through.  The
// preferred fall-through must be unbound, the other may be already
// bound (ie, a backward target).
//
// The true and false targets may be jumped to unconditionally or
// control may split conditionally.  Unconditional jumping and
// splitting should be emitted in tail position (as the last thing
// when compiling an expression) because they can cause either label
// to be bound or the non-fall through to be jumped to leaving an
// invalid virtual frame.
//
// The labels in the control destination can be extracted and
// manipulated normally without affecting the state of the
// destination.

class ControlDestination BASE_EMBEDDED {
 public:
  ControlDestination(JumpTarget* true_target,
                     JumpTarget* false_target,
                     bool true_is_fall_through)
      : true_target_(true_target),
        false_target_(false_target),
        true_is_fall_through_(true_is_fall_through),
        is_used_(false) {
    ASSERT(true_is_fall_through ? !true_target->is_bound()
                                : !false_target->is_bound());
  }

  // Accessors for the jump targets.  Directly jumping or branching to
  // or binding the targets will not update the destination's state.
  JumpTarget* true_target() const { return true_target_; }
  JumpTarget* false_target() const { return false_target_; }

  // True if the the destination has been jumped to unconditionally or
  // control has been split to both targets.  This predicate does not
  // test whether the targets have been extracted and manipulated as
  // raw jump targets.
  bool is_used() const { return is_used_; }

  // True if the destination is used and the true target (respectively
  // false target) was the fall through.  If the target is backward,
  // "fall through" included jumping unconditionally to it.
  bool true_was_fall_through() const {
    return is_used_ && true_is_fall_through_;
  }

  bool false_was_fall_through() const {
    return is_used_ && !true_is_fall_through_;
  }

  // Emit a branch to one of the true or false targets, and bind the
  // other target.  Because this binds the fall-through target, it
  // should be emitted in tail position (as the last thing when
  // compiling an expression).
  void Split(Condition cc) {
    ASSERT(!is_used_);
    if (true_is_fall_through_) {
      false_target_->Branch(NegateCondition(cc));
      true_target_->Bind();
    } else {
      true_target_->Branch(cc);
      false_target_->Bind();
    }
    is_used_ = true;
  }

  // Emit an unconditional jump in tail position, to the true target
  // (if the argument is true) or the false target.  The "jump" will
  // actually bind the jump target if it is forward, jump to it if it
  // is backward.
  void Goto(bool where) {
    ASSERT(!is_used_);
    JumpTarget* target = where ? true_target_ : false_target_;
    if (target->is_bound()) {
      target->Jump();
    } else {
      target->Bind();
    }
    is_used_ = true;
    true_is_fall_through_ = where;
  }

  // Mark this jump target as used as if Goto had been called, but
  // without generating a jump or binding a label (the control effect
  // should have already happened).  This is used when the left
  // subexpression of the short-circuit boolean operators are
  // compiled.
  void Use(bool where) {
    ASSERT(!is_used_);
    ASSERT((where ? true_target_ : false_target_)->is_bound());
    is_used_ = true;
    true_is_fall_through_ = where;
  }

  // Swap the true and false targets but keep the same actual label as
  // the fall through.  This is used when compiling negated
  // expressions, where we want to swap the targets but preserve the
  // state.
  void Invert() {
    JumpTarget* temp_target = true_target_;
    true_target_ = false_target_;
    false_target_ = temp_target;

    true_is_fall_through_ = !true_is_fall_through_;
  }

 private:
  // True and false jump targets.
  JumpTarget* true_target_;
  JumpTarget* false_target_;

  // Before using the destination: true if the true target is the
  // preferred fall through, false if the false target is.  After
  // using the destination: true if the true target was actually used
  // as the fall through, false if the false target was.
  bool true_is_fall_through_;

  // True if the Split or Goto functions have been called.
  bool is_used_;
};


// -------------------------------------------------------------------------
// Code generation state

// The state is passed down the AST by the code generator (and back up, in
// the form of the state of the jump target pair).  It is threaded through
// the call stack.  Constructing a state implicitly pushes it on the owning
// code generator's stack of states, and destroying one implicitly pops it.
//
// The code generator state is only used for expressions, so statements have
// the initial state.

class CodeGenState BASE_EMBEDDED {
 public:
  // Create an initial code generator state.  Destroying the initial state
  // leaves the code generator with a NULL state.
  explicit CodeGenState(CodeGenerator* owner);

  // Create a code generator state based on a code generator's current
  // state.  The new state has its own control destination.
  CodeGenState(CodeGenerator* owner, ControlDestination* destination);

  // Destroy a code generator state and restore the owning code generator's
  // previous state.
  ~CodeGenState();

  // Accessors for the state.
  ControlDestination* destination() const { return destination_; }

 private:
  // The owning code generator.
  CodeGenerator* owner_;

  // A control destination in case the expression has a control-flow
  // effect.
  ControlDestination* destination_;

  // The previous state of the owning code generator, restored when
  // this state is destroyed.
  CodeGenState* previous_;
};


// -------------------------------------------------------------------------
// Arguments allocation mode.

enum ArgumentsAllocationMode {
  NO_ARGUMENTS_ALLOCATION,
  EAGER_ARGUMENTS_ALLOCATION,
  LAZY_ARGUMENTS_ALLOCATION
};


// -------------------------------------------------------------------------
// CodeGenerator

class CodeGenerator: public AstVisitor {
 public:
  // Takes a function literal, generates code for it. This function should only
  // be called by compiler.cc.
  static Handle<Code> MakeCode(CompilationInfo* info);

  // Printing of AST, etc. as requested by flags.
  static void MakeCodePrologue(CompilationInfo* info);

  // Allocate and install the code.
  static Handle<Code> MakeCodeEpilogue(MacroAssembler* masm,
                                       Code::Flags flags,
                                       CompilationInfo* info);

#ifdef ENABLE_LOGGING_AND_PROFILING
  static bool ShouldGenerateLog(Expression* type);
#endif

  static void RecordPositions(MacroAssembler* masm, int pos);

  // Accessors
  MacroAssembler* masm() { return masm_; }
  VirtualFrame* frame() const { return frame_; }
  inline Handle<Script> script();

  bool has_valid_frame() const { return frame_ != NULL; }

  // Set the virtual frame to be new_frame, with non-frame register
  // reference counts given by non_frame_registers.  The non-frame
  // register reference counts of the old frame are returned in
  // non_frame_registers.
  void SetFrame(VirtualFrame* new_frame, RegisterFile* non_frame_registers);

  void DeleteFrame();

  RegisterAllocator* allocator() const { return allocator_; }

  CodeGenState* state() { return state_; }
  void set_state(CodeGenState* state) { state_ = state; }

  void AddDeferred(DeferredCode* code) { deferred_.Add(code); }

  bool in_spilled_code() const { return in_spilled_code_; }
  void set_in_spilled_code(bool flag) { in_spilled_code_ = flag; }

  // If the name is an inline runtime function call return the number of
  // expected arguments. Otherwise return -1.
  static int InlineRuntimeCallArgumentsCount(Handle<String> name);

 private:
  // Construction/Destruction
  explicit CodeGenerator(MacroAssembler* masm);

  // Accessors
  inline bool is_eval();
  inline Scope* scope();

  // Generating deferred code.
  void ProcessDeferred();

  // State
  ControlDestination* destination() const { return state_->destination(); }

  // Control of side-effect-free int32 expression compilation.
  bool in_safe_int32_mode() { return in_safe_int32_mode_; }
  void set_in_safe_int32_mode(bool value) { in_safe_int32_mode_ = value; }
  bool safe_int32_mode_enabled() {
    return FLAG_safe_int32_compiler && safe_int32_mode_enabled_;
  }
  void set_safe_int32_mode_enabled(bool value) {
    safe_int32_mode_enabled_ = value;
  }
  void set_unsafe_bailout(BreakTarget* unsafe_bailout) {
    unsafe_bailout_ = unsafe_bailout;
  }

  // Take the Result that is an untagged int32, and convert it to a tagged
  // Smi or HeapNumber.  Remove the untagged_int32 flag from the result.
  void ConvertInt32ResultToNumber(Result* value);
  void ConvertInt32ResultToSmi(Result* value);

  // Track loop nesting level.
  int loop_nesting() const { return loop_nesting_; }
  void IncrementLoopNesting() { loop_nesting_++; }
  void DecrementLoopNesting() { loop_nesting_--; }

  // Node visitors.
  void VisitStatements(ZoneList<Statement*>* statements);

#define DEF_VISIT(type) \
  void Visit##type(type* node);
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  // Visit a statement and then spill the virtual frame if control flow can
  // reach the end of the statement (ie, it does not exit via break,
  // continue, return, or throw).  This function is used temporarily while
  // the code generator is being transformed.
  void VisitAndSpill(Statement* statement);

  // Visit a list of statements and then spill the virtual frame if control
  // flow can reach the end of the list.
  void VisitStatementsAndSpill(ZoneList<Statement*>* statements);

  // Main code generation function
  void Generate(CompilationInfo* info);

  // Generate the return sequence code.  Should be called no more than
  // once per compiled function, immediately after binding the return
  // target (which can not be done more than once).
  void GenerateReturnSequence(Result* return_value);

  // Returns the arguments allocation mode.
  ArgumentsAllocationMode ArgumentsMode();

  // Store the arguments object and allocate it if necessary.
  Result StoreArgumentsObject(bool initial);

  // The following are used by class Reference.
  void LoadReference(Reference* ref);

  static Operand ContextOperand(Register context, int index) {
    return Operand(context, Context::SlotOffset(index));
  }

  Operand SlotOperand(Slot* slot, Register tmp);

  Operand ContextSlotOperandCheckExtensions(Slot* slot,
                                            Result tmp,
                                            JumpTarget* slow);

  // Expressions
  static Operand GlobalObject() {
    return ContextOperand(esi, Context::GLOBAL_INDEX);
  }

  void LoadCondition(Expression* expr,
                     ControlDestination* destination,
                     bool force_control);
  void Load(Expression* expr);
  void LoadGlobal();
  void LoadGlobalReceiver();

  // Generate code to push the value of an expression on top of the frame
  // and then spill the frame fully to memory.  This function is used
  // temporarily while the code generator is being transformed.
  void LoadAndSpill(Expression* expression);

  // Evaluate an expression and place its value on top of the frame,
  // using, or not using, the side-effect-free expression compiler.
  void LoadInSafeInt32Mode(Expression* expr, BreakTarget* unsafe_bailout);
  void LoadWithSafeInt32ModeDisabled(Expression* expr);

  // Read a value from a slot and leave it on top of the expression stack.
  Result LoadFromSlot(Slot* slot, TypeofState typeof_state);
  Result LoadFromSlotCheckForArguments(Slot* slot, TypeofState typeof_state);
  Result LoadFromGlobalSlotCheckExtensions(Slot* slot,
                                           TypeofState typeof_state,
                                           JumpTarget* slow);

  // Store the value on top of the expression stack into a slot, leaving the
  // value in place.
  void StoreToSlot(Slot* slot, InitState init_state);

  // Support for compiling assignment expressions.
  void EmitSlotAssignment(Assignment* node);
  void EmitNamedPropertyAssignment(Assignment* node);
  void EmitKeyedPropertyAssignment(Assignment* node);

  // Receiver is passed on the frame and consumed.
  Result EmitNamedLoad(Handle<String> name, bool is_contextual);

  // If the store is contextual, value is passed on the frame and consumed.
  // Otherwise, receiver and value are passed on the frame and consumed.
  Result EmitNamedStore(Handle<String> name, bool is_contextual);

  // Receiver and key are passed on the frame and consumed.
  Result EmitKeyedLoad();

  // Receiver, key, and value are passed on the frame and consumed.
  Result EmitKeyedStore(StaticType* key_type);

  // Special code for typeof expressions: Unfortunately, we must
  // be careful when loading the expression in 'typeof'
  // expressions. We are not allowed to throw reference errors for
  // non-existing properties of the global object, so we must make it
  // look like an explicit property access, instead of an access
  // through the context chain.
  void LoadTypeofExpression(Expression* x);

  // Translate the value on top of the frame into control flow to the
  // control destination.
  void ToBoolean(ControlDestination* destination);

  // Generate code that computes a shortcutting logical operation.
  void GenerateLogicalBooleanOperation(BinaryOperation* node);

  void GenericBinaryOperation(BinaryOperation* expr,
                              OverwriteMode overwrite_mode);

  // If possible, combine two constant smi values using op to produce
  // a smi result, and push it on the virtual frame, all at compile time.
  // Returns true if it succeeds.  Otherwise it has no effect.
  bool FoldConstantSmis(Token::Value op, int left, int right);

  // Emit code to perform a binary operation on a constant
  // smi and a likely smi.  Consumes the Result operand.
  Result ConstantSmiBinaryOperation(BinaryOperation* expr,
                                    Result* operand,
                                    Handle<Object> constant_operand,
                                    bool reversed,
                                    OverwriteMode overwrite_mode);

  // Emit code to perform a binary operation on two likely smis.
  // The code to handle smi arguments is produced inline.
  // Consumes the Results left and right.
  Result LikelySmiBinaryOperation(BinaryOperation* expr,
                                  Result* left,
                                  Result* right,
                                  OverwriteMode overwrite_mode);


  // Emit code to perform a binary operation on two untagged int32 values.
  // The values are on top of the frame, and the result is pushed on the frame.
  void Int32BinaryOperation(BinaryOperation* node);


  void Comparison(AstNode* node,
                  Condition cc,
                  bool strict,
                  ControlDestination* destination);
  void GenerateInlineNumberComparison(Result* left_side,
                                      Result* right_side,
                                      Condition cc,
                                      ControlDestination* dest);

  // To prevent long attacker-controlled byte sequences, integer constants
  // from the JavaScript source are loaded in two parts if they are larger
  // than 17 bits.
  static const int kMaxSmiInlinedBits = 17;
  bool IsUnsafeSmi(Handle<Object> value);
  // Load an integer constant x into a register target or into the stack using
  // at most 16 bits of user-controlled data per assembly operation.
  void MoveUnsafeSmi(Register target, Handle<Object> value);
  void StoreUnsafeSmiToLocal(int offset, Handle<Object> value);
  void PushUnsafeSmi(Handle<Object> value);

  void CallWithArguments(ZoneList<Expression*>* arguments,
                         CallFunctionFlags flags,
                         int position);

  // An optimized implementation of expressions of the form
  // x.apply(y, arguments).  We call x the applicand and y the receiver.
  // The optimization avoids allocating an arguments object if possible.
  void CallApplyLazy(Expression* applicand,
                     Expression* receiver,
                     VariableProxy* arguments,
                     int position);

  void CheckStack();

  struct InlineRuntimeLUT {
    void (CodeGenerator::*method)(ZoneList<Expression*>*);
    const char* name;
    int nargs;
  };

  static InlineRuntimeLUT* FindInlineRuntimeLUT(Handle<String> name);
  bool CheckForInlineRuntimeCall(CallRuntime* node);
  static bool PatchInlineRuntimeEntry(Handle<String> name,
                                      const InlineRuntimeLUT& new_entry,
                                      InlineRuntimeLUT* old_entry);

  void ProcessDeclarations(ZoneList<Declaration*>* declarations);

  static Handle<Code> ComputeCallInitialize(int argc, InLoopFlag in_loop);

  // Declare global variables and functions in the given array of
  // name/value pairs.
  void DeclareGlobals(Handle<FixedArray> pairs);

  // Instantiate the function based on the shared function info.
  Result InstantiateFunction(Handle<SharedFunctionInfo> function_info);

  // Support for type checks.
  void GenerateIsSmi(ZoneList<Expression*>* args);
  void GenerateIsNonNegativeSmi(ZoneList<Expression*>* args);
  void GenerateIsArray(ZoneList<Expression*>* args);
  void GenerateIsRegExp(ZoneList<Expression*>* args);
  void GenerateIsObject(ZoneList<Expression*>* args);
  void GenerateIsFunction(ZoneList<Expression*>* args);
  void GenerateIsUndetectableObject(ZoneList<Expression*>* args);

  // Support for construct call checks.
  void GenerateIsConstructCall(ZoneList<Expression*>* args);

  // Support for arguments.length and arguments[?].
  void GenerateArgumentsLength(ZoneList<Expression*>* args);
  void GenerateArguments(ZoneList<Expression*>* args);

  // Support for accessing the class and value fields of an object.
  void GenerateClassOf(ZoneList<Expression*>* args);
  void GenerateValueOf(ZoneList<Expression*>* args);
  void GenerateSetValueOf(ZoneList<Expression*>* args);

  // Fast support for charCodeAt(n).
  void GenerateFastCharCodeAt(ZoneList<Expression*>* args);

  // Fast support for string.charAt(n) and string[n].
  void GenerateCharFromCode(ZoneList<Expression*>* args);

  // Fast support for object equality testing.
  void GenerateObjectEquals(ZoneList<Expression*>* args);

  void GenerateLog(ZoneList<Expression*>* args);

  void GenerateGetFramePointer(ZoneList<Expression*>* args);

  // Fast support for Math.random().
  void GenerateRandomHeapNumber(ZoneList<Expression*>* args);

  // Fast support for StringAdd.
  void GenerateStringAdd(ZoneList<Expression*>* args);

  // Fast support for SubString.
  void GenerateSubString(ZoneList<Expression*>* args);

  // Fast support for StringCompare.
  void GenerateStringCompare(ZoneList<Expression*>* args);

  // Support for direct calls from JavaScript to native RegExp code.
  void GenerateRegExpExec(ZoneList<Expression*>* args);

  void GenerateRegExpConstructResult(ZoneList<Expression*>* args);

  // Support for fast native caches.
  void GenerateGetFromCache(ZoneList<Expression*>* args);

  // Fast support for number to string.
  void GenerateNumberToString(ZoneList<Expression*>* args);

  // Fast swapping of elements. Takes three expressions, the object and two
  // indices. This should only be used if the indices are known to be
  // non-negative and within bounds of the elements array at the call site.
  void GenerateSwapElements(ZoneList<Expression*>* args);

  // Fast call for custom callbacks.
  void GenerateCallFunction(ZoneList<Expression*>* args);

  // Fast call to math functions.
  void GenerateMathPow(ZoneList<Expression*>* args);
  void GenerateMathSin(ZoneList<Expression*>* args);
  void GenerateMathCos(ZoneList<Expression*>* args);
  void GenerateMathSqrt(ZoneList<Expression*>* args);

  // Simple condition analysis.
  enum ConditionAnalysis {
    ALWAYS_TRUE,
    ALWAYS_FALSE,
    DONT_KNOW
  };
  ConditionAnalysis AnalyzeCondition(Expression* cond);

  // Methods used to indicate which source code is generated for. Source
  // positions are collected by the assembler and emitted with the relocation
  // information.
  void CodeForFunctionPosition(FunctionLiteral* fun);
  void CodeForReturnPosition(FunctionLiteral* fun);
  void CodeForStatementPosition(Statement* stmt);
  void CodeForDoWhileConditionPosition(DoWhileStatement* stmt);
  void CodeForSourcePosition(int pos);

  void SetTypeForStackSlot(Slot* slot, TypeInfo info);

#ifdef DEBUG
  // True if the registers are valid for entry to a block.  There should
  // be no frame-external references to (non-reserved) registers.
  bool HasValidEntryRegisters();
#endif

  ZoneList<DeferredCode*> deferred_;

  // Assembler
  MacroAssembler* masm_;  // to generate code

  CompilationInfo* info_;

  // Code generation state
  VirtualFrame* frame_;
  RegisterAllocator* allocator_;
  CodeGenState* state_;
  int loop_nesting_;
  bool in_safe_int32_mode_;
  bool safe_int32_mode_enabled_;

  // Jump targets.
  // The target of the return from the function.
  BreakTarget function_return_;
  // The target of the bailout from a side-effect-free int32 subexpression.
  BreakTarget* unsafe_bailout_;

  // True if the function return is shadowed (ie, jumping to the target
  // function_return_ does not jump to the true function return, but rather
  // to some unlinking code).
  bool function_return_is_shadowed_;

  // True when we are in code that expects the virtual frame to be fully
  // spilled.  Some virtual frame function are disabled in DEBUG builds when
  // called from spilled code, because they do not leave the virtual frame
  // in a spilled state.
  bool in_spilled_code_;

  static InlineRuntimeLUT kInlineRuntimeLUT[];

  friend class VirtualFrame;
  friend class JumpTarget;
  friend class Reference;
  friend class Result;
  friend class FastCodeGenerator;
  friend class FullCodeGenerator;
  friend class FullCodeGenSyntaxChecker;

  friend class CodeGeneratorPatcher;  // Used in test-log-stack-tracer.cc

  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};


// Compute a transcendental math function natively, or call the
// TranscendentalCache runtime function.
class TranscendentalCacheStub: public CodeStub {
 public:
  explicit TranscendentalCacheStub(TranscendentalCache::Type type)
      : type_(type) {}
  void Generate(MacroAssembler* masm);
 private:
  TranscendentalCache::Type type_;
  Major MajorKey() { return TranscendentalCache; }
  int MinorKey() { return type_; }
  Runtime::FunctionId RuntimeFunction();
  void GenerateOperation(MacroAssembler* masm);
};


// Flag that indicates how to generate code for the stub GenericBinaryOpStub.
enum GenericBinaryFlags {
  NO_GENERIC_BINARY_FLAGS = 0,
  NO_SMI_CODE_IN_STUB = 1 << 0  // Omit smi code in stub.
};


class GenericBinaryOpStub: public CodeStub {
 public:
  GenericBinaryOpStub(Token::Value op,
                      OverwriteMode mode,
                      GenericBinaryFlags flags,
                      TypeInfo operands_type)
      : op_(op),
        mode_(mode),
        flags_(flags),
        args_in_registers_(false),
        args_reversed_(false),
        static_operands_type_(operands_type),
        runtime_operands_type_(BinaryOpIC::DEFAULT),
        name_(NULL) {
    if (static_operands_type_.IsSmi()) {
      mode_ = NO_OVERWRITE;
    }
    use_sse3_ = CpuFeatures::IsSupported(SSE3);
    ASSERT(OpBits::is_valid(Token::NUM_TOKENS));
  }

  GenericBinaryOpStub(int key, BinaryOpIC::TypeInfo runtime_operands_type)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        flags_(FlagBits::decode(key)),
        args_in_registers_(ArgsInRegistersBits::decode(key)),
        args_reversed_(ArgsReversedBits::decode(key)),
        use_sse3_(SSE3Bits::decode(key)),
        static_operands_type_(TypeInfo::ExpandedRepresentation(
            StaticTypeInfoBits::decode(key))),
        runtime_operands_type_(runtime_operands_type),
        name_(NULL) {
  }

  // Generate code to call the stub with the supplied arguments. This will add
  // code at the call site to prepare arguments either in registers or on the
  // stack together with the actual call.
  void GenerateCall(MacroAssembler* masm, Register left, Register right);
  void GenerateCall(MacroAssembler* masm, Register left, Smi* right);
  void GenerateCall(MacroAssembler* masm, Smi* left, Register right);

  Result GenerateCall(MacroAssembler* masm,
                      VirtualFrame* frame,
                      Result* left,
                      Result* right);

 private:
  Token::Value op_;
  OverwriteMode mode_;
  GenericBinaryFlags flags_;
  bool args_in_registers_;  // Arguments passed in registers not on the stack.
  bool args_reversed_;  // Left and right argument are swapped.
  bool use_sse3_;

  // Number type information of operands, determined by code generator.
  TypeInfo static_operands_type_;

  // Operand type information determined at runtime.
  BinaryOpIC::TypeInfo runtime_operands_type_;

  char* name_;

  const char* GetName();

#ifdef DEBUG
  void Print() {
    PrintF("GenericBinaryOpStub %d (op %s), "
           "(mode %d, flags %d, registers %d, reversed %d, type_info %s)\n",
           MinorKey(),
           Token::String(op_),
           static_cast<int>(mode_),
           static_cast<int>(flags_),
           static_cast<int>(args_in_registers_),
           static_cast<int>(args_reversed_),
           static_operands_type_.ToString());
  }
#endif

  // Minor key encoding in 18 bits RRNNNFRASOOOOOOOMM.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 7> {};
  class SSE3Bits: public BitField<bool, 9, 1> {};
  class ArgsInRegistersBits: public BitField<bool, 10, 1> {};
  class ArgsReversedBits: public BitField<bool, 11, 1> {};
  class FlagBits: public BitField<GenericBinaryFlags, 12, 1> {};
  class StaticTypeInfoBits: public BitField<int, 13, 3> {};
  class RuntimeTypeInfoBits: public BitField<BinaryOpIC::TypeInfo, 16, 2> {};

  Major MajorKey() { return GenericBinaryOp; }
  int MinorKey() {
    // Encode the parameters in a unique 18 bit value.
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | FlagBits::encode(flags_)
           | SSE3Bits::encode(use_sse3_)
           | ArgsInRegistersBits::encode(args_in_registers_)
           | ArgsReversedBits::encode(args_reversed_)
           | StaticTypeInfoBits::encode(
                 static_operands_type_.ThreeBitRepresentation())
           | RuntimeTypeInfoBits::encode(runtime_operands_type_);
  }

  void Generate(MacroAssembler* masm);
  void GenerateSmiCode(MacroAssembler* masm, Label* slow);
  void GenerateLoadArguments(MacroAssembler* masm);
  void GenerateReturn(MacroAssembler* masm);
  void GenerateHeapResultAllocation(MacroAssembler* masm, Label* alloc_failure);
  void GenerateRegisterArgsPush(MacroAssembler* masm);
  void GenerateTypeTransition(MacroAssembler* masm);

  bool ArgsInRegistersSupported() {
    return op_ == Token::ADD || op_ == Token::SUB
        || op_ == Token::MUL || op_ == Token::DIV;
  }
  bool IsOperationCommutative() {
    return (op_ == Token::ADD) || (op_ == Token::MUL);
  }

  void SetArgsInRegisters() { args_in_registers_ = true; }
  void SetArgsReversed() { args_reversed_ = true; }
  bool HasSmiCodeInStub() { return (flags_ & NO_SMI_CODE_IN_STUB) == 0; }
  bool HasArgsInRegisters() { return args_in_registers_; }
  bool HasArgsReversed() { return args_reversed_; }

  bool ShouldGenerateSmiCode() {
    return HasSmiCodeInStub() &&
        runtime_operands_type_ != BinaryOpIC::HEAP_NUMBERS &&
        runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  bool ShouldGenerateFPCode() {
    return runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  virtual int GetCodeKind() { return Code::BINARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return BinaryOpIC::ToState(runtime_operands_type_);
  }
};


class StringHelper : public AllStatic {
 public:
  // Generates fast code for getting a char code out of a string
  // object at the given index. May bail out for four reasons (in the
  // listed order):
  //   * Receiver is not a string (receiver_not_string label).
  //   * Index is not a smi (index_not_smi label).
  //   * Index is out of range (index_out_of_range).
  //   * Some other reason (slow_case label). In this case it's
  //     guaranteed that the above conditions are not violated,
  //     e.g. it's safe to assume the receiver is a string and the
  //     index is a non-negative smi < length.
  // When successful, object, index, and scratch are clobbered.
  // Otherwise, scratch and result are clobbered.
  static void GenerateFastCharCodeAt(MacroAssembler* masm,
                                     Register object,
                                     Register index,
                                     Register scratch,
                                     Register result,
                                     Label* receiver_not_string,
                                     Label* index_not_smi,
                                     Label* index_out_of_range,
                                     Label* slow_case);

  // Generates code for creating a one-char string from the given char
  // code. May do a runtime call, so any register can be clobbered
  // and, if the given invoke flag specifies a call, an internal frame
  // is required. In tail call mode the result must be eax register.
  static void GenerateCharFromCode(MacroAssembler* masm,
                                   Register code,
                                   Register result,
                                   InvokeFlag flag);

  // Generate code for copying characters using a simple loop. This should only
  // be used in places where the number of characters is small and the
  // additional setup and checking in GenerateCopyCharactersREP adds too much
  // overhead. Copying of overlapping regions is not supported.
  static void GenerateCopyCharacters(MacroAssembler* masm,
                                     Register dest,
                                     Register src,
                                     Register count,
                                     Register scratch,
                                     bool ascii);

  // Generate code for copying characters using the rep movs instruction.
  // Copies ecx characters from esi to edi. Copying of overlapping regions is
  // not supported.
  static void GenerateCopyCharactersREP(MacroAssembler* masm,
                                        Register dest,     // Must be edi.
                                        Register src,      // Must be esi.
                                        Register count,    // Must be ecx.
                                        Register scratch,  // Neither of above.
                                        bool ascii);

  // Probe the symbol table for a two character string. If the string is
  // not found by probing a jump to the label not_found is performed. This jump
  // does not guarantee that the string is not in the symbol table. If the
  // string is found the code falls through with the string in register eax.
  static void GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                                   Register c1,
                                                   Register c2,
                                                   Register scratch1,
                                                   Register scratch2,
                                                   Register scratch3,
                                                   Label* not_found);

  // Generate string hash.
  static void GenerateHashInit(MacroAssembler* masm,
                               Register hash,
                               Register character,
                               Register scratch);
  static void GenerateHashAddCharacter(MacroAssembler* masm,
                                       Register hash,
                                       Register character,
                                       Register scratch);
  static void GenerateHashGetHash(MacroAssembler* masm,
                                  Register hash,
                                  Register scratch);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
};


// Flag that indicates how to generate code for the stub StringAddStub.
enum StringAddFlags {
  NO_STRING_ADD_FLAGS = 0,
  NO_STRING_CHECK_IN_STUB = 1 << 0  // Omit string check in stub.
};


class StringAddStub: public CodeStub {
 public:
  explicit StringAddStub(StringAddFlags flags) {
    string_check_ = ((flags & NO_STRING_CHECK_IN_STUB) == 0);
  }

 private:
  Major MajorKey() { return StringAdd; }
  int MinorKey() { return string_check_ ? 0 : 1; }

  void Generate(MacroAssembler* masm);

  // Should the stub check whether arguments are strings?
  bool string_check_;
};


class SubStringStub: public CodeStub {
 public:
  SubStringStub() {}

 private:
  Major MajorKey() { return SubString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class StringCompareStub: public CodeStub {
 public:
  explicit StringCompareStub() {
  }

  // Compare two flat ascii strings and returns result in eax after popping two
  // arguments from the stack.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3);

 private:
  Major MajorKey() { return StringCompare; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class NumberToStringStub: public CodeStub {
 public:
  NumberToStringStub() { }

  // Generate code to do a lookup in the number string cache. If the number in
  // the register object is found in the cache the generated code falls through
  // with the result in the result register. The object and the result register
  // can be the same. If the number is not found in the cache the code jumps to
  // the label not_found with only the content of register object unchanged.
  static void GenerateLookupNumberStringCache(MacroAssembler* masm,
                                              Register object,
                                              Register result,
                                              Register scratch1,
                                              Register scratch2,
                                              bool object_is_smi,
                                              Label* not_found);

 private:
  Major MajorKey() { return NumberToString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "NumberToStringStub"; }

#ifdef DEBUG
  void Print() {
    PrintF("NumberToStringStub\n");
  }
#endif
};


class RecordWriteStub : public CodeStub {
 public:
  RecordWriteStub(Register object, Register addr, Register scratch)
      : object_(object), addr_(addr), scratch_(scratch) { }

  void Generate(MacroAssembler* masm);

 private:
  Register object_;
  Register addr_;
  Register scratch_;

#ifdef DEBUG
  void Print() {
    PrintF("RecordWriteStub (object reg %d), (addr reg %d), (scratch reg %d)\n",
           object_.code(), addr_.code(), scratch_.code());
  }
#endif

  // Minor key encoding in 12 bits. 4 bits for each of the three
  // registers (object, address and scratch) OOOOAAAASSSS.
  class ScratchBits: public BitField<uint32_t, 0, 4> {};
  class AddressBits: public BitField<uint32_t, 4, 4> {};
  class ObjectBits: public BitField<uint32_t, 8, 4> {};

  Major MajorKey() { return RecordWrite; }

  int MinorKey() {
    // Encode the registers.
    return ObjectBits::encode(object_.code()) |
           AddressBits::encode(addr_.code()) |
           ScratchBits::encode(scratch_.code());
  }
};


} }  // namespace v8::internal

#endif  // V8_IA32_CODEGEN_IA32_H_
