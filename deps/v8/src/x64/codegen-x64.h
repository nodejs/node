// Copyright 2009 the V8 project authors. All rights reserved.
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

#ifndef V8_X64_CODEGEN_X64_H_
#define V8_X64_CODEGEN_X64_H_

namespace v8 {
namespace internal {

// Forward declarations
class DeferredCode;
class RegisterAllocator;
class RegisterFile;

enum InitState { CONST_INIT, NOT_CONST_INIT };
enum TypeofState { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };


// -------------------------------------------------------------------------
// Reference support

// A reference is a C++ stack-allocated object that keeps an ECMA
// reference on the execution stack while in scope. For variables
// the reference is empty, indicating that it isn't necessary to
// store state on the stack for keeping track of references to those.
// For properties, we keep either one (named) or two (indexed) values
// on the execution stack to represent the reference.

class Reference BASE_EMBEDDED {
 public:
  // The values of the types is important, see size().
  enum Type { ILLEGAL = -1, SLOT = 0, NAMED = 1, KEYED = 2 };
  Reference(CodeGenerator* cgen, Expression* expression);
  ~Reference();

  Expression* expression() const { return expression_; }
  Type type() const { return type_; }
  void set_type(Type value) {
    ASSERT(type_ == ILLEGAL);
    type_ = value;
  }

  // The size the reference takes up on the stack.
  int size() const { return (type_ == ILLEGAL) ? 0 : type_; }

  bool is_illegal() const { return type_ == ILLEGAL; }
  bool is_slot() const { return type_ == SLOT; }
  bool is_property() const { return type_ == NAMED || type_ == KEYED; }

  // Return the name.  Only valid for named property references.
  Handle<String> GetName();

  // Generate code to push the value of the reference on top of the
  // expression stack.  The reference is expected to be already on top of
  // the expression stack, and it is left in place with its value above it.
  void GetValue(TypeofState typeof_state);

  // Like GetValue except that the slot is expected to be written to before
  // being read from again.  Thae value of the reference may be invalidated,
  // causing subsequent attempts to read it to fail.
  void TakeValue(TypeofState typeof_state);

  // Generate code to store the value on top of the expression stack in the
  // reference.  The reference is expected to be immediately below the value
  // on the expression stack.  The stored value is left in place (with the
  // reference intact below it) to support chained assignments.
  void SetValue(InitState init_state);

 private:
  CodeGenerator* cgen_;
  Expression* expression_;
  Type type_;
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
  // state.  The new state may or may not be inside a typeof, and has its
  // own control destination.
  CodeGenState(CodeGenerator* owner,
               TypeofState typeof_state,
               ControlDestination* destination);

  // Destroy a code generator state and restore the owning code generator's
  // previous state.
  ~CodeGenState();

  // Accessors for the state.
  TypeofState typeof_state() const { return typeof_state_; }
  ControlDestination* destination() const { return destination_; }

 private:
  // The owning code generator.
  CodeGenerator* owner_;

  // A flag indicating whether we are compiling the immediate subexpression
  // of a typeof expression.
  TypeofState typeof_state_;

  // A control destination in case the expression has a control-flow
  // effect.
  ControlDestination* destination_;

  // The previous state of the owning code generator, restored when
  // this state is destroyed.
  CodeGenState* previous_;
};


// -------------------------------------------------------------------------
// Arguments allocation mode

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
  static Handle<Code> MakeCode(FunctionLiteral* fun,
                               Handle<Script> script,
                               bool is_eval);

#ifdef ENABLE_LOGGING_AND_PROFILING
  static bool ShouldGenerateLog(Expression* type);
#endif

  static void SetFunctionInfo(Handle<JSFunction> fun,
                              int length,
                              int function_token_position,
                              int start_position,
                              int end_position,
                              bool is_expression,
                              bool is_toplevel,
                              Handle<Script> script,
                              Handle<String> inferred_name);

  // Accessors
  MacroAssembler* masm() { return masm_; }

  VirtualFrame* frame() const { return frame_; }

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

