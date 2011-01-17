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

#ifndef V8_HYDROGEN_H_
#define V8_HYDROGEN_H_

#include "v8.h"

#include "ast.h"
#include "compiler.h"
#include "data-flow.h"
#include "hydrogen-instructions.h"
#include "zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HEnvironment;
class HGraph;
class HLoopInformation;
class HTracer;
class LAllocator;
class LChunk;
class LiveRange;


class HBasicBlock: public ZoneObject {
 public:
  explicit HBasicBlock(HGraph* graph);
  virtual ~HBasicBlock() { }

  // Simple accessors.
  int block_id() const { return block_id_; }
  void set_block_id(int id) { block_id_ = id; }
  HGraph* graph() const { return graph_; }
  const ZoneList<HPhi*>* phis() const { return &phis_; }
  HInstruction* first() const { return first_; }
  HInstruction* GetLastInstruction();
  HControlInstruction* end() const { return end_; }
  HLoopInformation* loop_information() const { return loop_information_; }
  const ZoneList<HBasicBlock*>* predecessors() const { return &predecessors_; }
  bool HasPredecessor() const { return predecessors_.length() > 0; }
  const ZoneList<HBasicBlock*>* dominated_blocks() const {
    return &dominated_blocks_;
  }
  const ZoneList<int>* deleted_phis() const {
    return &deleted_phis_;
  }
  void RecordDeletedPhi(int merge_index) {
    deleted_phis_.Add(merge_index);
  }
  HBasicBlock* dominator() const { return dominator_; }
  HEnvironment* last_environment() const { return last_environment_; }
  int argument_count() const { return argument_count_; }
  void set_argument_count(int count) { argument_count_ = count; }
  int first_instruction_index() const { return first_instruction_index_; }
  void set_first_instruction_index(int index) {
    first_instruction_index_ = index;
  }
  int last_instruction_index() const { return last_instruction_index_; }
  void set_last_instruction_index(int index) {
    last_instruction_index_ = index;
  }

  void AttachLoopInformation();
  void DetachLoopInformation();
  bool IsLoopHeader() const { return loop_information() != NULL; }
  bool IsStartBlock() const { return block_id() == 0; }
  void PostProcessLoopHeader(IterationStatement* stmt);

  bool IsFinished() const { return end_ != NULL; }
  void AddPhi(HPhi* phi);
  void RemovePhi(HPhi* phi);
  void AddInstruction(HInstruction* instr);
  bool Dominates(HBasicBlock* other) const;

  void SetInitialEnvironment(HEnvironment* env);
  void ClearEnvironment() { last_environment_ = NULL; }
  bool HasEnvironment() const { return last_environment_ != NULL; }
  void UpdateEnvironment(HEnvironment* env) { last_environment_ = env; }
  HBasicBlock* parent_loop_header() const {
    if (!HasParentLoopHeader()) return NULL;
    return parent_loop_header_.get();
  }

  void set_parent_loop_header(HBasicBlock* block) {
    parent_loop_header_.set(block);
  }

  bool HasParentLoopHeader() const { return parent_loop_header_.is_set(); }

  void SetJoinId(int id);

  void Finish(HControlInstruction* last);
  void Goto(HBasicBlock* block, bool include_stack_check = false);

  int PredecessorIndexOf(HBasicBlock* predecessor) const;
  void AddSimulate(int id) { AddInstruction(CreateSimulate(id)); }
  void AssignCommonDominator(HBasicBlock* other);

  // Add the inlined function exit sequence, adding an HLeaveInlined
  // instruction and updating the bailout environment.
  void AddLeaveInlined(HValue* return_value, HBasicBlock* target);

  // If a target block is tagged as an inline function return, all
  // predecessors should contain the inlined exit sequence:
  //
  // LeaveInlined
  // Simulate (caller's environment)
  // Goto (target block)
  bool IsInlineReturnTarget() const { return is_inline_return_target_; }
  void MarkAsInlineReturnTarget() { is_inline_return_target_ = true; }

  Handle<Object> cond() { return cond_; }
  void set_cond(Handle<Object> value) { cond_ = value; }

#ifdef DEBUG
  void Verify();
#endif

 private:
  void RegisterPredecessor(HBasicBlock* pred);
  void AddDominatedBlock(HBasicBlock* block);

