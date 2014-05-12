// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FULL_CODEGEN_H_
#define V8_FULL_CODEGEN_H_

#include "v8.h"

#include "allocation.h"
#include "assert-scope.h"
#include "ast.h"
#include "code-stubs.h"
#include "codegen.h"
#include "compiler.h"
#include "data-flow.h"
#include "globals.h"
#include "objects.h"

namespace v8 {
namespace internal {

// Forward declarations.
class JumpPatchSite;

// AST node visitor which can tell whether a given statement will be breakable
// when the code is compiled by the full compiler in the debugger. This means
// that there will be an IC (load/store/call) in the code generated for the
// debugger to piggybag on.
class BreakableStatementChecker: public AstVisitor {
 public:
  explicit BreakableStatementChecker(Zone* zone) : is_breakable_(false) {
    InitializeAstVisitor(zone);
  }

  void Check(Statement* stmt);
  void Check(Expression* stmt);

  bool is_breakable() { return is_breakable_; }

 private:
  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  bool is_breakable_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(BreakableStatementChecker);
};


// -----------------------------------------------------------------------------
// Full code generator.

class FullCodeGenerator: public AstVisitor {
 public:
  enum State {
    NO_REGISTERS,
    TOS_REG
  };

  FullCodeGenerator(MacroAssembler* masm, CompilationInfo* info)
      : masm_(masm),
        info_(info),
        scope_(info->scope()),
        nesting_stack_(NULL),
        loop_depth_(0),
        globals_(NULL),
        context_(NULL),
        bailout_entries_(info->HasDeoptimizationSupport()
                         ? info->function()->ast_node_count() : 0,
                         info->zone()),
        back_edges_(2, info->zone()),
        ic_total_count_(0) {
    Initialize();
  }

  void Initialize();

  static bool MakeCode(CompilationInfo* info);

  // Encode state and pc-offset as a BitField<type, start, size>.
  // Only use 30 bits because we encode the result as a smi.
  class StateField : public BitField<State, 0, 1> { };
  class PcField    : public BitField<unsigned, 1, 30-1> { };

  static const char* State2String(State state) {
    switch (state) {
      case NO_REGISTERS: return "NO_REGISTERS";
      case TOS_REG: return "TOS_REG";
    }
    UNREACHABLE();
    return NULL;
  }

  static const int kMaxBackEdgeWeight = 127;

  // Platform-specific code size multiplier.
#if V8_TARGET_ARCH_IA32
  static const int kCodeSizeMultiplier = 105;
  static const int kBootCodeSizeMultiplier = 100;
#elif V8_TARGET_ARCH_X64
  static const int kCodeSizeMultiplier = 170;
  static const int kBootCodeSizeMultiplier = 140;
#elif V8_TARGET_ARCH_ARM
  static const int kCodeSizeMultiplier = 149;
  static const int kBootCodeSizeMultiplier = 110;
#elif V8_TARGET_ARCH_ARM64
// TODO(all): Copied ARM value. Check this is sensible for ARM64.
  static const int kCodeSizeMultiplier = 149;
  static const int kBootCodeSizeMultiplier = 110;
#elif V8_TARGET_ARCH_MIPS
  static const int kCodeSizeMultiplier = 149;
  static const int kBootCodeSizeMultiplier = 120;
#else
#error Unsupported target architecture.
#endif

 private:
  class Breakable;
  class Iteration;

  class TestContext;

  class NestedStatement BASE_EMBEDDED {
   public:
    explicit NestedStatement(FullCodeGenerator* codegen) : codegen_(codegen) {
      // Link into codegen's nesting stack.
      previous_ = codegen->nesting_stack_;
      codegen->nesting_stack_ = this;
    }
    virtual ~NestedStatement() {
      // Unlink from codegen's nesting stack.
      ASSERT_EQ(this, codegen_->nesting_stack_);
      codegen_->nesting_stack_ = previous_;
    }

    virtual Breakable* AsBreakable() { return NULL; }
    virtual Iteration* AsIteration() { return NULL; }