 private:
  // Construction/Destruction
  CodeGenerator(int buffer_size, Handle<Script> script, bool is_eval);
  virtual ~CodeGenerator() { delete masm_; }

  // Accessors
  Scope* scope() const { return scope_; }

  // Generating deferred code.
  void ProcessDeferred();

  bool is_eval() { return is_eval_; }

  // State
  TypeofState typeof_state() const { return state_->typeof_state(); }
  ControlDestination* destination() const { return state_->destination(); }

  // Track loop nesting level.
  int loop_nesting() const { return loop_nesting_; }
  void IncrementLoopNesting() { loop_nesting_++; }
  void DecrementLoopNesting() { loop_nesting_--; }


  // Node visitors.
  void VisitStatements(ZoneList<Statement*>* statements);

#define DEF_VISIT(type) \
  void Visit##type(type* node);
  NODE_LIST(DEF_VISIT)
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
  void GenCode(FunctionLiteral* fun);

  // Generate the return sequence code.  Should be called no more than
  // once per compiled function, immediately after binding the return
  // target (which can not be done more than once).
  void GenerateReturnSequence(Result* return_value);

  // Returns the arguments allocation mode.
  ArgumentsAllocationMode ArgumentsMode() const;

  // Store the arguments object and allocate it if necessary.
  Result StoreArgumentsObject(bool initial);

  // The following are used by class Reference.
  void LoadReference(Reference* ref);
  void UnloadReference(Reference* ref);

  Operand ContextOperand(Register context, int index) const {
    return Operand(context, Context::SlotOffset(index));
  }

  Operand SlotOperand(Slot* slot, Register tmp);

  Operand ContextSlotOperandCheckExtensions(Slot* slot,
                                            Result tmp,
                                            JumpTarget* slow);

  // Expressions
  Operand GlobalObject() const {
    return ContextOperand(rsi, Context::GLOBAL_INDEX);
  }

  void LoadCondition(Expression* x,
                     TypeofState typeof_state,
                     ControlDestination* destination,
                     bool force_control);
  void Load(Expression* x, TypeofState typeof_state = NOT_INSIDE_TYPEOF);
  void LoadGlobal();
  void LoadGlobalReceiver();

  // Generate code to push the value of an expression on top of the frame
  // and then spill the frame fully to memory.  This function is used
  // temporarily while the code generator is being transformed.
  void LoadAndSpill(Expression* expression,
                    TypeofState typeof_state = NOT_INSIDE_TYPEOF);

  // Read a value from a slot and leave it on top of the expression stack.
  void LoadFromSlot(Slot* slot, TypeofState typeof_state);
  void LoadFromSlotCheckForArguments(Slot* slot, TypeofState state);
  Result LoadFromGlobalSlotCheckExtensions(Slot* slot,
                                           TypeofState typeof_state,
                                           JumpTarget* slow);

  // Store the value on top of the expression stack into a slot, leaving the
  // value in place.
  void StoreToSlot(Slot* slot, InitState init_state);

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

  void GenericBinaryOperation(
      Token::Value op,
      SmiAnalysis* type,
      OverwriteMode overwrite_mode);

  // If possible, combine two constant smi values using op to produce
  // a smi result, and push it on the virtual frame, all at compile time.
  // Returns true if it succeeds.  Otherwise it has no effect.
  bool FoldConstantSmis(Token::Value op, int left, int right);

  // Emit code to perform a binary operation on a constant
  // smi and a likely smi.  Consumes the Result *operand.
  void ConstantSmiBinaryOperation(Token::Value op,
                                  Result* operand,
                                  Handle<Object> constant_operand,
                                  SmiAnalysis* type,
                                  bool reversed,
                                  OverwriteMode overwrite_mode);

  // Emit code to perform a binary operation on two likely smis.
  // The code to handle smi arguments is produced inline.
  // Consumes the Results *left and *right.
  void LikelySmiBinaryOperation(Token::Value op,
                                Result* left,
                                Result* right,
                                OverwriteMode overwrite_mode);

  void Comparison(Condition cc,
                  bool strict,
                  ControlDestination* destination);

