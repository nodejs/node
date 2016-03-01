// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FULL_CODEGEN_FULL_CODEGEN_H_
#define V8_FULL_CODEGEN_FULL_CODEGEN_H_

#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/bit-vector.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/globals.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Forward declarations.
class JumpPatchSite;

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
        isolate_(info->isolate()),
        zone_(info->zone()),
        scope_(info->scope()),
        nesting_stack_(NULL),
        loop_depth_(0),
        try_catch_depth_(0),
        globals_(NULL),
        context_(NULL),
        bailout_entries_(info->HasDeoptimizationSupport()
                             ? info->literal()->ast_node_count()
                             : 0,
                         info->zone()),
        back_edges_(2, info->zone()),
        handler_table_(info->zone()),
        ic_total_count_(0) {
    DCHECK(!info->IsStub());
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
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X87
  static const int kCodeSizeMultiplier = 105;
#elif V8_TARGET_ARCH_X64
  static const int kCodeSizeMultiplier = 165;
#elif V8_TARGET_ARCH_ARM
  static const int kCodeSizeMultiplier = 149;
#elif V8_TARGET_ARCH_ARM64
  static const int kCodeSizeMultiplier = 220;
#elif V8_TARGET_ARCH_PPC64
  static const int kCodeSizeMultiplier = 200;
#elif V8_TARGET_ARCH_PPC
  static const int kCodeSizeMultiplier = 200;
#elif V8_TARGET_ARCH_MIPS
  static const int kCodeSizeMultiplier = 149;
#elif V8_TARGET_ARCH_MIPS64
  static const int kCodeSizeMultiplier = 149;
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
      DCHECK_EQ(this, codegen_->nesting_stack_);
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

    // Like the Exit() method above, but limited to accumulating stack depth.
    virtual NestedStatement* AccumulateDepth(int* stack_depth) {
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

    Breakable* AsBreakable() override { return this; }
    bool IsBreakTarget(Statement* target) override {
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

    Iteration* AsIteration() override { return this; }
    bool IsContinueTarget(Statement* target) override {
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

    NestedStatement* Exit(int* stack_depth, int* context_length) override {
      auto block_scope = statement()->AsBlock()->scope();
      if (block_scope != nullptr) {
        if (block_scope->ContextLocalCount() > 0) ++(*context_length);
      }
      return previous_;
    }
  };

  // The try block of a try/catch statement.
  class TryCatch : public NestedStatement {
   public:
    static const int kElementCount = TryBlockConstant::kElementCount;

    explicit TryCatch(FullCodeGenerator* codegen) : NestedStatement(codegen) {}

    NestedStatement* Exit(int* stack_depth, int* context_length) override {
      *stack_depth += kElementCount;
      return previous_;
    }
    NestedStatement* AccumulateDepth(int* stack_depth) override {
      *stack_depth += kElementCount;
      return previous_;
    }
  };

  // The try block of a try/finally statement.
  class TryFinally : public NestedStatement {
   public:
    static const int kElementCount = TryBlockConstant::kElementCount;

    TryFinally(FullCodeGenerator* codegen, Label* finally_entry)
        : NestedStatement(codegen), finally_entry_(finally_entry) {
    }

    NestedStatement* Exit(int* stack_depth, int* context_length) override;
    NestedStatement* AccumulateDepth(int* stack_depth) override {
      *stack_depth += kElementCount;
      return previous_;
    }

   private:
    Label* finally_entry_;
  };

  // The finally block of a try/finally statement.
  class Finally : public NestedStatement {
   public:
    static const int kElementCount = 3;

    explicit Finally(FullCodeGenerator* codegen) : NestedStatement(codegen) {}

    NestedStatement* Exit(int* stack_depth, int* context_length) override {
      *stack_depth += kElementCount;
      return previous_;
    }
    NestedStatement* AccumulateDepth(int* stack_depth) override {
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

    NestedStatement* Exit(int* stack_depth, int* context_length) override {
      *stack_depth += kElementCount;
      return previous_;
    }
    NestedStatement* AccumulateDepth(int* stack_depth) override {
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

    NestedStatement* Exit(int* stack_depth, int* context_length) override {
      ++(*context_length);
      return previous_;
    }
  };

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
#elif V8_TARGET_ARCH_MIPS64
  void Split(Condition cc,
             Register lhs,
             const Operand&  rhs,
             Label* if_true,
             Label* if_false,
             Label* fall_through);
#elif V8_TARGET_ARCH_PPC
  void Split(Condition cc, Label* if_true, Label* if_false, Label* fall_through,
             CRegister cr = cr7);
#else  // All other arch.
  void Split(Condition cc,
             Label* if_true,
             Label* if_false,
             Label* fall_through);
#endif

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

  void VisitDeclarations(ZoneList<Declaration*>* declarations) override;
  void DeclareModules(Handle<FixedArray> descriptions);
  void DeclareGlobals(Handle<FixedArray> pairs);
  int DeclareGlobalsFlags();

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

  // Returns a smi for the index into the FixedArray that backs the feedback
  // vector
  Smi* SmiFromSlot(FeedbackVectorSlot slot) const {
    return Smi::FromInt(TypeFeedbackVector::GetIndexFromSpec(
        literal()->feedback_vector_spec(), slot));
  }

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
  void EmitCall(Call* expr, ConvertReceiverMode = ConvertReceiverMode::kAny);
  void EmitSuperConstructorCall(Call* expr);
  void EmitCallWithLoadIC(Call* expr);
  void EmitSuperCallWithLoadIC(Call* expr);
  void EmitKeyedCallWithLoadIC(Call* expr, Expression* key);
  void EmitKeyedSuperCallWithLoadIC(Call* expr);
  void EmitPossiblyEvalCall(Call* expr);

#define FOR_EACH_FULL_CODE_INTRINSIC(F) \
  F(IsSmi)                              \
  F(IsArray)                            \
  F(IsTypedArray)                       \
  F(IsRegExp)                           \
  F(IsJSProxy)                          \
  F(Call)                               \
  F(ArgumentsLength)                    \
  F(Arguments)                          \
  F(ValueOf)                            \
  F(SetValueOf)                         \
  F(IsDate)                             \
  F(StringCharFromCode)                 \
  F(StringCharAt)                       \
  F(OneByteSeqStringSetChar)            \
  F(TwoByteSeqStringSetChar)            \
  F(ObjectEquals)                       \
  F(IsFunction)                         \
  F(IsJSReceiver)                       \
  F(IsSimdValue)                        \
  F(MathPow)                            \
  F(IsMinusZero)                        \
  F(HasCachedArrayIndex)                \
  F(GetCachedArrayIndex)                \
  F(GetSuperConstructor)                \
  F(FastOneByteArrayJoin)               \
  F(GeneratorNext)                      \
  F(GeneratorThrow)                     \
  F(DebugBreakInOptimizedCode)          \
  F(ClassOf)                            \
  F(StringCharCodeAt)                   \
  F(SubString)                          \
  F(RegExpExec)                         \
  F(RegExpConstructResult)              \
  F(ToInteger)                          \
  F(NumberToString)                     \
  F(ToString)                           \
  F(ToLength)                           \
  F(ToNumber)                           \
  F(ToName)                             \
  F(ToObject)                           \
  F(DebugIsActive)                      \
  F(CreateIterResultObject)

#define GENERATOR_DECLARATION(Name) void Emit##Name(CallRuntime* call);
  FOR_EACH_FULL_CODE_INTRINSIC(GENERATOR_DECLARATION)
#undef GENERATOR_DECLARATION

  void EmitIntrinsicAsStubCall(CallRuntime* expr, const Callable& callable);

  // Platform-specific code for resuming generators.
  void EmitGeneratorResume(Expression *generator,
                           Expression *value,
                           JSGeneratorObject::ResumeMode resume_mode);

  // Platform-specific code for loading variables.
  void EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                     TypeofMode typeof_mode, Label* slow);
  MemOperand ContextSlotOperandCheckExtensions(Variable* var, Label* slow);
  void EmitDynamicLookupFastCase(VariableProxy* proxy, TypeofMode typeof_mode,
                                 Label* slow, Label* done);
  void EmitGlobalVariableLoad(VariableProxy* proxy, TypeofMode typeof_mode);
  void EmitVariableLoad(VariableProxy* proxy,
                        TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);

  void EmitAccessor(ObjectLiteralProperty* property);

  bool NeedsHoleCheckForLoad(VariableProxy* proxy);

  // Expects the arguments and the function already pushed.
  void EmitResolvePossiblyDirectEval(int arg_count);

  // Platform-specific support for allocating a new closure based on
  // the given function info.
  void EmitNewClosure(Handle<SharedFunctionInfo> info, bool pretenure);

  // Re-usable portions of CallRuntime
  void EmitLoadJSRuntimeFunction(CallRuntime* expr);
  void EmitCallJSRuntimeFunction(CallRuntime* expr);

  // Load a value from a named property.
  // The receiver is left on the stack by the IC.
  void EmitNamedPropertyLoad(Property* expr);

  // Load a value from super.named property.
  // Expect receiver ('this' value) and home_object on the stack.
  void EmitNamedSuperPropertyLoad(Property* expr);

  // Load a value from super[keyed] property.
  // Expect receiver ('this' value), home_object and key on the stack.
  void EmitKeyedSuperPropertyLoad(Property* expr);

  // Load a value from a keyed property.
  // The receiver and the key is left on the stack by the IC.
  void EmitKeyedPropertyLoad(Property* expr);

  // Adds the properties to the class (function) object and to its prototype.
  // Expects the class (function) in the accumulator. The class (function) is
  // in the accumulator after installing all the properties.
  void EmitClassDefineProperties(ClassLiteral* lit);

  // Pushes the property key as a Name on the stack.
  void EmitPropertyKey(ObjectLiteralProperty* property, BailoutId bailout_id);

  // Apply the compound assignment operator. Expects the left operand on top
  // of the stack and the right one in the accumulator.
  void EmitBinaryOp(BinaryOperation* expr, Token::Value op);

  // Helper functions for generating inlined smi code for certain
  // binary operations.
  void EmitInlineSmiBinaryOp(BinaryOperation* expr,
                             Token::Value op,
                             Expression* left,
                             Expression* right);

  // Assign to the given expression as if via '='. The right-hand-side value
  // is expected in the accumulator. slot is only used if FLAG_vector_stores
  // is true.
  void EmitAssignment(Expression* expr, FeedbackVectorSlot slot);

  // Complete a variable assignment.  The right-hand-side value is expected
  // in the accumulator.
  void EmitVariableAssignment(Variable* var, Token::Value op,
                              FeedbackVectorSlot slot);

  // Helper functions to EmitVariableAssignment
  void EmitStoreToStackLocalOrContextSlot(Variable* var,
                                          MemOperand location);

  // Complete a named property assignment.  The receiver is expected on top
  // of the stack and the right-hand-side value in the accumulator.
  void EmitNamedPropertyAssignment(Assignment* expr);

  // Complete a super named property assignment. The right-hand-side value
  // is expected in accumulator.
  void EmitNamedSuperPropertyStore(Property* prop);

  // Complete a super named property assignment. The right-hand-side value
  // is expected in accumulator.
  void EmitKeyedSuperPropertyStore(Property* prop);

  // Complete a keyed property assignment.  The receiver and key are
  // expected on top of the stack and the right-hand-side value in the
  // accumulator.
  void EmitKeyedPropertyAssignment(Assignment* expr);

  static bool NeedsHomeObject(Expression* expr) {
    return FunctionLiteral::NeedsHomeObject(expr);
  }

  // Adds the [[HomeObject]] to |initializer| if it is a FunctionLiteral.
  // The value of the initializer is expected to be at the top of the stack.
  // |offset| is the offset in the stack where the home object can be found.
  void EmitSetHomeObject(Expression* initializer, int offset,
                         FeedbackVectorSlot slot);

  void EmitSetHomeObjectAccumulator(Expression* initializer, int offset,
                                    FeedbackVectorSlot slot);

  void CallIC(Handle<Code> code,
              TypeFeedbackId id = TypeFeedbackId::None());

  // Inside typeof reference errors are never thrown.
  void CallLoadIC(TypeofMode typeof_mode, LanguageMode language_mode = SLOPPY,
                  TypeFeedbackId id = TypeFeedbackId::None());
  void CallStoreIC(TypeFeedbackId id = TypeFeedbackId::None());

  void SetFunctionPosition(FunctionLiteral* fun);
  void SetReturnPosition(FunctionLiteral* fun);

  enum InsertBreak { INSERT_BREAK, SKIP_BREAK };

  // During stepping we want to be able to break at each statement, but not at
  // every (sub-)expression. That is why by default we insert breaks at every
  // statement position, but not at every expression position, unless stated
  // otherwise.
  void SetStatementPosition(Statement* stmt,
                            InsertBreak insert_break = INSERT_BREAK);
  void SetExpressionPosition(Expression* expr,
                             InsertBreak insert_break = SKIP_BREAK);

  // Consider an expression a statement. As such, we also insert a break.
  // This is used in loop headers where we want to break for each iteration.
  void SetExpressionAsStatementPosition(Expression* expr);

  void SetCallPosition(Expression* expr);

  void SetConstructCallPosition(Expression* expr) {
    // Currently call and construct calls are treated the same wrt debugging.
    SetCallPosition(expr);
  }

  // Non-local control flow support.
  void EnterTryBlock(int handler_index, Label* handler);
  void ExitTryBlock(int handler_index);
  void EnterFinallyBlock();
  void ExitFinallyBlock();
  void ClearPendingMessage();

  // Loop nesting counter.
  int loop_depth() { return loop_depth_; }
  void increment_loop_depth() { loop_depth_++; }
  void decrement_loop_depth() {
    DCHECK(loop_depth_ > 0);
    loop_depth_--;
  }

  MacroAssembler* masm() const { return masm_; }

  class ExpressionContext;
  const ExpressionContext* context() { return context_; }
  void set_new_context(const ExpressionContext* context) { context_ = context; }

  Isolate* isolate() const { return isolate_; }
  Zone* zone() const { return zone_; }
  Handle<Script> script() { return info_->script(); }
  bool is_eval() { return info_->is_eval(); }
  bool is_native() { return info_->is_native(); }
  LanguageMode language_mode() { return literal()->language_mode(); }
  bool has_simple_parameters() { return info_->has_simple_parameters(); }
  FunctionLiteral* literal() const { return info_->literal(); }
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

  void PushCalleeAndWithBaseObject(Call* expr);

  // AST node visit functions.
#define DECLARE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  void VisitComma(BinaryOperation* expr);
  void VisitLogicalExpression(BinaryOperation* expr);
  void VisitArithmeticExpression(BinaryOperation* expr);

  void VisitForTypeofValue(Expression* expr);

  void Generate();
  void PopulateDeoptimizationData(Handle<Code> code);
  void PopulateTypeFeedbackInfo(Handle<Code> code);
  void PopulateHandlerTable(Handle<Code> code);

  bool MustCreateObjectLiteralWithRuntime(ObjectLiteral* expr) const;
  bool MustCreateArrayLiteralWithRuntime(ArrayLiteral* expr) const;

  void EmitLoadStoreICSlot(FeedbackVectorSlot slot);

  int NewHandlerTableEntry();

  struct BailoutEntry {
    BailoutId id;
    unsigned pc_and_state;
  };

  struct BackEdgeEntry {
    BailoutId id;
    unsigned pc;
    uint32_t loop_depth;
  };

  struct HandlerTableEntry {
    unsigned range_start;
    unsigned range_end;
    unsigned handler_offset;
    int stack_depth;
    int try_catch_depth;
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

    void Plug(bool flag) const override;
    void Plug(Register reg) const override;
    void Plug(Label* materialize_true, Label* materialize_false) const override;
    void Plug(Variable* var) const override;
    void Plug(Handle<Object> lit) const override;
    void Plug(Heap::RootListIndex) const override;
    void PlugTOS() const override;
    void DropAndPlug(int count, Register reg) const override;
    void PrepareTest(Label* materialize_true, Label* materialize_false,
                     Label** if_true, Label** if_false,
                     Label** fall_through) const override;
    bool IsAccumulatorValue() const override { return true; }
  };

  class StackValueContext : public ExpressionContext {
   public:
    explicit StackValueContext(FullCodeGenerator* codegen)
        : ExpressionContext(codegen) { }

    void Plug(bool flag) const override;
    void Plug(Register reg) const override;
    void Plug(Label* materialize_true, Label* materialize_false) const override;
    void Plug(Variable* var) const override;
    void Plug(Handle<Object> lit) const override;
    void Plug(Heap::RootListIndex) const override;
    void PlugTOS() const override;
    void DropAndPlug(int count, Register reg) const override;
    void PrepareTest(Label* materialize_true, Label* materialize_false,
                     Label** if_true, Label** if_false,
                     Label** fall_through) const override;
    bool IsStackValue() const override { return true; }
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
      DCHECK(context->IsTest());
      return reinterpret_cast<const TestContext*>(context);
    }

    Expression* condition() const { return condition_; }
    Label* true_label() const { return true_label_; }
    Label* false_label() const { return false_label_; }
    Label* fall_through() const { return fall_through_; }

    void Plug(bool flag) const override;
    void Plug(Register reg) const override;
    void Plug(Label* materialize_true, Label* materialize_false) const override;
    void Plug(Variable* var) const override;
    void Plug(Handle<Object> lit) const override;
    void Plug(Heap::RootListIndex) const override;
    void PlugTOS() const override;
    void DropAndPlug(int count, Register reg) const override;
    void PrepareTest(Label* materialize_true, Label* materialize_false,
                     Label** if_true, Label** if_false,
                     Label** fall_through) const override;
    bool IsTest() const override { return true; }

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

    void Plug(bool flag) const override;
    void Plug(Register reg) const override;
    void Plug(Label* materialize_true, Label* materialize_false) const override;
    void Plug(Variable* var) const override;
    void Plug(Handle<Object> lit) const override;
    void Plug(Heap::RootListIndex) const override;
    void PlugTOS() const override;
    void DropAndPlug(int count, Register reg) const override;
    void PrepareTest(Label* materialize_true, Label* materialize_false,
                     Label** if_true, Label** if_false,
                     Label** fall_through) const override;
    bool IsEffect() const override { return true; }
  };

  class EnterBlockScopeIfNeeded {
   public:
    EnterBlockScopeIfNeeded(FullCodeGenerator* codegen, Scope* scope,
                            BailoutId entry_id, BailoutId declarations_id,
                            BailoutId exit_id);
    ~EnterBlockScopeIfNeeded();

   private:
    MacroAssembler* masm() const { return codegen_->masm(); }

    FullCodeGenerator* codegen_;
    Scope* saved_scope_;
    BailoutId exit_id_;
    bool needs_block_context_;
  };

  MacroAssembler* masm_;
  CompilationInfo* info_;
  Isolate* isolate_;
  Zone* zone_;
  Scope* scope_;
  Label return_label_;
  NestedStatement* nesting_stack_;
  int loop_depth_;
  int try_catch_depth_;
  ZoneList<Handle<Object> >* globals_;
  Handle<FixedArray> modules_;
  int module_index_;
  const ExpressionContext* context_;
  ZoneList<BailoutEntry> bailout_entries_;
  ZoneList<BackEdgeEntry> back_edges_;
  ZoneVector<HandlerTableEntry> handler_table_;
  int ic_total_count_;
  Handle<Cell> profiling_counter_;
  bool generate_debug_code_;

  friend class NestedStatement;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(FullCodeGenerator);
};


class BackEdgeTable {
 public:
  BackEdgeTable(Code* code, DisallowHeapAllocation* required) {
    DCHECK(code->kind() == Code::FUNCTION);
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

  // Increase allowed loop nesting level by one and patch those matching loops.
  static void Patch(Isolate* isolate, Code* unoptimized_code);

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
  static bool Verify(Isolate* isolate, Code* unoptimized_code);
#endif  // DEBUG

 private:
  Address entry_at(uint32_t index) {
    DCHECK(index < length_);
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


}  // namespace internal
}  // namespace v8

#endif  // V8_FULL_CODEGEN_FULL_CODEGEN_H_
