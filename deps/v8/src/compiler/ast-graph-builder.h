// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_AST_GRAPH_BUILDER_H_
#define V8_COMPILER_AST_GRAPH_BUILDER_H_

#include "src/v8.h"

#include "src/ast.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/js-graph.h"

namespace v8 {
namespace internal {
namespace compiler {

class ControlBuilder;
class LoopBuilder;
class Graph;

// The AstGraphBuilder produces a high-level IR graph, based on an
// underlying AST. The produced graph can either be compiled into a
// stand-alone function or be wired into another graph for the purposes
// of function inlining.
class AstGraphBuilder : public StructuredGraphBuilder, public AstVisitor {
 public:
  AstGraphBuilder(CompilationInfo* info, JSGraph* jsgraph);

  // Creates a graph by visiting the entire AST.
  bool CreateGraph();

 protected:
  class AstContext;
  class AstEffectContext;
  class AstValueContext;
  class AstTestContext;
  class BreakableScope;
  class ContextScope;
  class Environment;

  Environment* environment() {
    return reinterpret_cast<Environment*>(
        StructuredGraphBuilder::environment());
  }

  AstContext* ast_context() const { return ast_context_; }
  BreakableScope* breakable() const { return breakable_; }
  ContextScope* execution_context() const { return execution_context_; }

  void set_ast_context(AstContext* ctx) { ast_context_ = ctx; }
  void set_breakable(BreakableScope* brk) { breakable_ = brk; }
  void set_execution_context(ContextScope* ctx) { execution_context_ = ctx; }

  // Support for control flow builders. The concrete type of the environment
  // depends on the graph builder, but environments themselves are not virtual.
  typedef StructuredGraphBuilder::Environment BaseEnvironment;
  virtual BaseEnvironment* CopyEnvironment(BaseEnvironment* env);

  // TODO(mstarzinger): The pipeline only needs to be a friend to access the
  // function context. Remove as soon as the context is a parameter.
  friend class Pipeline;

  // Getters for values in the activation record.
  Node* GetFunctionClosure();
  Node* GetFunctionContext();

  //
  // The following build methods all generate graph fragments and return one
  // resulting node. The operand stack height remains the same, variables and
  // other dependencies tracked by the environment might be mutated though.
  //

  // Builder to create a local function context.
  Node* BuildLocalFunctionContext(Node* context, Node* closure);

  // Builder to create an arguments object if it is used.
  Node* BuildArgumentsObject(Variable* arguments);

  // Builders for variable load and assignment.
  Node* BuildVariableAssignment(Variable* var, Node* value, Token::Value op,
                                BailoutId bailout_id);
  Node* BuildVariableDelete(Variable* var);
  Node* BuildVariableLoad(Variable* var, BailoutId bailout_id,
                          ContextualMode mode = CONTEXTUAL);

  // Builders for accessing the function context.
  Node* BuildLoadBuiltinsObject();
  Node* BuildLoadGlobalObject();
  Node* BuildLoadClosure();

  // Builders for automatic type conversion.
  Node* BuildToBoolean(Node* value);

  // Builders for error reporting at runtime.
  Node* BuildThrowReferenceError(Variable* var);

  // Builders for dynamic hole-checks at runtime.
  Node* BuildHoleCheckSilent(Node* value, Node* for_hole, Node* not_hole);
  Node* BuildHoleCheckThrow(Node* value, Variable* var, Node* not_hole);

  // Builders for binary operations.
  Node* BuildBinaryOp(Node* left, Node* right, Token::Value op);

#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  // Visiting functions for AST nodes make this an AstVisitor.
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Visiting function for declarations list is overridden.
  virtual void VisitDeclarations(ZoneList<Declaration*>* declarations);

 private:
  CompilationInfo* info_;
  AstContext* ast_context_;
  JSGraph* jsgraph_;

  // List of global declarations for functions and variables.
  ZoneList<Handle<Object> > globals_;

  // Stack of breakable statements entered by the visitor.
  BreakableScope* breakable_;

  // Stack of context objects pushed onto the chain by the visitor.
  ContextScope* execution_context_;

  // Nodes representing values in the activation record.
  SetOncePointer<Node> function_closure_;
  SetOncePointer<Node> function_context_;

  CompilationInfo* info() { return info_; }
  StrictMode strict_mode() { return info()->strict_mode(); }
  JSGraph* jsgraph() { return jsgraph_; }
  JSOperatorBuilder* javascript() { return jsgraph_->javascript(); }
  ZoneList<Handle<Object> >* globals() { return &globals_; }

  // Current scope during visitation.
  inline Scope* current_scope() const;

  // Process arguments to a call by popping {arity} elements off the operand
  // stack and build a call node using the given call operator.
  Node* ProcessArguments(Operator* op, int arity);

  // Visit statements.
  void VisitIfNotNull(Statement* stmt);