  // To prevent long attacker-controlled byte sequences, integer constants
  // from the JavaScript source are loaded in two parts if they are larger
  // than 16 bits.
  static const int kMaxSmiInlinedBits = 16;
  bool IsUnsafeSmi(Handle<Object> value);
  // Load an integer constant x into a register target using
  // at most 16 bits of user-controlled data per assembly operation.
  void LoadUnsafeSmi(Register target, Handle<Object> value);

  void CallWithArguments(ZoneList<Expression*>* arguments, int position);

  void CheckStack();

  struct InlineRuntimeLUT {
    void (CodeGenerator::*method)(ZoneList<Expression*>*);
    const char* name;
  };
  static InlineRuntimeLUT* FindInlineRuntimeLUT(Handle<String> name);
  bool CheckForInlineRuntimeCall(CallRuntime* node);
  static bool PatchInlineRuntimeEntry(Handle<String> name,
                                      const InlineRuntimeLUT& new_entry,
                                      InlineRuntimeLUT* old_entry);
  Handle<JSFunction> BuildBoilerplate(FunctionLiteral* node);
  void ProcessDeclarations(ZoneList<Declaration*>* declarations);

  Handle<Code> ComputeCallInitialize(int argc, InLoopFlag in_loop);

  // Declare global variables and functions in the given array of
  // name/value pairs.
  void DeclareGlobals(Handle<FixedArray> pairs);

  // Instantiate the function boilerplate.
  void InstantiateBoilerplate(Handle<JSFunction> boilerplate);

  // Support for type checks.
  void GenerateIsSmi(ZoneList<Expression*>* args);
  void GenerateIsNonNegativeSmi(ZoneList<Expression*>* args);
  void GenerateIsArray(ZoneList<Expression*>* args);

  // Support for construct call checks.
  void GenerateIsConstructCall(ZoneList<Expression*>* args);

  // Support for arguments.length and arguments[?].
  void GenerateArgumentsLength(ZoneList<Expression*>* args);
  void GenerateArgumentsAccess(ZoneList<Expression*>* args);

  // Support for accessing the class and value fields of an object.
  void GenerateClassOf(ZoneList<Expression*>* args);
  void GenerateValueOf(ZoneList<Expression*>* args);
  void GenerateSetValueOf(ZoneList<Expression*>* args);

  // Fast support for charCodeAt(n).
  void GenerateFastCharCodeAt(ZoneList<Expression*>* args);

  // Fast support for object equality testing.
  void GenerateObjectEquals(ZoneList<Expression*>* args);

  void GenerateLog(ZoneList<Expression*>* args);

  // Fast support for Math.random().
  void GenerateRandomPositiveSmi(ZoneList<Expression*>* args);

  // Fast support for Math.sin and Math.cos.
  enum MathOp { SIN, COS };
  void GenerateFastMathOp(MathOp op, ZoneList<Expression*>* args);
  inline void GenerateMathSin(ZoneList<Expression*>* args);
  inline void GenerateMathCos(ZoneList<Expression*>* args);

  // Methods used to indicate which source code is generated for. Source
  // positions are collected by the assembler and emitted with the relocation
  // information.
  void CodeForFunctionPosition(FunctionLiteral* fun);
  void CodeForReturnPosition(FunctionLiteral* fun);
  void CodeForStatementPosition(Node* node);
  void CodeForSourcePosition(int pos);

#ifdef DEBUG
  // True if the registers are valid for entry to a block.  There should
  // be no frame-external references to (non-reserved) registers.
  bool HasValidEntryRegisters();
#endif

  bool is_eval_;  // Tells whether code is generated for eval.
  Handle<Script> script_;
  ZoneList<DeferredCode*> deferred_;

  // Assembler
  MacroAssembler* masm_;  // to generate code

  // Code generation state
  Scope* scope_;
  VirtualFrame* frame_;
  RegisterAllocator* allocator_;
  CodeGenState* state_;
  int loop_nesting_;

  // Jump targets.
  // The target of the return from the function.
  BreakTarget function_return_;

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

  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};


} }  // namespace v8::internal

#endif  // V8_X64_CODEGEN_X64_H_