    virtual bool IsContinueTarget(Statement* target) { return false; }
    virtual bool IsBreakTarget(Statement* target) { return false; }

    // Notify the statement that we are exiting it via break, continue, or
    // return and give it a chance to generate cleanup code.  Return the
    // next outer statement in the nesting stack.  We accumulate in
    // *stack_depth the amount to drop the stack and in *context_length the
    // number of context chain links to unwind as we traverse the nesting
    // stack from an exit to its target.
    virtual NestedStatement* Exit(int* stack_depth, int* context_length) {
      return previous_;
    }

   protected:
    MacroAssembler* masm() { return codegen_->masm(); }

    FullCodeGenerator* codegen_;
    NestedStatement* previous_;

   private:
    DISALLOW_COPY_AND_ASSIGN(NestedStatement);
  };

  // A breakable statement such as a block.
  class Breakable : public NestedStatement {
   public:
    Breakable(FullCodeGenerator* codegen, BreakableStatement* statement)
        : NestedStatement(codegen), statement_(statement) {
    }
    virtual ~Breakable() {}

    virtual Breakable* AsBreakable() { return this; }
    virtual bool IsBreakTarget(Statement* target) {
      return statement() == target;
    }

    BreakableStatement* statement() { return statement_; }
    Label* break_label() { return &break_label_; }

   private:
    BreakableStatement* statement_;
    Label break_label_;
  };

  // An iteration statement such as a while, for, or do loop.
  class Iteration : public Breakable {
   public:
    Iteration(FullCodeGenerator* codegen, IterationStatement* statement)
        : Breakable(codegen, statement) {
    }
    virtual ~Iteration() {}

    virtual Iteration* AsIteration() { return this; }
    virtual bool IsContinueTarget(Statement* target) {
      return statement() == target;
    }

    Label* continue_label() { return &continue_label_; }

   private:
    Label continue_label_;
  };

  // A nested block statement.
  class NestedBlock : public Breakable {
   public:
    NestedBlock(FullCodeGenerator* codegen, Block* block)
        : Breakable(codegen, block) {
    }
    virtual ~NestedBlock() {}

    virtual NestedStatement* Exit(int* stack_depth, int* context_length) {
      if (statement()->AsBlock()->scope() != NULL) {
        ++(*context_length);
      }
      return previous_;
    };
  };

  // The try block of a try/catch statement.
  class TryCatch : public NestedStatement {
   public:
    explicit TryCatch(FullCodeGenerator* codegen) : NestedStatement(codegen) {
    }
    virtual ~TryCatch() {}

    virtual NestedStatement* Exit(int* stack_depth, int* context_length);
  };

  // The try block of a try/finally statement.
  class TryFinally : public NestedStatement {
   public:
    TryFinally(FullCodeGenerator* codegen, Label* finally_entry)
        : NestedStatement(codegen), finally_entry_(finally_entry) {
    }
    virtual ~TryFinally() {}

    virtual NestedStatement* Exit(int* stack_depth, int* context_length);

   private:
    Label* finally_entry_;
  };

  // The finally block of a try/finally statement.
  class Finally : public NestedStatement {
   public:
    static const int kElementCount = 5;

    explicit Finally(FullCodeGenerator* codegen) : NestedStatement(codegen) { }
    virtual ~Finally() {}

    virtual NestedStatement* Exit(int* stack_depth, int* context_length) {
      *stack_depth += kElementCount;
      return previous_;
    }
  };

  // The body of a for/in loop.
  class ForIn : public Iteration {
   public:
    static const int kElementCount = 5;

    ForIn(FullCodeGenerator* codegen, ForInStatement* statement)
        : Iteration(codegen, statement) {
    }
    virtual ~ForIn() {}

    virtual NestedStatement* Exit(int* stack_depth, int* context_length) {
      *stack_depth += kElementCount;
      return previous_;
    }
  };


