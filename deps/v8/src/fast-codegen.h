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

#ifndef V8_FAST_CODEGEN_H_
#define V8_FAST_CODEGEN_H_

#include "v8.h"

#include "ast.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Fast code generator.

class FastCodeGenerator: public AstVisitor {
 public:
  FastCodeGenerator(MacroAssembler* masm, Handle<Script> script, bool is_eval)
      : masm_(masm),
        function_(NULL),
        script_(script),
        is_eval_(is_eval),
        nesting_stack_(NULL),
        loop_depth_(0),
        true_label_(NULL),
        false_label_(NULL) {
  }

  static Handle<Code> MakeCode(FunctionLiteral* fun,
                               Handle<Script> script,
                               bool is_eval);

  void Generate(FunctionLiteral* fun);

 private:
  class Breakable;
  class Iteration;
  class TryCatch;
  class TryFinally;
  class Finally;
  class ForIn;

  class NestedStatement BASE_EMBEDDED {
   public:
    explicit NestedStatement(FastCodeGenerator* codegen) : codegen_(codegen) {
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
    virtual TryCatch* AsTryCatch() { return NULL; }
    virtual TryFinally* AsTryFinally() { return NULL; }
    virtual Finally* AsFinally() { return NULL; }
    virtual ForIn* AsForIn() { return NULL; }

    virtual bool IsContinueTarget(Statement* target) { return false; }
    virtual bool IsBreakTarget(Statement* target) { return false; }

    // Generate code to leave the nested statement. This includes
    // cleaning up any stack elements in use and restoring the
    // stack to the expectations of the surrounding statements.
    // Takes a number of stack elements currently on top of the
    // nested statement's stack, and returns a number of stack
    // elements left on top of the surrounding statement's stack.
    // The generated code must preserve the result register (which
    // contains the value in case of a return).
    virtual int Exit(int stack_depth) {
      // Default implementation for the case where there is
      // nothing to clean up.
      return stack_depth;
    }
    NestedStatement* outer() { return previous_; }
   protected:
    MacroAssembler* masm() { return codegen_->masm(); }
   private:
    FastCodeGenerator* codegen_;
    NestedStatement* previous_;
    DISALLOW_COPY_AND_ASSIGN(NestedStatement);
  };

  class Breakable : public NestedStatement {
   public:
    Breakable(FastCodeGenerator* codegen,
              BreakableStatement* break_target)
        : NestedStatement(codegen),
          target_(break_target) {}
    virtual ~Breakable() {}
    virtual Breakable* AsBreakable() { return this; }
    virtual bool IsBreakTarget(Statement* statement) {
      return target_ == statement;
    }
    BreakableStatement* statement() { return target_; }
    Label* break_target() { return &break_target_label_; }
   private:
    BreakableStatement* target_;
    Label break_target_label_;
    DISALLOW_COPY_AND_ASSIGN(Breakable);
  };

  class Iteration : public Breakable {
   public:
    Iteration(FastCodeGenerator* codegen,
              IterationStatement* iteration_statement)
        : Breakable(codegen, iteration_statement) {}
    virtual ~Iteration() {}
    virtual Iteration* AsIteration() { return this; }
    virtual bool IsContinueTarget(Statement* statement) {
      return this->statement() == statement;
    }
    Label* continue_target() { return &continue_target_label_; }
   private:
    Label continue_target_label_;
    DISALLOW_COPY_AND_ASSIGN(Iteration);
  };

  // The environment inside the try block of a try/catch statement.
  class TryCatch : public NestedStatement {
   public:
    explicit TryCatch(FastCodeGenerator* codegen, Label* catch_entry)
        : NestedStatement(codegen), catch_entry_(catch_entry) { }
    virtual ~TryCatch() {}
    virtual TryCatch* AsTryCatch() { return this; }
    Label* catch_entry() { return catch_entry_; }
    virtual int Exit(int stack_depth);
   private:
    Label* catch_entry_;
    DISALLOW_COPY_AND_ASSIGN(TryCatch);
  };

  // The environment inside the try block of a try/finally statement.
  class TryFinally : public NestedStatement {
   public:
    explicit TryFinally(FastCodeGenerator* codegen, Label* finally_entry)
        : NestedStatement(codegen), finally_entry_(finally_entry) { }
    virtual ~TryFinally() {}
    virtual TryFinally* AsTryFinally() { return this; }
    Label* finally_entry() { return finally_entry_; }
    virtual int Exit(int stack_depth);
   private:
    Label* finally_entry_;
    DISALLOW_COPY_AND_ASSIGN(TryFinally);
  };

  // A FinallyEnvironment represents being inside a finally block.
  // Abnormal termination of the finally block needs to clean up
  // the block's parameters from the stack.
  class Finally : public NestedStatement {
   public:
    explicit Finally(FastCodeGenerator* codegen) : NestedStatement(codegen) { }
    virtual ~Finally() {}
    virtual Finally* AsFinally() { return this; }
    virtual int Exit(int stack_depth) {
      return stack_depth + kFinallyStackElementCount;
    }
   private:
    // Number of extra stack slots occupied during a finally block.
    static const int kFinallyStackElementCount = 2;
    DISALLOW_COPY_AND_ASSIGN(Finally);
  };

