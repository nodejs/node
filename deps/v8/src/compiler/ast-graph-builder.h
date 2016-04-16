// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_AST_GRAPH_BUILDER_H_
#define V8_COMPILER_AST_GRAPH_BUILDER_H_

#include "src/ast/ast.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/liveness-analyzer.h"
#include "src/compiler/state-values-utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class BitVector;


namespace compiler {

// Forward declarations.
class ControlBuilder;
class Graph;
class LoopAssignmentAnalysis;
class LoopBuilder;
class Node;
class TypeHintAnalysis;


// The AstGraphBuilder produces a high-level IR graph, based on an
// underlying AST. The produced graph can either be compiled into a
// stand-alone function or be wired into another graph for the purposes
// of function inlining.
class AstGraphBuilder : public AstVisitor {
 public:
  AstGraphBuilder(Zone* local_zone, CompilationInfo* info, JSGraph* jsgraph,
                  LoopAssignmentAnalysis* loop_assignment = nullptr,
                  TypeHintAnalysis* type_hint_analysis = nullptr);

  // Creates a graph by visiting the entire AST.
  bool CreateGraph(bool stack_check = true);

  // Helpers to create new control nodes.
  Node* NewIfTrue() { return NewNode(common()->IfTrue()); }
  Node* NewIfFalse() { return NewNode(common()->IfFalse()); }
  Node* NewMerge() { return NewNode(common()->Merge(1), true); }
  Node* NewLoop() { return NewNode(common()->Loop(1), true); }
  Node* NewBranch(Node* condition, BranchHint hint = BranchHint::kNone) {
    return NewNode(common()->Branch(hint), condition);
  }

 protected:
#define DECLARE_VISIT(type) void Visit##type(type* node) override;
  // Visiting functions for AST nodes make this an AstVisitor.
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Visiting function for declarations list is overridden.
  void VisitDeclarations(ZoneList<Declaration*>* declarations) override;

 private:
  class AstContext;
  class AstEffectContext;
  class AstValueContext;
  class AstTestContext;
  class ContextScope;
  class ControlScope;
  class ControlScopeForBreakable;
  class ControlScopeForIteration;
  class ControlScopeForCatch;
  class ControlScopeForFinally;
  class Environment;
  class FrameStateBeforeAndAfter;
  friend class ControlBuilder;

  Isolate* isolate_;
  Zone* local_zone_;
  CompilationInfo* info_;
  JSGraph* jsgraph_;
  Environment* environment_;
  AstContext* ast_context_;

  // List of global declarations for functions and variables.
  ZoneVector<Handle<Object>> globals_;

  // Stack of control scopes currently entered by the visitor.
  ControlScope* execution_control_;

  // Stack of context objects pushed onto the chain by the visitor.
  ContextScope* execution_context_;

  // Nodes representing values in the activation record.
  SetOncePointer<Node> function_closure_;
  SetOncePointer<Node> function_context_;
  SetOncePointer<Node> new_target_;

  // Tracks how many try-blocks are currently entered.
  int try_catch_nesting_level_;
  int try_nesting_level_;

  // Temporary storage for building node input lists.
  int input_buffer_size_;
  Node** input_buffer_;

  // Optimization to cache loaded feedback vector.
  SetOncePointer<Node> feedback_vector_;

  // Control nodes that exit the function body.
  ZoneVector<Node*> exit_controls_;

  // Result of loop assignment analysis performed before graph creation.
  LoopAssignmentAnalysis* loop_assignment_analysis_;

  // Result of type hint analysis performed before graph creation.
  TypeHintAnalysis* type_hint_analysis_;

  // Cache for StateValues nodes for frame states.
  StateValuesCache state_values_cache_;

  // Analyzer of local variable liveness.
  LivenessAnalyzer liveness_analyzer_;

  // Function info for frame state construction.
  const FrameStateFunctionInfo* const frame_state_function_info_;

  // Growth increment for the temporary buffer used to construct input lists to
  // new nodes.
  static const int kInputBufferSizeIncrement = 64;