  // The body of a with or catch.
  class WithOrCatch : public NestedStatement {
   public:
    explicit WithOrCatch(FullCodeGenerator* codegen)
        : NestedStatement(codegen) {
    }
    virtual ~WithOrCatch() {}

    virtual NestedStatement* Exit(int* stack_depth, int* context_length) {
      ++(*context_length);
      return previous_;
    }
  };

  // Type of a member function that generates inline code for a native function.
  typedef void (FullCodeGenerator::*InlineFunctionGenerator)(CallRuntime* expr);

  static const InlineFunctionGenerator kInlineFunctionGenerators[];

  // A platform-specific utility to overwrite the accumulator register
  // with a GC-safe value.
  void ClearAccumulator();

  // Determine whether or not to inline the smi case for the given
  // operation.
  bool ShouldInlineSmiCase(Token::Value op);

  // Helper function to convert a pure value into a test context.  The value
  // is expected on the stack or the accumulator, depending on the platform.
  // See the platform-specific implementation for details.
  void DoTest(Expression* condition,
              Label* if_true,
              Label* if_false,
              Label* fall_through);
  void DoTest(const TestContext* context);

  // Helper function to split control flow and avoid a branch to the
  // fall-through label if it is set up.
#if V8_TARGET_ARCH_MIPS
  void Split(Condition cc,
             Register lhs,
             const Operand&  rhs,
             Label* if_true,
             Label* if_false,
             Label* fall_through);
#else  // All non-mips arch.
  void Split(Condition cc,
             Label* if_true,
             Label* if_false,
             Label* fall_through);
#endif  // V8_TARGET_ARCH_MIPS

  // Load the value of a known (PARAMETER, LOCAL, or CONTEXT) variable into
  // a register.  Emits a context chain walk if if necessary (so does
  // SetVar) so avoid calling both on the same variable.
  void GetVar(Register destination, Variable* var);

  // Assign to a known (PARAMETER, LOCAL, or CONTEXT) variable.  If it's in
  // the context, the write barrier will be emitted and source, scratch0,
  // scratch1 will be clobbered.  Emits a context chain walk if if necessary
  // (so does GetVar) so avoid calling both on the same variable.
  void SetVar(Variable* var,
              Register source,
              Register scratch0,
              Register scratch1);

  // An operand used to read/write a stack-allocated (PARAMETER or LOCAL)
  // variable.  Writing does not need the write barrier.
  MemOperand StackOperand(Variable* var);

  // An operand used to read/write a known (PARAMETER, LOCAL, or CONTEXT)
  // variable.  May emit code to traverse the context chain, loading the
  // found context into the scratch register.  Writing to this operand will
  // need the write barrier if location is CONTEXT.
  MemOperand VarOperand(Variable* var, Register scratch);

  void VisitForEffect(Expression* expr) {
    EffectContext context(this);
    Visit(expr);
    PrepareForBailout(expr, NO_REGISTERS);
  }

  void VisitForAccumulatorValue(Expression* expr) {
    AccumulatorValueContext context(this);
    Visit(expr);
    PrepareForBailout(expr, TOS_REG);
  }

  void VisitForStackValue(Expression* expr) {
    StackValueContext context(this);
    Visit(expr);
    PrepareForBailout(expr, NO_REGISTERS);
  }

  void VisitForControl(Expression* expr,
                       Label* if_true,
                       Label* if_false,
                       Label* fall_through) {
    TestContext context(this, expr, if_true, if_false, fall_through);
    Visit(expr);
    // For test contexts, we prepare for bailout before branching, not at
    // the end of the entire expression.  This happens as part of visiting
    // the expression.
  }

  void VisitInDuplicateContext(Expression* expr);

  void VisitDeclarations(ZoneList<Declaration*>* declarations);
  void DeclareModules(Handle<FixedArray> descriptions);
  void DeclareGlobals(Handle<FixedArray> pairs);
  int DeclareGlobalsFlags();