  HSimulate* CreateSimulate(int id);

  int block_id_;
  HGraph* graph_;
  ZoneList<HPhi*> phis_;
  HInstruction* first_;
  HInstruction* last_;  // Last non-control instruction of the block.
  HControlInstruction* end_;
  HLoopInformation* loop_information_;
  ZoneList<HBasicBlock*> predecessors_;
  HBasicBlock* dominator_;
  ZoneList<HBasicBlock*> dominated_blocks_;
  HEnvironment* last_environment_;
  // Outgoing parameter count at block exit, set during lithium translation.
  int argument_count_;
  // Instruction indices into the lithium code stream.
  int first_instruction_index_;
  int last_instruction_index_;
  ZoneList<int> deleted_phis_;
  SetOncePointer<HBasicBlock> parent_loop_header_;
  bool is_inline_return_target_;
  Handle<Object> cond_;
};


class HLoopInformation: public ZoneObject {
 public:
  explicit HLoopInformation(HBasicBlock* loop_header)
      : back_edges_(4), loop_header_(loop_header), blocks_(8) {
    blocks_.Add(loop_header);
  }
  virtual ~HLoopInformation() {}

  const ZoneList<HBasicBlock*>* back_edges() const { return &back_edges_; }
  const ZoneList<HBasicBlock*>* blocks() const { return &blocks_; }
  HBasicBlock* loop_header() const { return loop_header_; }
  HBasicBlock* GetLastBackEdge() const;
  void RegisterBackEdge(HBasicBlock* block);

 private:
  void AddBlock(HBasicBlock* block);

  ZoneList<HBasicBlock*> back_edges_;
  HBasicBlock* loop_header_;
  ZoneList<HBasicBlock*> blocks_;
};


class HSubgraph: public ZoneObject {
 public:
  explicit HSubgraph(HGraph* graph)
      : graph_(graph),
        entry_block_(NULL),
        exit_block_(NULL),
        break_continue_info_(4) {
  }

  HGraph* graph() const { return graph_; }
  HEnvironment* environment() const {
    ASSERT(HasExit());
    return exit_block_->last_environment();
  }

  bool HasExit() const { return exit_block_ != NULL; }

  void PreProcessOsrEntry(IterationStatement* statement);

  void AppendOptional(HSubgraph* graph,
                      bool on_true_branch,
                      HValue* boolean_value);
  void AppendJoin(HSubgraph* then_graph, HSubgraph* else_graph, AstNode* node);
  void AppendWhile(HSubgraph* condition,
                   HSubgraph* body,
                   IterationStatement* statement,
                   HSubgraph* continue_subgraph,
                   HSubgraph* exit);
  void AppendDoWhile(HSubgraph* body,
                     IterationStatement* statement,
                     HSubgraph* go_back,
                     HSubgraph* exit);
  void AppendEndless(HSubgraph* body, IterationStatement* statement);
  void Append(HSubgraph* next, BreakableStatement* statement);
  void ResolveContinue(IterationStatement* statement);
  HBasicBlock* BundleBreak(BreakableStatement* statement);
  HBasicBlock* BundleContinue(IterationStatement* statement);
  HBasicBlock* BundleBreakContinue(BreakableStatement* statement,
                                   bool is_continue,
                                   int join_id);
  HBasicBlock* JoinBlocks(HBasicBlock* a, HBasicBlock* b, int id);

  void FinishExit(HControlInstruction* instruction);
  void FinishBreakContinue(BreakableStatement* target, bool is_continue);
  void Initialize(HBasicBlock* block) {
    ASSERT(entry_block_ == NULL);
    entry_block_ = block;
    exit_block_ = block;
  }
  HBasicBlock* entry_block() const { return entry_block_; }
  HBasicBlock* exit_block() const { return exit_block_; }
  void set_exit_block(HBasicBlock* block) {
    exit_block_ = block;
  }

  void ConnectExitTo(HBasicBlock* other, bool include_stack_check = false) {
    if (HasExit()) {
      exit_block()->Goto(other, include_stack_check);
    }
  }

  void AddBreakContinueInfo(HSubgraph* other) {
    break_continue_info_.AddAll(other->break_continue_info_);
  }