  Zone* local_zone() const { return local_zone_; }
  Environment* environment() const { return environment_; }
  AstContext* ast_context() const { return ast_context_; }
  ControlScope* execution_control() const { return execution_control_; }
  ContextScope* execution_context() const { return execution_context_; }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return isolate_; }
  LanguageMode language_mode() const;
  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph_->graph(); }
  Zone* graph_zone() { return graph()->zone(); }
  JSOperatorBuilder* javascript() { return jsgraph_->javascript(); }
  ZoneVector<Handle<Object>>* globals() { return &globals_; }
  Scope* current_scope() const;
  Node* current_context() const;
  LivenessAnalyzer* liveness_analyzer() { return &liveness_analyzer_; }
  const FrameStateFunctionInfo* frame_state_function_info() const {
    return frame_state_function_info_;
  }

  void set_environment(Environment* env) { environment_ = env; }
  void set_ast_context(AstContext* ctx) { ast_context_ = ctx; }
  void set_execution_control(ControlScope* ctrl) { execution_control_ = ctrl; }
  void set_execution_context(ContextScope* ctx) { execution_context_ = ctx; }

  // Create the main graph body by visiting the AST.
  void CreateGraphBody(bool stack_check);

  // Get or create the node that represents the incoming function closure.
  Node* GetFunctionClosureForContext();
  Node* GetFunctionClosure();

  // Get or create the node that represents the incoming function context.
  Node* GetFunctionContext();

  // Get or create the node that represents the incoming new target value.
  Node* GetNewTarget();

  // Node creation helpers.
  Node* NewNode(const Operator* op, bool incomplete = false) {
    return MakeNode(op, 0, static_cast<Node**>(nullptr), incomplete);
  }

  Node* NewNode(const Operator* op, Node* n1) {
    return MakeNode(op, 1, &n1, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* buffer[] = {n1, n2};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* buffer[] = {n1, n2, n3};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* buffer[] = {n1, n2, n3, n4};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5) {
    Node* buffer[] = {n1, n2, n3, n4, n5};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6};
    return MakeNode(op, arraysize(nodes), nodes, false);
  }

  Node* NewNode(const Operator* op, int value_input_count, Node** value_inputs,
                bool incomplete = false) {
    return MakeNode(op, value_input_count, value_inputs, incomplete);
  }

  // Creates a new Phi node having {count} input values.
  Node* NewPhi(int count, Node* input, Node* control);
  Node* NewEffectPhi(int count, Node* input, Node* control);

  // Helpers for merging control, effect or value dependencies.
  Node* MergeControl(Node* control, Node* other);
  Node* MergeEffect(Node* value, Node* other, Node* control);
  Node* MergeValue(Node* value, Node* other, Node* control);

  // The main node creation chokepoint. Adds context, frame state, effect,
  // and control dependencies depending on the operator.
  Node* MakeNode(const Operator* op, int value_input_count, Node** value_inputs,
                 bool incomplete);

  // Helper to indicate a node exits the function body.
  void UpdateControlDependencyToLeaveFunction(Node* exit);

  // Builds deoptimization for a given node.
  void PrepareFrameState(Node* node, BailoutId ast_id,
                         OutputFrameStateCombine framestate_combine =
                             OutputFrameStateCombine::Ignore());

  BitVector* GetVariablesAssignedInLoop(IterationStatement* stmt);

  // Check if the given statement is an OSR entry.
  // If so, record the stack height into the compilation and return {true}.
  bool CheckOsrEntry(IterationStatement* stmt);

  // Computes local variable liveness and replaces dead variables in
  // frame states with the undefined values.
  void ClearNonLiveSlotsInFrameStates();

  Node** EnsureInputBufferSize(int size);

  // Named and keyed loads require a VectorSlotPair for successful lowering.
  VectorSlotPair CreateVectorSlotPair(FeedbackVectorSlot slot) const;

  // Determine which contexts need to be checked for extension objects that
  // might shadow the optimistic declaration of dynamic lookup variables.
  uint32_t ComputeBitsetForDynamicGlobal(Variable* variable);
  uint32_t ComputeBitsetForDynamicContext(Variable* variable);

  // ===========================================================================
  // The following build methods all generate graph fragments and return one
  // resulting node. The operand stack height remains the same, variables and
  // other dependencies tracked by the environment might be mutated though.

  // Builders to create local function, script and block contexts.
  Node* BuildLocalActivationContext(Node* context);
  Node* BuildLocalFunctionContext(Scope* scope);
  Node* BuildLocalScriptContext(Scope* scope);
  Node* BuildLocalBlockContext(Scope* scope);

  // Builder to create an arguments object if it is used.
  Node* BuildArgumentsObject(Variable* arguments);

  // Builder to create an array of rest parameters if used
  Node* BuildRestArgumentsArray(Variable* rest, int index);

  // Builder that assigns to the {.this_function} internal variable if needed.
  Node* BuildThisFunctionVariable(Variable* this_function_var);

  // Builder that assigns to the {new.target} internal variable if needed.
  Node* BuildNewTargetVariable(Variable* new_target_var);

  // Builders for variable load and assignment.
  Node* BuildVariableAssignment(Variable* variable, Node* value,
                                Token::Value op, const VectorSlotPair& slot,
                                BailoutId bailout_id,
                                FrameStateBeforeAndAfter& states,
                                OutputFrameStateCombine framestate_combine =
                                    OutputFrameStateCombine::Ignore());
  Node* BuildVariableDelete(Variable* variable, BailoutId bailout_id,
                            OutputFrameStateCombine framestate_combine);
  Node* BuildVariableLoad(Variable* variable, BailoutId bailout_id,
                          FrameStateBeforeAndAfter& states,
                          const VectorSlotPair& feedback,
                          OutputFrameStateCombine framestate_combine,
                          TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);

  // Builders for property loads and stores.
  Node* BuildKeyedLoad(Node* receiver, Node* key,
                       const VectorSlotPair& feedback);
  Node* BuildNamedLoad(Node* receiver, Handle<Name> name,
                       const VectorSlotPair& feedback);
  Node* BuildKeyedStore(Node* receiver, Node* key, Node* value,
                        const VectorSlotPair& feedback);
  Node* BuildNamedStore(Node* receiver, Handle<Name> name, Node* value,
                        const VectorSlotPair& feedback);

  // Builders for super property loads and stores.
  Node* BuildKeyedSuperStore(Node* receiver, Node* home_object, Node* key,
                             Node* value);
  Node* BuildNamedSuperStore(Node* receiver, Node* home_object,
                             Handle<Name> name, Node* value);
  Node* BuildNamedSuperLoad(Node* receiver, Node* home_object,
                            Handle<Name> name, const VectorSlotPair& feedback);
  Node* BuildKeyedSuperLoad(Node* receiver, Node* home_object, Node* key,
                            const VectorSlotPair& feedback);

  // Builders for global variable loads and stores.
  Node* BuildGlobalLoad(Handle<Name> name, const VectorSlotPair& feedback,
                        TypeofMode typeof_mode);
  Node* BuildGlobalStore(Handle<Name> name, Node* value,
                         const VectorSlotPair& feedback);

  // Builders for dynamic variable loads and stores.
  Node* BuildDynamicLoad(Handle<Name> name, TypeofMode typeof_mode);
  Node* BuildDynamicStore(Handle<Name> name, Node* value);

  // Builders for accessing the function context.
  Node* BuildLoadGlobalObject();
  Node* BuildLoadNativeContextField(int index);

  // Builders for automatic type conversion.
  Node* BuildToBoolean(Node* input, TypeFeedbackId feedback_id);
  Node* BuildToName(Node* input, BailoutId bailout_id);
  Node* BuildToObject(Node* input, BailoutId bailout_id);

  // Builder for adding the [[HomeObject]] to a value if the value came from a
  // function literal and needs a home object. Do nothing otherwise.
  Node* BuildSetHomeObject(Node* value, Node* home_object,
                           ObjectLiteralProperty* property,
                           int slot_number = 0);

  // Builders for error reporting at runtime.
  Node* BuildThrowError(Node* exception, BailoutId bailout_id);
  Node* BuildThrowReferenceError(Variable* var, BailoutId bailout_id);
  Node* BuildThrowConstAssignError(BailoutId bailout_id);
  Node* BuildThrowStaticPrototypeError(BailoutId bailout_id);
  Node* BuildThrowUnsupportedSuperError(BailoutId bailout_id);

  // Builders for dynamic hole-checks at runtime.
  Node* BuildHoleCheckSilent(Node* value, Node* for_hole, Node* not_hole);
  Node* BuildHoleCheckThenThrow(Node* value, Variable* var, Node* not_hole,
                                BailoutId bailout_id);
  Node* BuildHoleCheckElseThrow(Node* value, Variable* var, Node* for_hole,
                                BailoutId bailout_id);

  // Builders for conditional errors.
  Node* BuildThrowIfStaticPrototype(Node* name, BailoutId bailout_id);

  // Builders for non-local control flow.
  Node* BuildReturn(Node* return_value);
  Node* BuildThrow(Node* exception_value);

  // Builders for binary operations.
  Node* BuildBinaryOp(Node* left, Node* right, Token::Value op,
                      TypeFeedbackId feedback_id);

  // Process arguments to a call by popping {arity} elements off the operand
  // stack and build a call node using the given call operator.
  Node* ProcessArguments(const Operator* op, int arity);

  // ===========================================================================
  // The following build methods have the same contract as the above ones, but
  // they can also return {nullptr} to indicate that no fragment was built. Note
  // that these are optimizations, disabling any of them should still produce
  // correct graphs.

  // Optimization for variable load from global object.
  Node* TryLoadGlobalConstant(Handle<Name> name);

  // Optimization for variable load of dynamic lookup slot that is most likely
  // to resolve to a global slot or context slot (inferred from scope chain).
  Node* TryLoadDynamicVariable(Variable* variable, Handle<String> name,
                               BailoutId bailout_id,
                               FrameStateBeforeAndAfter& states,
                               const VectorSlotPair& feedback,
                               OutputFrameStateCombine combine,
                               TypeofMode typeof_mode);

  // Optimizations for automatic type conversion.
  Node* TryFastToBoolean(Node* input);
  Node* TryFastToName(Node* input);

  // ===========================================================================
  // The following visitation methods all recursively visit a subtree of the
  // underlying AST and extent the graph. The operand stack is mutated in a way
  // consistent with other compilers:
  //  - Expressions pop operands and push result, depending on {AstContext}.
  //  - Statements keep the operand stack balanced.

  // Visit statements.
  void VisitIfNotNull(Statement* stmt);
  void VisitInScope(Statement* stmt, Scope* scope, Node* context);

  // Visit expressions.
  void Visit(Expression* expr);
  void VisitForTest(Expression* expr);
  void VisitForEffect(Expression* expr);
  void VisitForValue(Expression* expr);
  void VisitForValueOrNull(Expression* expr);
  void VisitForValueOrTheHole(Expression* expr);
  void VisitForValues(ZoneList<Expression*>* exprs);

  // Common for all IterationStatement bodies.
  void VisitIterationBody(IterationStatement* stmt, LoopBuilder* loop);

  // Dispatched from VisitCall.
  void VisitCallSuper(Call* expr);

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
  void VisitForInAssignment(Expression* expr, Node* value,
                            const VectorSlotPair& feedback,
                            BailoutId bailout_id_before,
                            BailoutId bailout_id_after);

  // Dispatched from VisitObjectLiteral.
  void VisitObjectLiteralAccessor(Node* home_object,
                                  ObjectLiteralProperty* property);

  // Dispatched from VisitClassLiteral.
  void VisitClassLiteralContents(ClassLiteral* expr);

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
class AstGraphBuilder::Environment : public ZoneObject {
 public:
  Environment(AstGraphBuilder* builder, Scope* scope, Node* control_dependency);