  // Generate code to allocate all (including nested) modules and contexts.
  // Because of recursive linking and the presence of module alias declarations,
  // this has to be a separate pass _before_ populating or executing any module.
  void AllocateModules(ZoneList<Declaration*>* declarations);

  // Generate code to create an iterator result object.  The "value" property is
  // set to a value popped from the stack, and "done" is set according to the
  // argument.  The result object is left in the result register.
  void EmitCreateIteratorResult(bool done);

  // Try to perform a comparison as a fast inlined literal compare if
  // the operands allow it.  Returns true if the compare operations
  // has been matched and all code generated; false otherwise.
  bool TryLiteralCompare(CompareOperation* compare);

  // Platform-specific code for comparing the type of a value with
  // a given literal string.
  void EmitLiteralCompareTypeof(Expression* expr,
                                Expression* sub_expr,
                                Handle<String> check);

  // Platform-specific code for equality comparison with a nil-like value.
  void EmitLiteralCompareNil(CompareOperation* expr,
                             Expression* sub_expr,
                             NilValue nil);

  // Bailout support.
  void PrepareForBailout(Expression* node, State state);
  void PrepareForBailoutForId(BailoutId id, State state);

  // Feedback slot support. The feedback vector will be cleared during gc and
  // collected by the type-feedback oracle.
  Handle<FixedArray> FeedbackVector() {
    return info_->feedback_vector();
  }
  void EnsureSlotContainsAllocationSite(int slot);

  // Record a call's return site offset, used to rebuild the frame if the
  // called function was inlined at the site.
  void RecordJSReturnSite(Call* call);

  // Prepare for bailout before a test (or compare) and branch.  If
  // should_normalize, then the following comparison will not handle the
  // canonical JS true value so we will insert a (dead) test against true at
  // the actual bailout target from the optimized code. If not
  // should_normalize, the true and false labels are ignored.
  void PrepareForBailoutBeforeSplit(Expression* expr,
                                    bool should_normalize,
                                    Label* if_true,
                                    Label* if_false);

  // If enabled, emit debug code for checking that the current context is
  // neither a with nor a catch context.
  void EmitDebugCheckDeclarationContext(Variable* variable);

  // This is meant to be called at loop back edges, |back_edge_target| is
  // the jump target of the back edge and is used to approximate the amount
  // of code inside the loop.
  void EmitBackEdgeBookkeeping(IterationStatement* stmt,
                               Label* back_edge_target);
  // Record the OSR AST id corresponding to a back edge in the code.
  void RecordBackEdge(BailoutId osr_ast_id);
  // Emit a table of back edge ids, pcs and loop depths into the code stream.
  // Return the offset of the start of the table.
  unsigned EmitBackEdgeTable();

  void EmitProfilingCounterDecrement(int delta);
  void EmitProfilingCounterReset();

  // Emit code to pop values from the stack associated with nested statements
  // like try/catch, try/finally, etc, running the finallies and unwinding the
  // handlers as needed.
  void EmitUnwindBeforeReturn();

  // Platform-specific return sequence
  void EmitReturnSequence();

  // Platform-specific code sequences for calls
  void EmitCall(Call* expr, CallIC::CallType = CallIC::FUNCTION);
  void EmitCallWithLoadIC(Call* expr);
  void EmitKeyedCallWithLoadIC(Call* expr, Expression* key);

  // Platform-specific code for inline runtime calls.
  InlineFunctionGenerator FindInlineFunctionGenerator(Runtime::FunctionId id);

  void EmitInlineRuntimeCall(CallRuntime* expr);

#define EMIT_INLINE_RUNTIME_CALL(name, x, y) \
  void Emit##name(CallRuntime* expr);
  INLINE_FUNCTION_LIST(EMIT_INLINE_RUNTIME_CALL)
#undef EMIT_INLINE_RUNTIME_CALL

  // Platform-specific code for resuming generators.
  void EmitGeneratorResume(Expression *generator,
                           Expression *value,
                           JSGeneratorObject::ResumeMode resume_mode);