 protected:
  class BreakContinueInfo: public ZoneObject {
   public:
    BreakContinueInfo(BreakableStatement* target, HBasicBlock* block,
                      bool is_continue)
      : target_(target), block_(block), continue_(is_continue) {}
    BreakableStatement* target() const { return target_; }
    HBasicBlock* block() const { return block_; }
    bool is_continue() const { return continue_; }
    bool IsResolved() const { return block_ == NULL; }
    void Resolve() { block_ = NULL; }

   private:
    BreakableStatement* target_;
    HBasicBlock* block_;
    bool continue_;
  };

  const ZoneList<BreakContinueInfo*>* break_continue_info() const {
    return &break_continue_info_;
  }

  HGraph* graph_;  // The graph this is a subgraph of.
  HBasicBlock* entry_block_;
  HBasicBlock* exit_block_;

 private:
  ZoneList<BreakContinueInfo*> break_continue_info_;
};


class HGraph: public HSubgraph {
 public:
  explicit HGraph(CompilationInfo* info);

  CompilationInfo* info() const { return info_; }
  const ZoneList<HBasicBlock*>* blocks() const { return &blocks_; }
  const ZoneList<HPhi*>* phi_list() const { return phi_list_; }
  Handle<String> debug_name() const { return info_->function()->debug_name(); }
  HEnvironment* start_environment() const { return start_environment_; }

  void InitializeInferredTypes();
  void InsertTypeConversions();
  void InsertRepresentationChanges();
  bool ProcessArgumentsObject();
  void EliminateRedundantPhis();
  void Canonicalize();
  void OrderBlocks();
  void AssignDominators();

  // Returns false if there are phi-uses of the arguments-object
  // which are not supported by the optimizing compiler.
  bool CollectPhis();

  Handle<Code> Compile();

  void set_undefined_constant(HConstant* constant) {
    undefined_constant_.set(constant);
  }
  HConstant* GetConstantUndefined() const { return undefined_constant_.get(); }
  HConstant* GetConstant1();
  HConstant* GetConstantMinus1();
  HConstant* GetConstantTrue();
  HConstant* GetConstantFalse();

  HBasicBlock* CreateBasicBlock();
  HArgumentsObject* GetArgumentsObject() const {
    return arguments_object_.get();
  }
  bool HasArgumentsObject() const { return arguments_object_.is_set(); }

  void SetArgumentsObject(HArgumentsObject* object) {
    arguments_object_.set(object);
  }

  // True iff. we are compiling for OSR and the statement is the entry.
  bool HasOsrEntryAt(IterationStatement* statement);

  int GetMaximumValueID() const { return values_.length(); }
  int GetNextBlockID() { return next_block_id_++; }
  int GetNextValueID(HValue* value) {
    values_.Add(value);
    return values_.length() - 1;
  }
  HValue* LookupValue(int id) const {
    if (id >= 0 && id < values_.length()) return values_[id];
    return NULL;
  }

#ifdef DEBUG
  void Verify() const;
#endif

 private:
  void Postorder(HBasicBlock* block,
                 BitVector* visited,
                 ZoneList<HBasicBlock*>* order,
                 HBasicBlock* loop_header);
  void PostorderLoopBlocks(HLoopInformation* loop,
                           BitVector* visited,
                           ZoneList<HBasicBlock*>* order,
                           HBasicBlock* loop_header);
  HConstant* GetConstant(SetOncePointer<HConstant>* pointer,
                         Object* value);

  void InsertTypeConversions(HInstruction* instr);
  void PropagateMinusZeroChecks(HValue* value, BitVector* visited);
  void InsertRepresentationChangeForUse(HValue* value,
                                        HValue* use,
                                        Representation to,
                                        bool truncating);
  void InsertRepresentationChanges(HValue* current);
  void InferTypes(ZoneList<HValue*>* worklist);
  void InitializeInferredTypes(int from_inclusive, int to_inclusive);
  void CheckForBackEdge(HBasicBlock* block, HBasicBlock* successor);