  int parameters_count() const { return parameters_count_; }
  int locals_count() const { return locals_count_; }
  int context_chain_length() { return static_cast<int>(contexts_.size()); }
  int stack_height() {
    return static_cast<int>(values()->size()) - parameters_count_ -
           locals_count_;
  }

  // Operations on parameter or local variables.
  void Bind(Variable* variable, Node* node);
  Node* Lookup(Variable* variable);
  void MarkAllLocalsLive();

  // Raw operations on parameter variables.
  void RawParameterBind(int index, Node* node);
  Node* RawParameterLookup(int index);

  // Operations on the context chain.
  Node* Context() const { return contexts_.back(); }
  void PushContext(Node* context) { contexts()->push_back(context); }
  void PopContext() { contexts()->pop_back(); }
  void TrimContextChain(int trim_to_length) {
    contexts()->resize(trim_to_length);
  }

  // Operations on the operand stack.
  void Push(Node* node) {
    values()->push_back(node);
  }
  Node* Top() {
    DCHECK(stack_height() > 0);
    return values()->back();
  }
  Node* Pop() {
    DCHECK(stack_height() > 0);
    Node* back = values()->back();
    values()->pop_back();
    return back;
  }

  // Direct mutations of the operand stack.
  void Poke(int depth, Node* node) {
    DCHECK(depth >= 0 && depth < stack_height());
    int index = static_cast<int>(values()->size()) - depth - 1;
    values()->at(index) = node;
  }
  Node* Peek(int depth) {
    DCHECK(depth >= 0 && depth < stack_height());
    int index = static_cast<int>(values()->size()) - depth - 1;
    return values()->at(index);
  }
  void Drop(int depth) {
    DCHECK(depth >= 0 && depth <= stack_height());
    values()->erase(values()->end() - depth, values()->end());
  }
  void TrimStack(int trim_to_height) {
    int depth = stack_height() - trim_to_height;
    DCHECK(depth >= 0 && depth <= stack_height());
    values()->erase(values()->end() - depth, values()->end());
  }