  // Visit expressions.
  void VisitForTest(Expression* expr);
  void VisitForEffect(Expression* expr);
  void VisitForValue(Expression* expr);
  void VisitForValueOrNull(Expression* expr);
  void VisitForValues(ZoneList<Expression*>* exprs);

  // Common for all IterationStatement bodies.
  void VisitIterationBody(IterationStatement* stmt, LoopBuilder* loop, int);

  // Dispatched from VisitCallRuntime.
  void VisitCallJSRuntime(CallRuntime* expr);

  // Dispatched from VisitUnaryOperation.
  void VisitDelete(UnaryOperation* expr);
  void VisitVoid(UnaryOperation* expr);
  void VisitTypeof(UnaryOperation* expr);
  void VisitNot(UnaryOperation* expr);

  // Dispatched from VisitBinaryOperation.
  void VisitComma(BinaryOperation* expr);
  void VisitLogicalExpression(BinaryOperation* expr);
  void VisitArithmeticExpression(BinaryOperation* expr);

  // Dispatched from VisitForInStatement.
  void VisitForInAssignment(Expression* expr, Node* value);

  void BuildLazyBailout(Node* node, BailoutId ast_id);
  void BuildLazyBailoutWithPushedNode(Node* node, BailoutId ast_id);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(AstGraphBuilder);
};


// The abstract execution environment for generated code consists of
// parameter variables, local variables and the operand stack. The
// environment will perform proper SSA-renaming of all tracked nodes
// at split and merge points in the control flow. Internally all the
// values are stored in one list using the following layout:
//
//  [parameters (+receiver)] [locals] [operand stack]
//
class AstGraphBuilder::Environment
    : public StructuredGraphBuilder::Environment {
 public:
  Environment(AstGraphBuilder* builder, Scope* scope, Node* control_dependency);
  Environment(const Environment& copy);

  int parameters_count() const { return parameters_count_; }
  int locals_count() const { return locals_count_; }
  int stack_height() {
    return static_cast<int>(values()->size()) - parameters_count_ -
           locals_count_;
  }

  // Operations on parameter or local variables. The parameter indices are
  // shifted by 1 (receiver is parameter index -1 but environment index 0).
  void Bind(Variable* variable, Node* node) {
    DCHECK(variable->IsStackAllocated());
    if (variable->IsParameter()) {
      values()->at(variable->index() + 1) = node;
      parameters_dirty_ = true;
    } else {
      DCHECK(variable->IsStackLocal());
      values()->at(variable->index() + parameters_count_) = node;
      locals_dirty_ = true;
    }
  }
  Node* Lookup(Variable* variable) {
    DCHECK(variable->IsStackAllocated());
    if (variable->IsParameter()) {
      return values()->at(variable->index() + 1);
    } else {
      DCHECK(variable->IsStackLocal());
      return values()->at(variable->index() + parameters_count_);
    }
  }

  // Operations on the operand stack.
  void Push(Node* node) {
    values()->push_back(node);
    stack_dirty_ = true;
  }
  Node* Top() {
    DCHECK(stack_height() > 0);
    return values()->back();
  }
  Node* Pop() {
    DCHECK(stack_height() > 0);
    Node* back = values()->back();
    values()->pop_back();
    stack_dirty_ = true;
    return back;
  }

  // Direct mutations of the operand stack.
  void Poke(int depth, Node* node) {
    DCHECK(depth >= 0 && depth < stack_height());
    int index = static_cast<int>(values()->size()) - depth - 1;
    values()->at(index) = node;
    stack_dirty_ = true;
  }
  Node* Peek(int depth) {
    DCHECK(depth >= 0 && depth < stack_height());
    int index = static_cast<int>(values()->size()) - depth - 1;
    return values()->at(index);
  }
  void Drop(int depth) {
    DCHECK(depth >= 0 && depth <= stack_height());
    values()->erase(values()->end() - depth, values()->end());
    stack_dirty_ = true;
  }

  // Preserve a checkpoint of the environment for the IR graph. Any
  // further mutation of the environment will not affect checkpoints.
  Node* Checkpoint(BailoutId ast_id);

 private:
  int parameters_count_;
  int locals_count_;
  Node* parameters_node_;
  Node* locals_node_;
  Node* stack_node_;
  bool parameters_dirty_;
  bool locals_dirty_;
  bool stack_dirty_;
};


// Each expression in the AST is evaluated in a specific context. This context
// decides how the evaluation result is passed up the visitor.
class AstGraphBuilder::AstContext BASE_EMBEDDED {
 public:
  bool IsEffect() const { return kind_ == Expression::kEffect; }
  bool IsValue() const { return kind_ == Expression::kValue; }
  bool IsTest() const { return kind_ == Expression::kTest; }

  // Plug a node into this expression context.  Call this function in tail
  // position in the Visit functions for expressions.
  virtual void ProduceValue(Node* value) = 0;
  virtual void ProduceValueWithLazyBailout(Node* value) = 0;