  int next_block_id_;
  CompilationInfo* info_;
  HEnvironment* start_environment_;
  ZoneList<HBasicBlock*> blocks_;
  ZoneList<HValue*> values_;
  ZoneList<HPhi*>* phi_list_;
  SetOncePointer<HConstant> undefined_constant_;
  SetOncePointer<HConstant> constant_1_;
  SetOncePointer<HConstant> constant_minus1_;
  SetOncePointer<HConstant> constant_true_;
  SetOncePointer<HConstant> constant_false_;
  SetOncePointer<HArgumentsObject> arguments_object_;

  friend class HSubgraph;

  DISALLOW_COPY_AND_ASSIGN(HGraph);
};


class HEnvironment: public ZoneObject {
 public:
  HEnvironment(HEnvironment* outer,
               Scope* scope,
               Handle<JSFunction> closure);

  // Simple accessors.
  Handle<JSFunction> closure() const { return closure_; }
  const ZoneList<HValue*>* values() const { return &values_; }
  const ZoneList<int>* assigned_variables() const {
    return &assigned_variables_;
  }
  int parameter_count() const { return parameter_count_; }
  int local_count() const { return local_count_; }
  HEnvironment* outer() const { return outer_; }
  int pop_count() const { return pop_count_; }
  int push_count() const { return push_count_; }

  int ast_id() const { return ast_id_; }
  void set_ast_id(int id) { ast_id_ = id; }

  int length() const { return values_.length(); }

  void Bind(Variable* variable, HValue* value) {
    Bind(IndexFor(variable), value);
  }

  void Bind(int index, HValue* value);

  HValue* Lookup(Variable* variable) const {
    return Lookup(IndexFor(variable));
  }

  HValue* Lookup(int index) const {
    HValue* result = values_[index];
    ASSERT(result != NULL);
    return result;
  }

  void Push(HValue* value) {
    ASSERT(value != NULL);
    ++push_count_;
    values_.Add(value);
  }

  HValue* Pop() {
    ASSERT(!ExpressionStackIsEmpty());
    if (push_count_ > 0) {
      --push_count_;
    } else {
      ++pop_count_;
    }
    return values_.RemoveLast();
  }

  void Drop(int count);

  HValue* Top() const { return ExpressionStackAt(0); }

  HValue* ExpressionStackAt(int index_from_top) const {
    int index = length() - index_from_top - 1;
    ASSERT(HasExpressionAt(index));
    return values_[index];
  }

  void SetExpressionStackAt(int index_from_top, HValue* value);

  HEnvironment* Copy() const;
  HEnvironment* CopyWithoutHistory() const;
  HEnvironment* CopyAsLoopHeader(HBasicBlock* block) const;

  // Create an "inlined version" of this environment, where the original
  // environment is the outer environment but the top expression stack
  // elements are moved to an inner environment as parameters. If
  // is_speculative, the argument values are expected to be PushArgument
  // instructions, otherwise they are the actual values.
  HEnvironment* CopyForInlining(Handle<JSFunction> target,
                                FunctionLiteral* function,
                                bool is_speculative,
                                HConstant* undefined) const;

  void AddIncomingEdge(HBasicBlock* block, HEnvironment* other);

  void ClearHistory() {
    pop_count_ = 0;
    push_count_ = 0;
    assigned_variables_.Clear();
  }

  void SetValueAt(int index, HValue* value) {
    ASSERT(index < length());
    values_[index] = value;
  }

  void PrintTo(StringStream* stream);
  void PrintToStd();

 private:
  explicit HEnvironment(const HEnvironment* other);

  // True if index is included in the expression stack part of the environment.
  bool HasExpressionAt(int index) const;

  bool ExpressionStackIsEmpty() const;

  void Initialize(int parameter_count, int local_count, int stack_height);
  void Initialize(const HEnvironment* other);

  // Map a variable to an environment index.  Parameter indices are shifted
  // by 1 (receiver is parameter index -1 but environment index 0).
  // Stack-allocated local indices are shifted by the number of parameters.
  int IndexFor(Variable* variable) const {
    Slot* slot = variable->AsSlot();
    ASSERT(slot != NULL && slot->IsStackAllocated());
    int shift = (slot->type() == Slot::PARAMETER) ? 1 : parameter_count_;
    return slot->index() + shift;
  }

  Handle<JSFunction> closure_;
  // Value array [parameters] [locals] [temporaries].
  ZoneList<HValue*> values_;
  ZoneList<int> assigned_variables_;
  int parameter_count_;
  int local_count_;
  HEnvironment* outer_;
  int pop_count_;
  int push_count_;
  int ast_id_;
};