  // Preserve a checkpoint of the environment for the IR graph. Any
  // further mutation of the environment will not affect checkpoints.
  Node* Checkpoint(BailoutId ast_id, OutputFrameStateCombine combine =
                                         OutputFrameStateCombine::Ignore(),
                   bool node_has_exception = false);

  // Control dependency tracked by this environment.
  Node* GetControlDependency() { return control_dependency_; }
  void UpdateControlDependency(Node* dependency) {
    control_dependency_ = dependency;
  }

  // Effect dependency tracked by this environment.
  Node* GetEffectDependency() { return effect_dependency_; }
  void UpdateEffectDependency(Node* dependency) {
    effect_dependency_ = dependency;
  }

  // Mark this environment as being unreachable.
  void MarkAsUnreachable() {
    UpdateControlDependency(builder()->jsgraph()->Dead());
    liveness_block_ = nullptr;
  }
  bool IsMarkedAsUnreachable() {
    return GetControlDependency()->opcode() == IrOpcode::kDead;
  }

  // Merge another environment into this one.
  void Merge(Environment* other);

  // Copies this environment at a control-flow split point.
  Environment* CopyForConditional();

  // Copies this environment to a potentially unreachable control-flow point.
  Environment* CopyAsUnreachable();

  // Copies this environment at a loop header control-flow point.
  Environment* CopyForLoop(BitVector* assigned, bool is_osr = false);