  // Platform-specific code for loading variables.
  void EmitLoadGlobalCheckExtensions(Variable* var,
                                     TypeofState typeof_state,
                                     Label* slow);
  MemOperand ContextSlotOperandCheckExtensions(Variable* var, Label* slow);
  void EmitDynamicLookupFastCase(Variable* var,
                                 TypeofState typeof_state,
                                 Label* slow,
                                 Label* done);
  void EmitVariableLoad(VariableProxy* proxy);

  void EmitAccessor(Expression* expression);

  // Expects the arguments and the function already pushed.
  void EmitResolvePossiblyDirectEval(int arg_count);

  // Platform-specific support for allocating a new closure based on
  // the given function info.
  void EmitNewClosure(Handle<SharedFunctionInfo> info, bool pretenure);

  // Platform-specific support for compiling assignments.

  // Load a value from a named property.
  // The receiver is left on the stack by the IC.
  void EmitNamedPropertyLoad(Property* expr);

  // Load a value from a keyed property.
  // The receiver and the key is left on the stack by the IC.
  void EmitKeyedPropertyLoad(Property* expr);

  // Apply the compound assignment operator. Expects the left operand on top
  // of the stack and the right one in the accumulator.
  void EmitBinaryOp(BinaryOperation* expr,
                    Token::Value op,
                    OverwriteMode mode);

  // Helper functions for generating inlined smi code for certain
  // binary operations.
  void EmitInlineSmiBinaryOp(BinaryOperation* expr,
                             Token::Value op,
                             OverwriteMode mode,
                             Expression* left,
                             Expression* right);

  // Assign to the given expression as if via '='. The right-hand-side value
  // is expected in the accumulator.
  void EmitAssignment(Expression* expr);

  // Complete a variable assignment.  The right-hand-side value is expected
  // in the accumulator.
  void EmitVariableAssignment(Variable* var,
                              Token::Value op);

  // Helper functions to EmitVariableAssignment
  void EmitStoreToStackLocalOrContextSlot(Variable* var,
                                          MemOperand location);
  void EmitCallStoreContextSlot(Handle<String> name, StrictMode strict_mode);

  // Complete a named property assignment.  The receiver is expected on top
  // of the stack and the right-hand-side value in the accumulator.
  void EmitNamedPropertyAssignment(Assignment* expr);

  // Complete a keyed property assignment.  The receiver and key are
  // expected on top of the stack and the right-hand-side value in the
  // accumulator.
  void EmitKeyedPropertyAssignment(Assignment* expr);

  void CallIC(Handle<Code> code,
              TypeFeedbackId id = TypeFeedbackId::None());

  void CallLoadIC(ContextualMode mode,
                  TypeFeedbackId id = TypeFeedbackId::None());
  void CallStoreIC(TypeFeedbackId id = TypeFeedbackId::None());

  void SetFunctionPosition(FunctionLiteral* fun);
  void SetReturnPosition(FunctionLiteral* fun);
  void SetStatementPosition(Statement* stmt);
  void SetExpressionPosition(Expression* expr);
  void SetStatementPosition(int pos);
  void SetSourcePosition(int pos);

  // Non-local control flow support.
  void EnterFinallyBlock();
  void ExitFinallyBlock();

  // Loop nesting counter.
  int loop_depth() { return loop_depth_; }
  void increment_loop_depth() { loop_depth_++; }
  void decrement_loop_depth() {
    ASSERT(loop_depth_ > 0);
    loop_depth_--;
  }

  MacroAssembler* masm() { return masm_; }

  class ExpressionContext;
  const ExpressionContext* context() { return context_; }
  void set_new_context(const ExpressionContext* context) { context_ = context; }

  Handle<Script> script() { return info_->script(); }
  bool is_eval() { return info_->is_eval(); }
  bool is_native() { return info_->is_native(); }
  StrictMode strict_mode() { return function()->strict_mode(); }
  FunctionLiteral* function() { return info_->function(); }
  Scope* scope() { return scope_; }