class HGraphBuilder;

class AstContext {
 public:
  bool IsEffect() const { return kind_ == Expression::kEffect; }
  bool IsValue() const { return kind_ == Expression::kValue; }
  bool IsTest() const { return kind_ == Expression::kTest; }

  // 'Fill' this context with a hydrogen value.  The value is assumed to
  // have already been inserted in the instruction stream (or not need to
  // be, e.g., HPhi).  Call this function in tail position in the Visit
  // functions for expressions.
  virtual void ReturnValue(HValue* value) = 0;

  // Add a hydrogen instruction to the instruction stream (recording an
  // environment simulation if necessary) and then fill this context with
  // the instruction as value.
  virtual void ReturnInstruction(HInstruction* instr, int ast_id) = 0;

 protected:
  AstContext(HGraphBuilder* owner, Expression::Context kind);
  virtual ~AstContext();

  HGraphBuilder* owner() const { return owner_; }

  // We want to be able to assert, in a context-specific way, that the stack
  // height makes sense when the context is filled.
#ifdef DEBUG
  int original_length_;
#endif

 private:
  HGraphBuilder* owner_;
  Expression::Context kind_;
  AstContext* outer_;
};


class EffectContext: public AstContext {
 public:
  explicit EffectContext(HGraphBuilder* owner)
      : AstContext(owner, Expression::kEffect) {
  }
  virtual ~EffectContext();

  virtual void ReturnValue(HValue* value);
  virtual void ReturnInstruction(HInstruction* instr, int ast_id);
};


class ValueContext: public AstContext {
 public:
  explicit ValueContext(HGraphBuilder* owner)
      : AstContext(owner, Expression::kValue) {
  }
  virtual ~ValueContext();

  virtual void ReturnValue(HValue* value);
  virtual void ReturnInstruction(HInstruction* instr, int ast_id);
};


class TestContext: public AstContext {
 public:
  TestContext(HGraphBuilder* owner,
              HBasicBlock* if_true,
              HBasicBlock* if_false)
      : AstContext(owner, Expression::kTest),
        if_true_(if_true),
        if_false_(if_false) {
  }

  virtual void ReturnValue(HValue* value);
  virtual void ReturnInstruction(HInstruction* instr, int ast_id);

  static TestContext* cast(AstContext* context) {
    ASSERT(context->IsTest());
    return reinterpret_cast<TestContext*>(context);
  }

  HBasicBlock* if_true() const { return if_true_; }
  HBasicBlock* if_false() const { return if_false_; }

 private:
  // Build the shared core part of the translation unpacking a value into
  // control flow.
  void BuildBranch(HValue* value);

  HBasicBlock* if_true_;
  HBasicBlock* if_false_;
};


class HGraphBuilder: public AstVisitor {
 public:
  explicit HGraphBuilder(TypeFeedbackOracle* oracle)
      : oracle_(oracle),
        graph_(NULL),
        current_subgraph_(NULL),
        peeled_statement_(NULL),
        ast_context_(NULL),
        call_context_(NULL),
        function_return_(NULL),
        inlined_count_(0) { }

  HGraph* CreateGraph(CompilationInfo* info);

  // Simple accessors.
  HGraph* graph() const { return graph_; }
  HSubgraph* subgraph() const { return current_subgraph_; }

  HEnvironment* environment() const { return subgraph()->environment(); }
  HBasicBlock* CurrentBlock() const { return subgraph()->exit_block(); }

  // Adding instructions.
  HInstruction* AddInstruction(HInstruction* instr);
  void AddSimulate(int id);

  // Bailout environment manipulation.
  void Push(HValue* value) { environment()->Push(value); }
  HValue* Pop() { return environment()->Pop(); }

 private:
  // Type of a member function that generates inline code for a native function.
  typedef void (HGraphBuilder::*InlineFunctionGenerator)(int argument_count,
                                                         int ast_id);

  // Forward declarations for inner scope classes.
  class SubgraphScope;

  static const InlineFunctionGenerator kInlineFunctionGenerators[];

  static const int kMaxCallPolymorphism = 4;
  static const int kMaxLoadPolymorphism = 4;
  static const int kMaxStorePolymorphism = 4;