 private:
  AstGraphBuilder* builder_;
  int parameters_count_;
  int locals_count_;
  LivenessAnalyzerBlock* liveness_block_;
  NodeVector values_;
  NodeVector contexts_;
  Node* control_dependency_;
  Node* effect_dependency_;
  Node* parameters_node_;
  Node* locals_node_;
  Node* stack_node_;

  explicit Environment(Environment* copy,
                       LivenessAnalyzerBlock* liveness_block);
  Environment* CopyAndShareLiveness();
  void UpdateStateValues(Node** state_values, int offset, int count);
  void UpdateStateValuesWithCache(Node** state_values, int offset, int count);
  Zone* zone() const { return builder_->local_zone(); }
  Graph* graph() const { return builder_->graph(); }
  AstGraphBuilder* builder() const { return builder_; }
  CommonOperatorBuilder* common() { return builder_->common(); }
  NodeVector* values() { return &values_; }
  NodeVector* contexts() { return &contexts_; }
  LivenessAnalyzerBlock* liveness_block() { return liveness_block_; }
  bool IsLivenessAnalysisEnabled();
  bool IsLivenessBlockConsistent();

  // Prepare environment to be used as loop header.
  void PrepareForLoop(BitVector* assigned, bool is_osr = false);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_AST_GRAPH_BUILDER_H_