  static Register result_register();
  static Register context_register();

  // Set fields in the stack frame. Offsets are the frame pointer relative
  // offsets defined in, e.g., StandardFrameConstants.
  void StoreToFrameField(int frame_offset, Register value);

  // Load a value from the current context. Indices are defined as an enum
  // in v8::internal::Context.
  void LoadContextField(Register dst, int context_index);

  // Push the function argument for the runtime functions PushWithContext
  // and PushCatchContext.
  void PushFunctionArgumentForContextAllocation();

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  void VisitComma(BinaryOperation* expr);
  void VisitLogicalExpression(BinaryOperation* expr);
  void VisitArithmeticExpression(BinaryOperation* expr);

  void VisitForTypeofValue(Expression* expr);

  void Generate();
  void PopulateDeoptimizationData(Handle<Code> code);
  void PopulateTypeFeedbackInfo(Handle<Code> code);

  Handle<FixedArray> handler_table() { return handler_table_; }

  struct BailoutEntry {
    BailoutId id;
    unsigned pc_and_state;
  };

  struct BackEdgeEntry {
    BailoutId id;
    unsigned pc;
    uint32_t loop_depth;
  };

  class ExpressionContext BASE_EMBEDDED {
   public:
    explicit ExpressionContext(FullCodeGenerator* codegen)
        : masm_(codegen->masm()), old_(codegen->context()), codegen_(codegen) {
      codegen->set_new_context(this);
    }

    virtual ~ExpressionContext() {
      codegen_->set_new_context(old_);
    }

    Isolate* isolate() const { return codegen_->isolate(); }

    // Convert constant control flow (true or false) to the result expected for
    // this expression context.
    virtual void Plug(bool flag) const = 0;

    // Emit code to convert a pure value (in a register, known variable
    // location, as a literal, or on top of the stack) into the result
    // expected according to this expression context.
    virtual void Plug(Register reg) const = 0;
    virtual void Plug(Variable* var) const = 0;
    virtual void Plug(Handle<Object> lit) const = 0;
    virtual void Plug(Heap::RootListIndex index) const = 0;
    virtual void PlugTOS() const = 0;

    // Emit code to convert pure control flow to a pair of unbound labels into
    // the result expected according to this expression context.  The
    // implementation will bind both labels unless it's a TestContext, which
    // won't bind them at this point.
    virtual void Plug(Label* materialize_true,
                      Label* materialize_false) const = 0;

    // Emit code to discard count elements from the top of stack, then convert
    // a pure value into the result expected according to this expression
    // context.
    virtual void DropAndPlug(int count, Register reg) const = 0;

    // Set up branch labels for a test expression.  The three Label** parameters
    // are output parameters.
    virtual void PrepareTest(Label* materialize_true,
                             Label* materialize_false,
                             Label** if_true,
                             Label** if_false,
                             Label** fall_through) const = 0;

    // Returns true if we are evaluating only for side effects (i.e. if the
    // result will be discarded).
    virtual bool IsEffect() const { return false; }

    // Returns true if we are evaluating for the value (in accu/on stack).
    virtual bool IsAccumulatorValue() const { return false; }
    virtual bool IsStackValue() const { return false; }

    // Returns true if we are branching on the value rather than materializing
    // it.  Only used for asserts.
    virtual bool IsTest() const { return false; }

   protected:
    FullCodeGenerator* codegen() const { return codegen_; }
    MacroAssembler* masm() const { return masm_; }
    MacroAssembler* masm_;

   private:
    const ExpressionContext* old_;
    FullCodeGenerator* codegen_;
  };

  class AccumulatorValueContext : public ExpressionContext {
   public:
    explicit AccumulatorValueContext(FullCodeGenerator* codegen)
        : ExpressionContext(codegen) { }