  static const int kMaxInlinedNodes = 196;
  static const int kMaxInlinedSize = 196;
  static const int kMaxSourceSize = 600;

  // Simple accessors.
  TypeFeedbackOracle* oracle() const { return oracle_; }
  AstContext* ast_context() const { return ast_context_; }
  void set_ast_context(AstContext* context) { ast_context_ = context; }
  AstContext* call_context() const { return call_context_; }
  HBasicBlock* function_return() const { return function_return_; }

  // Generators for inline runtime functions.
#define INLINE_FUNCTION_GENERATOR_DECLARATION(Name, argc, ressize)      \
  void Generate##Name(int argument_count, int ast_id);

  INLINE_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_DECLARATION)
  INLINE_RUNTIME_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_DECLARATION)
#undef INLINE_FUNCTION_GENERATOR_DECLARATION

  void Bailout(const char* reason);

  void AppendPeeledWhile(IterationStatement* stmt,
                         HSubgraph* cond_graph,
                         HSubgraph* body_graph,
                         HSubgraph* exit_graph);

  void AddToSubgraph(HSubgraph* graph, ZoneList<Statement*>* stmts);
  void AddToSubgraph(HSubgraph* graph, Statement* stmt);
  void AddToSubgraph(HSubgraph* graph, Expression* expr);

  HValue* Top() const { return environment()->Top(); }
  void Drop(int n) { environment()->Drop(n); }
  void Bind(Variable* var, HValue* value) { environment()->Bind(var, value); }

  void VisitForValue(Expression* expr);
  void VisitForEffect(Expression* expr);
  void VisitForControl(Expression* expr,
                       HBasicBlock* true_block,
                       HBasicBlock* false_block);

  // Visit an argument and wrap it in a PushArgument instruction.
  HValue* VisitArgument(Expression* expr);
  void VisitArgumentList(ZoneList<Expression*>* arguments);

  void AddPhi(HPhi* phi);

  void PushAndAdd(HInstruction* instr);

  void PushArgumentsForStubCall(int argument_count);

  // Remove the arguments from the bailout environment and emit instructions
  // to push them as outgoing parameters.
  void ProcessCall(HCall* call);

  void AssumeRepresentation(HValue* value, Representation r);
  static Representation ToRepresentation(TypeInfo info);

  void SetupScope(Scope* scope);
  virtual void VisitStatements(ZoneList<Statement*>* statements);