  // A ForInEnvironment represents being inside a for-in loop.
  // Abnormal termination of the for-in block needs to clean up
  // the block's temporary storage from the stack.
  class ForIn : public Iteration {
   public:
    ForIn(FastCodeGenerator* codegen,
          ForInStatement* statement)
        : Iteration(codegen, statement) { }
    virtual ~ForIn() {}
    virtual ForIn* AsForIn() { return this; }
    virtual int Exit(int stack_depth) {
      return stack_depth + kForInStackElementCount;
    }
   private:
    // TODO(lrn): Check that this value is correct when implementing
    // for-in.
    static const int kForInStackElementCount = 5;
    DISALLOW_COPY_AND_ASSIGN(ForIn);
  };


  int SlotOffset(Slot* slot);

  // Emit code to complete the evaluation of an expression based on its
  // expression context and given its value is in a register, non-lookup
  // slot, or a literal.
  void Apply(Expression::Context context, Register reg);
  void Apply(Expression::Context context, Slot* slot, Register scratch);
  void Apply(Expression::Context context, Literal* lit);

  // Emit code to complete the evaluation of an expression based on its
  // expression context and given its value is on top of the stack.
  void ApplyTOS(Expression::Context context);

  // Emit code to discard count elements from the top of stack, then
  // complete the evaluation of an expression based on its expression
  // context and given its value is in a register.
  void DropAndApply(int count, Expression::Context context, Register reg);

  void Move(Slot* dst, Register source, Register scratch1, Register scratch2);
  void Move(Register dst, Slot* source);

  // Return an operand used to read/write to a known (ie, non-LOOKUP) slot.
  // May emit code to traverse the context chain, destroying the scratch
  // register.
  MemOperand EmitSlotSearch(Slot* slot, Register scratch);

  // Test the JavaScript value in source as if in a test context, compile
  // control flow to a pair of labels.
  void TestAndBranch(Register source, Label* true_label, Label* false_label);

  void VisitForControl(Expression* expr, Label* if_true, Label* if_false) {
    ASSERT(expr->context() == Expression::kTest ||
           expr->context() == Expression::kValueTest ||
           expr->context() == Expression::kTestValue);
    Label* saved_true = true_label_;
    Label* saved_false = false_label_;
    true_label_ = if_true;
    false_label_ = if_false;
    Visit(expr);
    true_label_ = saved_true;
    false_label_ = saved_false;
  }

  void VisitDeclarations(ZoneList<Declaration*>* declarations);
  void DeclareGlobals(Handle<FixedArray> pairs);

  // Platform-specific return sequence
  void EmitReturnSequence(int position);

  // Platform-specific code sequences for calls
  void EmitCallWithStub(Call* expr);
  void EmitCallWithIC(Call* expr, Handle<Object> name, RelocInfo::Mode mode);

  // Platform-specific code for loading variables.
  void EmitVariableLoad(Variable* expr, Expression::Context context);

  // Platform-specific support for compiling assignments.

  // Load a value from a named property.
  // The receiver is left on the stack by the IC.
  void EmitNamedPropertyLoad(Property* expr, Expression::Context context);

  // Load a value from a keyed property.
  // The receiver and the key is left on the stack by the IC.
  void EmitKeyedPropertyLoad(Property* expr, Expression::Context context);

  // Apply the compound assignment operator. Expects both operands on top
  // of the stack.
  void EmitCompoundAssignmentOp(Token::Value op, Expression::Context context);

  // Complete a variable assignment.  The right-hand-side value is expected
  // on top of the stack.
  void EmitVariableAssignment(Variable* var, Expression::Context context);

  // Complete a named property assignment.  The receiver and right-hand-side
  // value are expected on top of the stack.
  void EmitNamedPropertyAssignment(Assignment* expr);

  // Complete a keyed property assignment.  The reciever, key, and
  // right-hand-side value are expected on top of the stack.
  void EmitKeyedPropertyAssignment(Assignment* expr);

  void SetFunctionPosition(FunctionLiteral* fun);
  void SetReturnPosition(FunctionLiteral* fun);
  void SetStatementPosition(Statement* stmt);
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
  static Register result_register();
  static Register context_register();

  // Set fields in the stack frame. Offsets are the frame pointer relative
  // offsets defined in, e.g., StandardFrameConstants.
  void StoreToFrameField(int frame_offset, Register value);

  // Load a value from the current context. Indices are defined as an enum
  // in v8::internal::Context.
  void LoadContextField(Register dst, int context_index);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT
  // Handles the shortcutted logical binary operations in VisitBinaryOperation.
  void EmitLogicalOperation(BinaryOperation* expr);

  MacroAssembler* masm_;
  FunctionLiteral* function_;
  Handle<Script> script_;
  bool is_eval_;
  Label return_label_;
  NestedStatement* nesting_stack_;
  int loop_depth_;

  Label* true_label_;
  Label* false_label_;

  friend class NestedStatement;

  DISALLOW_COPY_AND_ASSIGN(FastCodeGenerator);
};


} }  // namespace v8::internal

#endif  // V8_FAST_CODEGEN_H_