    virtual void Plug(bool flag) const;
    virtual void Plug(Register reg) const;
    virtual void Plug(Label* materialize_true, Label* materialize_false) const;
    virtual void Plug(Variable* var) const;
    virtual void Plug(Handle<Object> lit) const;
    virtual void Plug(Heap::RootListIndex) const;
    virtual void PlugTOS() const;
    virtual void DropAndPlug(int count, Register reg) const;
    virtual void PrepareTest(Label* materialize_true,
                             Label* materialize_false,
                             Label** if_true,
                             Label** if_false,
                             Label** fall_through) const;
    virtual bool IsAccumulatorValue() const { return true; }
  };

  class StackValueContext : public ExpressionContext {
   public:
    explicit StackValueContext(FullCodeGenerator* codegen)
        : ExpressionContext(codegen) { }

    virtual void Plug(bool flag) const;
    virtual void Plug(Register reg) const;
    virtual void Plug(Label* materialize_true, Label* materialize_false) const;
    virtual void Plug(Variable* var) const;
    virtual void Plug(Handle<Object> lit) const;
    virtual void Plug(Heap::RootListIndex) const;
    virtual void PlugTOS() const;
    virtual void DropAndPlug(int count, Register reg) const;
    virtual void PrepareTest(Label* materialize_true,
                             Label* materialize_false,
                             Label** if_true,
                             Label** if_false,
                             Label** fall_through) const;
    virtual bool IsStackValue() const { return true; }
  };

  class TestContext : public ExpressionContext {
   public:
    TestContext(FullCodeGenerator* codegen,
                Expression* condition,
                Label* true_label,
                Label* false_label,
                Label* fall_through)
        : ExpressionContext(codegen),
          condition_(condition),
          true_label_(true_label),
          false_label_(false_label),
          fall_through_(fall_through) { }

    static const TestContext* cast(const ExpressionContext* context) {
      ASSERT(context->IsTest());
      return reinterpret_cast<const TestContext*>(context);
    }

    Expression* condition() const { return condition_; }
    Label* true_label() const { return true_label_; }
    Label* false_label() const { return false_label_; }
    Label* fall_through() const { return fall_through_; }

    virtual void Plug(bool flag) const;
    virtual void Plug(Register reg) const;
    virtual void Plug(Label* materialize_true, Label* materialize_false) const;
    virtual void Plug(Variable* var) const;
    virtual void Plug(Handle<Object> lit) const;
    virtual void Plug(Heap::RootListIndex) const;
    virtual void PlugTOS() const;
    virtual void DropAndPlug(int count, Register reg) const;
    virtual void PrepareTest(Label* materialize_true,
                             Label* materialize_false,
                             Label** if_true,
                             Label** if_false,
                             Label** fall_through) const;
    virtual bool IsTest() const { return true; }

   private:
    Expression* condition_;
    Label* true_label_;
    Label* false_label_;
    Label* fall_through_;
  };

  class EffectContext : public ExpressionContext {
   public:
    explicit EffectContext(FullCodeGenerator* codegen)
        : ExpressionContext(codegen) { }

    virtual void Plug(bool flag) const;
    virtual void Plug(Register reg) const;
    virtual void Plug(Label* materialize_true, Label* materialize_false) const;
    virtual void Plug(Variable* var) const;
    virtual void Plug(Handle<Object> lit) const;
    virtual void Plug(Heap::RootListIndex) const;
    virtual void PlugTOS() const;
    virtual void DropAndPlug(int count, Register reg) const;
    virtual void PrepareTest(Label* materialize_true,
                             Label* materialize_false,
                             Label** if_true,
                             Label** if_false,
                             Label** fall_through) const;
    virtual bool IsEffect() const { return true; }
  };

  MacroAssembler* masm_;
  CompilationInfo* info_;
  Scope* scope_;
  Label return_label_;
  NestedStatement* nesting_stack_;
  int loop_depth_;
  ZoneList<Handle<Object> >* globals_;
  Handle<FixedArray> modules_;
  int module_index_;
  const ExpressionContext* context_;
  ZoneList<BailoutEntry> bailout_entries_;
  GrowableBitVector prepared_bailout_ids_;
  ZoneList<BackEdgeEntry> back_edges_;
  int ic_total_count_;
  Handle<FixedArray> handler_table_;
  Handle<Cell> profiling_counter_;
  bool generate_debug_code_;