#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  bool ShouldPeel(HSubgraph* cond, HSubgraph* body);

  HBasicBlock* CreateBasicBlock(HEnvironment* env);
  HSubgraph* CreateEmptySubgraph();
  HSubgraph* CreateGotoSubgraph(HEnvironment* env);
  HSubgraph* CreateBranchSubgraph(HEnvironment* env);
  HSubgraph* CreateLoopHeaderSubgraph(HEnvironment* env);
  HSubgraph* CreateInlinedSubgraph(HEnvironment* outer,
                                   Handle<JSFunction> target,
                                   FunctionLiteral* function);

  // Helpers for flow graph construction.
  void LookupGlobalPropertyCell(Variable* var,
                                LookupResult* lookup,
                                bool is_store);

  bool TryArgumentsAccess(Property* expr);
  bool TryCallApply(Call* expr);
  bool TryInline(Call* expr);
  bool TryMathFunctionInline(Call* expr);
  void TraceInline(Handle<JSFunction> target, bool result);

  void HandleGlobalVariableAssignment(Variable* var,
                                      HValue* value,
                                      int position,
                                      int ast_id);

  void HandlePropertyAssignment(Assignment* expr);
  void HandleCompoundAssignment(Assignment* expr);
  void HandlePolymorphicLoadNamedField(Property* expr,
                                       HValue* object,
                                       ZoneMapList* types,
                                       Handle<String> name);
  void HandlePolymorphicStoreNamedField(Assignment* expr,
                                        HValue* object,
                                        HValue* value,
                                        ZoneMapList* types,
                                        Handle<String> name);
  void HandlePolymorphicCallNamed(Call* expr,
                                  HValue* receiver,
                                  ZoneMapList* types,
                                  Handle<String> name);

  HInstruction* BuildBinaryOperation(BinaryOperation* expr,
                                     HValue* left,
                                     HValue* right);
  HInstruction* BuildIncrement(HValue* value, bool increment);
  HLoadNamedField* BuildLoadNamedField(HValue* object,
                                       Property* expr,
                                       Handle<Map> type,
                                       LookupResult* result,
                                       bool smi_and_map_check);
  HInstruction* BuildLoadNamedGeneric(HValue* object, Property* expr);
  HInstruction* BuildLoadKeyedFastElement(HValue* object,
                                          HValue* key,
                                          Property* expr);
  HInstruction* BuildLoadKeyedGeneric(HValue* object,
                                      HValue* key);

  HInstruction* BuildLoadNamed(HValue* object,
                               Property* prop,
                               Handle<Map> map,
                               Handle<String> name);
  HInstruction* BuildStoreNamed(HValue* object,
                                HValue* value,
                                Expression* expr);
  HInstruction* BuildStoreNamedField(HValue* object,
                                     Handle<String> name,
                                     HValue* value,
                                     Handle<Map> type,
                                     LookupResult* lookup,
                                     bool smi_and_map_check);
  HInstruction* BuildStoreNamedGeneric(HValue* object,
                                       Handle<String> name,
                                       HValue* value);
  HInstruction* BuildStoreKeyedGeneric(HValue* object,
                                       HValue* key,
                                       HValue* value);

  HInstruction* BuildStoreKeyedFastElement(HValue* object,
                                           HValue* key,
                                           HValue* val,
                                           Expression* expr);

  HCompare* BuildSwitchCompare(HSubgraph* subgraph,
                               HValue* switch_value,
                               CaseClause* clause);

  void AddCheckConstantFunction(Call* expr,
                                HValue* receiver,
                                Handle<Map> receiver_map,
                                bool smi_and_map_check);


  HBasicBlock* BuildTypeSwitch(ZoneMapList* maps,
                               ZoneList<HSubgraph*>* subgraphs,
                               HValue* receiver,
                               int join_id);

  TypeFeedbackOracle* oracle_;
  HGraph* graph_;
  HSubgraph* current_subgraph_;
  IterationStatement* peeled_statement_;
  // Expression context of the currently visited subexpression. NULL when
  // visiting statements.
  AstContext* ast_context_;

  // During function inlining, expression context of the call being
  // inlined. NULL when not inlining.
  AstContext* call_context_;

  // When inlining a call in an effect or value context, the return
  // block. NULL otherwise. When inlining a call in a test context, there
  // are a pair of target blocks in the call context.
  HBasicBlock* function_return_;

  int inlined_count_;

  friend class AstContext;  // Pushes and pops the AST context stack.

  DISALLOW_COPY_AND_ASSIGN(HGraphBuilder);
};


class HValueMap: public ZoneObject {
 public:
  HValueMap()
      : array_size_(0),
        lists_size_(0),
        count_(0),
        present_flags_(0),
        array_(NULL),
        lists_(NULL),
        free_list_head_(kNil) {
    ResizeLists(kInitialSize);
    Resize(kInitialSize);
  }

  void Kill(int flags);

  void Add(HValue* value) {
    present_flags_ |= value->flags();
    Insert(value);
  }

  HValue* Lookup(HValue* value) const;
  HValueMap* Copy() const { return new HValueMap(this); }

 private:
  // A linked list of HValue* values.  Stored in arrays.
  struct HValueMapListElement {
    HValue* value;
    int next;  // Index in the array of the next list element.
  };
  static const int kNil = -1;  // The end of a linked list

  // Must be a power of 2.
  static const int kInitialSize = 16;

  explicit HValueMap(const HValueMap* other);

  void Resize(int new_size);
  void ResizeLists(int new_size);
  void Insert(HValue* value);
  uint32_t Bound(uint32_t value) const { return value & (array_size_ - 1); }

  int array_size_;
  int lists_size_;
  int count_;  // The number of values stored in the HValueMap.
  int present_flags_;  // All flags that are in any value in the HValueMap.
  HValueMapListElement* array_;  // Primary store - contains the first value
  // with a given hash.  Colliding elements are stored in linked lists.
  HValueMapListElement* lists_;  // The linked lists containing hash collisions.
  int free_list_head_;  // Unused elements in lists_ are on the free list.
};