  // Unplugs a node from this expression context.  Call this to retrieve the
  // result of another Visit function that already plugged the context.
  virtual Node* ConsumeValue() = 0;

  // Shortcut for "context->ProduceValue(context->ConsumeValue())".
  void ReplaceValue() { ProduceValue(ConsumeValue()); }

 protected:
  AstContext(AstGraphBuilder* owner, Expression::Context kind,
             BailoutId bailout_id);
  virtual ~AstContext();

  AstGraphBuilder* owner() const { return owner_; }
  Environment* environment() const { return owner_->environment(); }

// We want to be able to assert, in a context-specific way, that the stack
// height makes sense when the context is filled.
#ifdef DEBUG
  int original_height_;
#endif

  BailoutId bailout_id_;

 private:
  Expression::Context kind_;
  AstGraphBuilder* owner_;
  AstContext* outer_;
};


// Context to evaluate expression for its side effects only.
class AstGraphBuilder::AstEffectContext V8_FINAL : public AstContext {
 public:
  explicit AstEffectContext(AstGraphBuilder* owner, BailoutId bailout_id)
      : AstContext(owner, Expression::kEffect, bailout_id) {}
  virtual ~AstEffectContext();
  virtual void ProduceValue(Node* value) V8_OVERRIDE;
  virtual void ProduceValueWithLazyBailout(Node* value) V8_OVERRIDE;
  virtual Node* ConsumeValue() V8_OVERRIDE;
};


// Context to evaluate expression for its value (and side effects).
class AstGraphBuilder::AstValueContext V8_FINAL : public AstContext {
 public:
  explicit AstValueContext(AstGraphBuilder* owner, BailoutId bailout_id)
      : AstContext(owner, Expression::kValue, bailout_id) {}
  virtual ~AstValueContext();
  virtual void ProduceValue(Node* value) V8_OVERRIDE;
  virtual void ProduceValueWithLazyBailout(Node* value) V8_OVERRIDE;
  virtual Node* ConsumeValue() V8_OVERRIDE;
};


// Context to evaluate expression for a condition value (and side effects).
class AstGraphBuilder::AstTestContext V8_FINAL : public AstContext {
 public:
  explicit AstTestContext(AstGraphBuilder* owner, BailoutId bailout_id)
      : AstContext(owner, Expression::kTest, bailout_id) {}
  virtual ~AstTestContext();
  virtual void ProduceValue(Node* value) V8_OVERRIDE;
  virtual void ProduceValueWithLazyBailout(Node* value) V8_OVERRIDE;
  virtual Node* ConsumeValue() V8_OVERRIDE;
};


// Scoped class tracking breakable statements entered by the visitor. Allows to
// properly 'break' and 'continue' iteration statements as well as to 'break'
// from blocks within switch statements.
class AstGraphBuilder::BreakableScope BASE_EMBEDDED {
 public:
  BreakableScope(AstGraphBuilder* owner, BreakableStatement* target,
                 ControlBuilder* control, int drop_extra)
      : owner_(owner),
        target_(target),
        next_(owner->breakable()),
        control_(control),
        drop_extra_(drop_extra) {
    owner_->set_breakable(this);  // Push.
  }

  ~BreakableScope() {
    owner_->set_breakable(next_);  // Pop.
  }

  // Either 'break' or 'continue' the target statement.
  void BreakTarget(BreakableStatement* target);
  void ContinueTarget(BreakableStatement* target);

 private:
  AstGraphBuilder* owner_;
  BreakableStatement* target_;
  BreakableScope* next_;
  ControlBuilder* control_;
  int drop_extra_;

  // Find the correct scope for the target statement. Note that this also drops
  // extra operands from the environment for each scope skipped along the way.
  BreakableScope* FindBreakable(BreakableStatement* target);
};


// Scoped class tracking context objects created by the visitor. Represents
// mutations of the context chain within the function body and allows to
// change the current {scope} and {context} during visitation.
class AstGraphBuilder::ContextScope BASE_EMBEDDED {
 public:
  ContextScope(AstGraphBuilder* owner, Scope* scope, Node* context)
      : owner_(owner),
        next_(owner->execution_context()),
        outer_(owner->current_context()),
        scope_(scope) {
    owner_->set_execution_context(this);  // Push.
    owner_->set_current_context(context);
  }

  ~ContextScope() {
    owner_->set_execution_context(next_);  // Pop.
    owner_->set_current_context(outer_);
  }

  // Current scope during visitation.
  Scope* scope() const { return scope_; }

 private:
  AstGraphBuilder* owner_;
  ContextScope* next_;
  Node* outer_;
  Scope* scope_;
};

Scope* AstGraphBuilder::current_scope() const {
  return execution_context_->scope();
}
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_AST_GRAPH_BUILDER_H_