  friend class NestedStatement;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(FullCodeGenerator);
};


// A map from property names to getter/setter pairs allocated in the zone.
class AccessorTable: public TemplateHashMap<Literal,
                                            ObjectLiteral::Accessors,
                                            ZoneAllocationPolicy> {
 public:
  explicit AccessorTable(Zone* zone) :
      TemplateHashMap<Literal, ObjectLiteral::Accessors,
                      ZoneAllocationPolicy>(Literal::Match,
                                            ZoneAllocationPolicy(zone)),
      zone_(zone) { }

  Iterator lookup(Literal* literal) {
    Iterator it = find(literal, true, ZoneAllocationPolicy(zone_));
    if (it->second == NULL) it->second = new(zone_) ObjectLiteral::Accessors();
    return it;
  }

 private:
  Zone* zone_;
};


class BackEdgeTable {
 public:
  BackEdgeTable(Code* code, DisallowHeapAllocation* required) {
    ASSERT(code->kind() == Code::FUNCTION);
    instruction_start_ = code->instruction_start();
    Address table_address = instruction_start_ + code->back_edge_table_offset();
    length_ = Memory::uint32_at(table_address);
    start_ = table_address + kTableLengthSize;
  }

  uint32_t length() { return length_; }

  BailoutId ast_id(uint32_t index) {
    return BailoutId(static_cast<int>(
        Memory::uint32_at(entry_at(index) + kAstIdOffset)));
  }

  uint32_t loop_depth(uint32_t index) {
    return Memory::uint32_at(entry_at(index) + kLoopDepthOffset);
  }

  uint32_t pc_offset(uint32_t index) {
    return Memory::uint32_at(entry_at(index) + kPcOffsetOffset);
  }

  Address pc(uint32_t index) {
    return instruction_start_ + pc_offset(index);
  }

  enum BackEdgeState {
    INTERRUPT,
    ON_STACK_REPLACEMENT,
    OSR_AFTER_STACK_CHECK
  };

  // Patch all interrupts with allowed loop depth in the unoptimized code to
  // unconditionally call replacement_code.
  static void Patch(Isolate* isolate,
                    Code* unoptimized_code);

  // Patch the back edge to the target state, provided the correct callee.
  static void PatchAt(Code* unoptimized_code,
                      Address pc,
                      BackEdgeState target_state,
                      Code* replacement_code);

  // Change all patched back edges back to normal interrupts.
  static void Revert(Isolate* isolate,
                     Code* unoptimized_code);

  // Change a back edge patched for on-stack replacement to perform a
  // stack check first.
  static void AddStackCheck(Handle<Code> code, uint32_t pc_offset);

  // Revert the patch by AddStackCheck.
  static void RemoveStackCheck(Handle<Code> code, uint32_t pc_offset);

  // Return the current patch state of the back edge.
  static BackEdgeState GetBackEdgeState(Isolate* isolate,
                                        Code* unoptimized_code,
                                        Address pc_after);

#ifdef DEBUG
  // Verify that all back edges of a certain loop depth are patched.
  static bool Verify(Isolate* isolate,
                     Code* unoptimized_code,
                     int loop_nesting_level);
#endif  // DEBUG

 private:
  Address entry_at(uint32_t index) {
    ASSERT(index < length_);
    return start_ + index * kEntrySize;
  }

  static const int kTableLengthSize = kIntSize;
  static const int kAstIdOffset = 0 * kIntSize;
  static const int kPcOffsetOffset = 1 * kIntSize;
  static const int kLoopDepthOffset = 2 * kIntSize;
  static const int kEntrySize = 3 * kIntSize;

  Address start_;
  Address instruction_start_;
  uint32_t length_;
};


} }  // namespace v8::internal

#endif  // V8_FULL_CODEGEN_H_