class HStatistics: public Malloced {
 public:
  void Print();
  void SaveTiming(const char* name, int64_t ticks, unsigned size);
  static HStatistics* Instance() {
    static SetOncePointer<HStatistics> instance;
    if (!instance.is_set()) {
      instance.set(new HStatistics());
    }
    return instance.get();
  }

 private:

  HStatistics()
      : timing_(5),
        names_(5),
        sizes_(5),
        total_(0),
        total_size_(0),
        full_code_gen_(0) { }

  List<int64_t> timing_;
  List<const char*> names_;
  List<unsigned> sizes_;
  int64_t total_;
  unsigned total_size_;
  int64_t full_code_gen_;
};


class HPhase BASE_EMBEDDED {
 public:
  static const char* const kFullCodeGen;
  static const char* const kTotal;

  explicit HPhase(const char* name) { Begin(name, NULL, NULL, NULL); }
  HPhase(const char* name, HGraph* graph) {
    Begin(name, graph, NULL, NULL);
  }
  HPhase(const char* name, LChunk* chunk) {
    Begin(name, NULL, chunk, NULL);
  }
  HPhase(const char* name, LAllocator* allocator) {
    Begin(name, NULL, NULL, allocator);
  }

  ~HPhase() {
    End();
  }

 private:
  void Begin(const char* name,
             HGraph* graph,
             LChunk* chunk,
             LAllocator* allocator);
  void End() const;

  int64_t start_;
  const char* name_;
  HGraph* graph_;
  LChunk* chunk_;
  LAllocator* allocator_;
  unsigned start_allocation_size_;
};


class HTracer: public Malloced {
 public:
  void TraceCompilation(FunctionLiteral* function);
  void TraceHydrogen(const char* name, HGraph* graph);
  void TraceLithium(const char* name, LChunk* chunk);
  void TraceLiveRanges(const char* name, LAllocator* allocator);

  static HTracer* Instance() {
    static SetOncePointer<HTracer> instance;
    if (!instance.is_set()) {
      instance.set(new HTracer("hydrogen.cfg"));
    }
    return instance.get();
  }

 private:
  class Tag BASE_EMBEDDED {
   public:
    Tag(HTracer* tracer, const char* name) {
      name_ = name;
      tracer_ = tracer;
      tracer->PrintIndent();
      tracer->trace_.Add("begin_%s\n", name);
      tracer->indent_++;
    }

    ~Tag() {
      tracer_->indent_--;
      tracer_->PrintIndent();
      tracer_->trace_.Add("end_%s\n", name_);
      ASSERT(tracer_->indent_ >= 0);
      tracer_->FlushToFile();
    }

   private:
    HTracer* tracer_;
    const char* name_;
  };

  explicit HTracer(const char* filename)
      : filename_(filename), trace_(&string_allocator_), indent_(0) {
    WriteChars(filename, "", 0, false);
  }

  void TraceLiveRange(LiveRange* range, const char* type);
  void Trace(const char* name, HGraph* graph, LChunk* chunk);
  void FlushToFile();

  void PrintEmptyProperty(const char* name) {
    PrintIndent();
    trace_.Add("%s\n", name);
  }

  void PrintStringProperty(const char* name, const char* value) {
    PrintIndent();
    trace_.Add("%s \"%s\"\n", name, value);
  }

  void PrintLongProperty(const char* name, int64_t value) {
    PrintIndent();
    trace_.Add("%s %d000\n", name, static_cast<int>(value / 1000));
  }

  void PrintBlockProperty(const char* name, int block_id) {
    PrintIndent();
    trace_.Add("%s \"B%d\"\n", name, block_id);
  }

  void PrintBlockProperty(const char* name, int block_id1, int block_id2) {
    PrintIndent();
    trace_.Add("%s \"B%d\" \"B%d\"\n", name, block_id1, block_id2);
  }

  void PrintIntProperty(const char* name, int value) {
    PrintIndent();
    trace_.Add("%s %d\n", name, value);
  }

  void PrintIndent() {
    for (int i = 0; i < indent_; i++) {
      trace_.Add("  ");
    }
  }

  const char* filename_;
  HeapStringAllocator string_allocator_;
  StringStream trace_;
  int indent_;
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_H_
