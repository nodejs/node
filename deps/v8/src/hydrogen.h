// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "allocation.h"
#include "ast.h"
#include "compiler.h"
#include "hydrogen-instructions.h"
#include "type-info.h"
#include "zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class BitVector;
class FunctionState;
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
  HInstruction* last() const { return last_; }
  void set_last(HInstruction* instr) { last_ = instr; }
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
  int LoopNestingDepth() const;

  void SetInitialEnvironment(HEnvironment* env);
  void ClearEnvironment() { last_environment_ = NULL; }
  bool HasEnvironment() const { return last_environment_ != NULL; }
  void UpdateEnvironment(HEnvironment* env) { last_environment_ = env; }
  HBasicBlock* parent_loop_header() const { return parent_loop_header_; }

  void set_parent_loop_header(HBasicBlock* block) {
    ASSERT(parent_loop_header_ == NULL);
    parent_loop_header_ = block;
  }

  bool HasParentLoopHeader() const { return parent_loop_header_ != NULL; }

  void SetJoinId(int ast_id);

  void Finish(HControlInstruction* last);
  void FinishExit(HControlInstruction* instruction);
  void Goto(HBasicBlock* block, FunctionState* state = NULL);

  int PredecessorIndexOf(HBasicBlock* predecessor) const;
  void AddSimulate(int ast_id) { AddInstruction(CreateSimulate(ast_id)); }
  void AssignCommonDominator(HBasicBlock* other);
  void AssignLoopSuccessorDominators();

  void FinishExitWithDeoptimization(HDeoptimize::UseEnvironment has_uses) {
    FinishExit(CreateDeoptimize(has_uses));
  }

  // Add the inlined function exit sequence, adding an HLeaveInlined
  // instruction and updating the bailout environment.
  void AddLeaveInlined(HValue* return_value,
                       HBasicBlock* target,
                       FunctionState* state = NULL);

  // If a target block is tagged as an inline function return, all
  // predecessors should contain the inlined exit sequence:
  //
  // LeaveInlined
  // Simulate (caller's environment)
  // Goto (target block)
  bool IsInlineReturnTarget() const { return is_inline_return_target_; }
  void MarkAsInlineReturnTarget() { is_inline_return_target_ = true; }

  bool IsDeoptimizing() const { return is_deoptimizing_; }
  void MarkAsDeoptimizing() { is_deoptimizing_ = true; }

  bool IsLoopSuccessorDominator() const {
    return dominates_loop_successors_;
  }
  void MarkAsLoopSuccessorDominator() {
    dominates_loop_successors_ = true;
  }

  inline Zone* zone();

#ifdef DEBUG
  void Verify();
#endif

 private:
  void RegisterPredecessor(HBasicBlock* pred);
  void AddDominatedBlock(HBasicBlock* block);

  HSimulate* CreateSimulate(int ast_id);
  HDeoptimize* CreateDeoptimize(HDeoptimize::UseEnvironment has_uses);

  int block_id_;
  HGraph* graph_;
  ZoneList<HPhi*> phis_;
  HInstruction* first_;
  HInstruction* last_;
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
  HBasicBlock* parent_loop_header_;
  bool is_inline_return_target_;
  bool is_deoptimizing_;
  bool dominates_loop_successors_;
};


class HPredecessorIterator BASE_EMBEDDED {
 public:
  explicit HPredecessorIterator(HBasicBlock* block)
      : predecessor_list_(block->predecessors()), current_(0) { }

  bool Done() { return current_ >= predecessor_list_->length(); }
  HBasicBlock* Current() { return predecessor_list_->at(current_); }
  void Advance() { current_++; }

 private:
  const ZoneList<HBasicBlock*>* predecessor_list_;
  int current_;
};


class HLoopInformation: public ZoneObject {
 public:
  explicit HLoopInformation(HBasicBlock* loop_header)
      : back_edges_(4),
        loop_header_(loop_header),
        blocks_(8),
        stack_check_(NULL) {
    blocks_.Add(loop_header);
  }
  virtual ~HLoopInformation() {}

  const ZoneList<HBasicBlock*>* back_edges() const { return &back_edges_; }
  const ZoneList<HBasicBlock*>* blocks() const { return &blocks_; }
  HBasicBlock* loop_header() const { return loop_header_; }
  HBasicBlock* GetLastBackEdge() const;
  void RegisterBackEdge(HBasicBlock* block);

  HStackCheck* stack_check() const { return stack_check_; }
  void set_stack_check(HStackCheck* stack_check) {
    stack_check_ = stack_check;
  }

 private:
  void AddBlock(HBasicBlock* block);

  ZoneList<HBasicBlock*> back_edges_;
  HBasicBlock* loop_header_;
  ZoneList<HBasicBlock*> blocks_;
  HStackCheck* stack_check_;
};

class BoundsCheckTable;
class HGraph: public ZoneObject {
 public:
  explicit HGraph(CompilationInfo* info);

  Isolate* isolate() { return isolate_; }
  Zone* zone() { return isolate_->zone(); }

  const ZoneList<HBasicBlock*>* blocks() const { return &blocks_; }
  const ZoneList<HPhi*>* phi_list() const { return phi_list_; }
  HBasicBlock* entry_block() const { return entry_block_; }
  HEnvironment* start_environment() const { return start_environment_; }

  void InitializeInferredTypes();
  void InsertTypeConversions();
  void InsertRepresentationChanges();
  void MarkDeoptimizeOnUndefined();
  void ComputeMinusZeroChecks();
  bool ProcessArgumentsObject();
  void EliminateRedundantPhis();
  void EliminateUnreachablePhis();
  void Canonicalize();
  void OrderBlocks();
  void AssignDominators();
  void ReplaceCheckedValues();
  void EliminateRedundantBoundsChecks();
  void DehoistSimpleArrayIndexComputations();
  void PropagateDeoptimizingMark();

  // Returns false if there are phi-uses of the arguments-object
  // which are not supported by the optimizing compiler.
  bool CheckArgumentsPhiUses();

  // Returns false if there are phi-uses of an uninitialized const
  // which are not supported by the optimizing compiler.
  bool CheckConstPhiUses();

  void CollectPhis();

  Handle<Code> Compile(CompilationInfo* info);

  void set_undefined_constant(HConstant* constant) {
    undefined_constant_.set(constant);
  }
  HConstant* GetConstantUndefined() const { return undefined_constant_.get(); }
  HConstant* GetConstant1();
  HConstant* GetConstantMinus1();
  HConstant* GetConstantTrue();
  HConstant* GetConstantFalse();
  HConstant* GetConstantHole();

  HBasicBlock* CreateBasicBlock();
  HArgumentsObject* GetArgumentsObject() const {
    return arguments_object_.get();
  }

  void SetArgumentsObject(HArgumentsObject* object) {
    arguments_object_.set(object);
  }

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
  void Verify(bool do_full_verify) const;
#endif

  bool has_osr_loop_entry() {
    return osr_loop_entry_.is_set();
  }

  HBasicBlock* osr_loop_entry() {
    return osr_loop_entry_.get();
  }

  void set_osr_loop_entry(HBasicBlock* entry) {
    osr_loop_entry_.set(entry);
  }

  ZoneList<HUnknownOSRValue*>* osr_values() {
    return osr_values_.get();
  }

  void set_osr_values(ZoneList<HUnknownOSRValue*>* values) {
    osr_values_.set(values);
  }

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

  void MarkAsDeoptimizingRecursively(HBasicBlock* block);
  void InsertTypeConversions(HInstruction* instr);
  void PropagateMinusZeroChecks(HValue* value, BitVector* visited);
  void RecursivelyMarkPhiDeoptimizeOnUndefined(HPhi* phi);
  void InsertRepresentationChangeForUse(HValue* value,
                                        HValue* use_value,
                                        int use_index,
                                        Representation to);
  void InsertRepresentationChangesForValue(HValue* value);
  void InferTypes(ZoneList<HValue*>* worklist);
  void InitializeInferredTypes(int from_inclusive, int to_inclusive);
  void CheckForBackEdge(HBasicBlock* block, HBasicBlock* successor);
  void EliminateRedundantBoundsChecks(HBasicBlock* bb, BoundsCheckTable* table);

  Isolate* isolate_;
  int next_block_id_;
  HBasicBlock* entry_block_;
  HEnvironment* start_environment_;
  ZoneList<HBasicBlock*> blocks_;
  ZoneList<HValue*> values_;
  ZoneList<HPhi*>* phi_list_;
  SetOncePointer<HConstant> undefined_constant_;
  SetOncePointer<HConstant> constant_1_;
  SetOncePointer<HConstant> constant_minus1_;
  SetOncePointer<HConstant> constant_true_;
  SetOncePointer<HConstant> constant_false_;
  SetOncePointer<HConstant> constant_hole_;
  SetOncePointer<HArgumentsObject> arguments_object_;

  SetOncePointer<HBasicBlock> osr_loop_entry_;
  SetOncePointer<ZoneList<HUnknownOSRValue*> > osr_values_;

  DISALLOW_COPY_AND_ASSIGN(HGraph);
};


Zone* HBasicBlock::zone() { return graph_->zone(); }


// Type of stack frame an environment might refer to.
enum FrameType { JS_FUNCTION, JS_CONSTRUCT, ARGUMENTS_ADAPTOR };


class HEnvironment: public ZoneObject {
 public:
  HEnvironment(HEnvironment* outer,
               Scope* scope,
               Handle<JSFunction> closure);

  HEnvironment* DiscardInlined(bool drop_extra) {
    HEnvironment* outer = outer_;
    while (outer->frame_type() != JS_FUNCTION) outer = outer->outer_;
    if (drop_extra) outer->Drop(1);
    return outer;
  }

  HEnvironment* arguments_environment() {
    return outer()->frame_type() == ARGUMENTS_ADAPTOR ? outer() : this;
  }

  // Simple accessors.
  Handle<JSFunction> closure() const { return closure_; }
  const ZoneList<HValue*>* values() const { return &values_; }
  const ZoneList<int>* assigned_variables() const {
    return &assigned_variables_;
  }
  FrameType frame_type() const { return frame_type_; }
  int parameter_count() const { return parameter_count_; }
  int specials_count() const { return specials_count_; }
  int local_count() const { return local_count_; }
  HEnvironment* outer() const { return outer_; }
  int pop_count() const { return pop_count_; }
  int push_count() const { return push_count_; }

  int ast_id() const { return ast_id_; }
  void set_ast_id(int id) { ast_id_ = id; }

  int length() const { return values_.length(); }
  bool is_special_index(int i) const {
    return i >= parameter_count() && i < parameter_count() + specials_count();
  }

  int first_expression_index() const {
    return parameter_count() + specials_count() + local_count();
  }

  void Bind(Variable* variable, HValue* value) {
    Bind(IndexFor(variable), value);
  }

  void Bind(int index, HValue* value);

  void BindContext(HValue* value) {
    Bind(parameter_count(), value);
  }

  HValue* Lookup(Variable* variable) const {
    return Lookup(IndexFor(variable));
  }

  HValue* Lookup(int index) const {
    HValue* result = values_[index];
    ASSERT(result != NULL);
    return result;
  }

  HValue* LookupContext() const {
    // Return first special.
    return Lookup(parameter_count());
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

  bool ExpressionStackIsEmpty() const;

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
  // elements are moved to an inner environment as parameters.
  HEnvironment* CopyForInlining(Handle<JSFunction> target,
                                int arguments,
                                FunctionLiteral* function,
                                HConstant* undefined,
                                CallKind call_kind,
                                bool is_construct) const;

  void AddIncomingEdge(HBasicBlock* block, HEnvironment* other);

  void ClearHistory() {
    pop_count_ = 0;
    push_count_ = 0;
    assigned_variables_.Rewind(0);
  }

  void SetValueAt(int index, HValue* value) {
    ASSERT(index < length());
    values_[index] = value;
  }

  void PrintTo(StringStream* stream);
  void PrintToStd();

 private:
  explicit HEnvironment(const HEnvironment* other);

  HEnvironment(HEnvironment* outer,
               Handle<JSFunction> closure,
               FrameType frame_type,
               int arguments);

  // Create an artificial stub environment (e.g. for argument adaptor or
  // constructor stub).
  HEnvironment* CreateStubEnvironment(HEnvironment* outer,
                                      Handle<JSFunction> target,
                                      FrameType frame_type,
                                      int arguments) const;

  // True if index is included in the expression stack part of the environment.
  bool HasExpressionAt(int index) const;

  void Initialize(int parameter_count, int local_count, int stack_height);
  void Initialize(const HEnvironment* other);

  // Map a variable to an environment index.  Parameter indices are shifted
  // by 1 (receiver is parameter index -1 but environment index 0).
  // Stack-allocated local indices are shifted by the number of parameters.
  int IndexFor(Variable* variable) const {
    ASSERT(variable->IsStackAllocated());
    int shift = variable->IsParameter()
        ? 1
        : parameter_count_ + specials_count_;
    return variable->index() + shift;
  }

  Handle<JSFunction> closure_;
  // Value array [parameters] [specials] [locals] [temporaries].
  ZoneList<HValue*> values_;
  ZoneList<int> assigned_variables_;
  FrameType frame_type_;
  int parameter_count_;
  int specials_count_;
  int local_count_;
  HEnvironment* outer_;
  int pop_count_;
  int push_count_;
  int ast_id_;
};


class HGraphBuilder;

enum ArgumentsAllowedFlag {
  ARGUMENTS_NOT_ALLOWED,
  ARGUMENTS_ALLOWED
};

// This class is not BASE_EMBEDDED because our inlining implementation uses
// new and delete.
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

  // Finishes the current basic block and materialize a boolean for
  // value context, nothing for effect, generate a branch for test context.
  // Call this function in tail position in the Visit functions for
  // expressions.
  virtual void ReturnControl(HControlInstruction* instr, int ast_id) = 0;

  void set_for_typeof(bool for_typeof) { for_typeof_ = for_typeof; }
  bool is_for_typeof() { return for_typeof_; }

 protected:
  AstContext(HGraphBuilder* owner, Expression::Context kind);
  virtual ~AstContext();

  HGraphBuilder* owner() const { return owner_; }

  inline Zone* zone();

  // We want to be able to assert, in a context-specific way, that the stack
  // height makes sense when the context is filled.
#ifdef DEBUG
  int original_length_;
#endif

 private:
  HGraphBuilder* owner_;
  Expression::Context kind_;
  AstContext* outer_;
  bool for_typeof_;
};


class EffectContext: public AstContext {
 public:
  explicit EffectContext(HGraphBuilder* owner)
      : AstContext(owner, Expression::kEffect) {
  }
  virtual ~EffectContext();

  virtual void ReturnValue(HValue* value);
  virtual void ReturnInstruction(HInstruction* instr, int ast_id);
  virtual void ReturnControl(HControlInstruction* instr, int ast_id);
};


class ValueContext: public AstContext {
 public:
  explicit ValueContext(HGraphBuilder* owner, ArgumentsAllowedFlag flag)
      : AstContext(owner, Expression::kValue), flag_(flag) {
  }
  virtual ~ValueContext();

  virtual void ReturnValue(HValue* value);
  virtual void ReturnInstruction(HInstruction* instr, int ast_id);
  virtual void ReturnControl(HControlInstruction* instr, int ast_id);

  bool arguments_allowed() { return flag_ == ARGUMENTS_ALLOWED; }

 private:
  ArgumentsAllowedFlag flag_;
};


class TestContext: public AstContext {
 public:
  TestContext(HGraphBuilder* owner,
              Expression* condition,
              HBasicBlock* if_true,
              HBasicBlock* if_false)
      : AstContext(owner, Expression::kTest),
        condition_(condition),
        if_true_(if_true),
        if_false_(if_false) {
  }

  virtual void ReturnValue(HValue* value);
  virtual void ReturnInstruction(HInstruction* instr, int ast_id);
  virtual void ReturnControl(HControlInstruction* instr, int ast_id);

  static TestContext* cast(AstContext* context) {
    ASSERT(context->IsTest());
    return reinterpret_cast<TestContext*>(context);
  }

  Expression* condition() const { return condition_; }
  HBasicBlock* if_true() const { return if_true_; }
  HBasicBlock* if_false() const { return if_false_; }

 private:
  // Build the shared core part of the translation unpacking a value into
  // control flow.
  void BuildBranch(HValue* value);

  Expression* condition_;
  HBasicBlock* if_true_;
  HBasicBlock* if_false_;
};


enum ReturnHandlingFlag {
  NORMAL_RETURN,
  DROP_EXTRA_ON_RETURN,
  CONSTRUCT_CALL_RETURN
};


class FunctionState {
 public:
  FunctionState(HGraphBuilder* owner,
                CompilationInfo* info,
                TypeFeedbackOracle* oracle,
                ReturnHandlingFlag return_handling);
  ~FunctionState();

  CompilationInfo* compilation_info() { return compilation_info_; }
  TypeFeedbackOracle* oracle() { return oracle_; }
  AstContext* call_context() { return call_context_; }
  bool drop_extra() { return return_handling_ == DROP_EXTRA_ON_RETURN; }
  bool is_construct() { return return_handling_ == CONSTRUCT_CALL_RETURN; }
  HBasicBlock* function_return() { return function_return_; }
  TestContext* test_context() { return test_context_; }
  void ClearInlinedTestContext() {
    delete test_context_;
    test_context_ = NULL;
  }

  FunctionState* outer() { return outer_; }

  HEnterInlined* entry() { return entry_; }
  void set_entry(HEnterInlined* entry) { entry_ = entry; }

  HArgumentsElements* arguments_elements() { return arguments_elements_; }
  void set_arguments_elements(HArgumentsElements* arguments_elements) {
    arguments_elements_ = arguments_elements;
  }

  bool arguments_pushed() { return arguments_elements() != NULL; }

 private:
  HGraphBuilder* owner_;

  CompilationInfo* compilation_info_;
  TypeFeedbackOracle* oracle_;

  // During function inlining, expression context of the call being
  // inlined. NULL when not inlining.
  AstContext* call_context_;

  // Indicate whether we have to perform special handling on return from
  // inlined functions.
  // - DROP_EXTRA_ON_RETURN: Drop an extra value from the environment.
  // - CONSTRUCT_CALL_RETURN: Either use allocated receiver or return value.
  ReturnHandlingFlag return_handling_;

  // When inlining in an effect or value context, this is the return block.
  // It is NULL otherwise.  When inlining in a test context, there are a
  // pair of return blocks in the context.  When not inlining, there is no
  // local return point.
  HBasicBlock* function_return_;

  // When inlining a call in a test context, a context containing a pair of
  // return blocks.  NULL in all other cases.
  TestContext* test_context_;

  // When inlining HEnterInlined instruction corresponding to the function
  // entry.
  HEnterInlined* entry_;

  HArgumentsElements* arguments_elements_;

  FunctionState* outer_;
};


class HGraphBuilder: public AstVisitor {
 public:
  enum BreakType { BREAK, CONTINUE };
  enum SwitchType { UNKNOWN_SWITCH, SMI_SWITCH, STRING_SWITCH };

  // A class encapsulating (lazily-allocated) break and continue blocks for
  // a breakable statement.  Separated from BreakAndContinueScope so that it
  // can have a separate lifetime.
  class BreakAndContinueInfo BASE_EMBEDDED {
   public:
    explicit BreakAndContinueInfo(BreakableStatement* target,
                                  int drop_extra = 0)
        : target_(target),
          break_block_(NULL),
          continue_block_(NULL),
          drop_extra_(drop_extra) {
    }

    BreakableStatement* target() { return target_; }
    HBasicBlock* break_block() { return break_block_; }
    void set_break_block(HBasicBlock* block) { break_block_ = block; }
    HBasicBlock* continue_block() { return continue_block_; }
    void set_continue_block(HBasicBlock* block) { continue_block_ = block; }
    int drop_extra() { return drop_extra_; }

   private:
    BreakableStatement* target_;
    HBasicBlock* break_block_;
    HBasicBlock* continue_block_;
    int drop_extra_;
  };

  // A helper class to maintain a stack of current BreakAndContinueInfo
  // structures mirroring BreakableStatement nesting.
  class BreakAndContinueScope BASE_EMBEDDED {
   public:
    BreakAndContinueScope(BreakAndContinueInfo* info, HGraphBuilder* owner)
        : info_(info), owner_(owner), next_(owner->break_scope()) {
      owner->set_break_scope(this);
    }

    ~BreakAndContinueScope() { owner_->set_break_scope(next_); }

    BreakAndContinueInfo* info() { return info_; }
    HGraphBuilder* owner() { return owner_; }
    BreakAndContinueScope* next() { return next_; }

    // Search the break stack for a break or continue target.
    HBasicBlock* Get(BreakableStatement* stmt, BreakType type, int* drop_extra);

   private:
    BreakAndContinueInfo* info_;
    HGraphBuilder* owner_;
    BreakAndContinueScope* next_;
  };

  HGraphBuilder(CompilationInfo* info, TypeFeedbackOracle* oracle);

  HGraph* CreateGraph();

  // Simple accessors.
  HGraph* graph() const { return graph_; }
  BreakAndContinueScope* break_scope() const { return break_scope_; }
  void set_break_scope(BreakAndContinueScope* head) { break_scope_ = head; }

  HBasicBlock* current_block() const { return current_block_; }
  void set_current_block(HBasicBlock* block) { current_block_ = block; }
  HEnvironment* environment() const {
    return current_block()->last_environment();
  }

  bool inline_bailout() { return inline_bailout_; }

  // Adding instructions.
  HInstruction* AddInstruction(HInstruction* instr);
  void AddSimulate(int ast_id);

  // Bailout environment manipulation.
  void Push(HValue* value) { environment()->Push(value); }
  HValue* Pop() { return environment()->Pop(); }

  void Bailout(const char* reason);

  HBasicBlock* CreateJoin(HBasicBlock* first,
                          HBasicBlock* second,
                          int join_id);

  TypeFeedbackOracle* oracle() const { return function_state()->oracle(); }

  FunctionState* function_state() const { return function_state_; }

  void VisitDeclarations(ZoneList<Declaration*>* declarations);

 private:
  // Type of a member function that generates inline code for a native function.
  typedef void (HGraphBuilder::*InlineFunctionGenerator)(CallRuntime* call);

  // Forward declarations for inner scope classes.
  class SubgraphScope;

  static const InlineFunctionGenerator kInlineFunctionGenerators[];

  static const int kMaxCallPolymorphism = 4;
  static const int kMaxLoadPolymorphism = 4;
  static const int kMaxStorePolymorphism = 4;

  // Even in the 'unlimited' case we have to have some limit in order not to
  // overflow the stack.
  static const int kUnlimitedMaxInlinedSourceSize = 100000;
  static const int kUnlimitedMaxInlinedNodes = 10000;
  static const int kUnlimitedMaxInlinedNodesCumulative = 10000;

  // Simple accessors.
  void set_function_state(FunctionState* state) { function_state_ = state; }

  AstContext* ast_context() const { return ast_context_; }
  void set_ast_context(AstContext* context) { ast_context_ = context; }

  // Accessors forwarded to the function state.
  CompilationInfo* info() const {
    return function_state()->compilation_info();
  }
  AstContext* call_context() const {
    return function_state()->call_context();
  }
  HBasicBlock* function_return() const {
    return function_state()->function_return();
  }
  TestContext* inlined_test_context() const {
    return function_state()->test_context();
  }
  void ClearInlinedTestContext() {
    function_state()->ClearInlinedTestContext();
  }
  StrictModeFlag function_strict_mode_flag() {
    return function_state()->compilation_info()->is_classic_mode()
        ? kNonStrictMode : kStrictMode;
  }

  // Generators for inline runtime functions.
#define INLINE_FUNCTION_GENERATOR_DECLARATION(Name, argc, ressize)      \
  void Generate##Name(CallRuntime* call);

  INLINE_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_DECLARATION)
  INLINE_RUNTIME_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_DECLARATION)
#undef INLINE_FUNCTION_GENERATOR_DECLARATION

  void VisitDelete(UnaryOperation* expr);
  void VisitVoid(UnaryOperation* expr);
  void VisitTypeof(UnaryOperation* expr);
  void VisitAdd(UnaryOperation* expr);
  void VisitSub(UnaryOperation* expr);
  void VisitBitNot(UnaryOperation* expr);
  void VisitNot(UnaryOperation* expr);

  void VisitComma(BinaryOperation* expr);
  void VisitLogicalExpression(BinaryOperation* expr);
  void VisitArithmeticExpression(BinaryOperation* expr);

  bool PreProcessOsrEntry(IterationStatement* statement);
  // True iff. we are compiling for OSR and the statement is the entry.
  bool HasOsrEntryAt(IterationStatement* statement);
  void VisitLoopBody(IterationStatement* stmt,
                     HBasicBlock* loop_entry,
                     BreakAndContinueInfo* break_info);

  // Create a back edge in the flow graph.  body_exit is the predecessor
  // block and loop_entry is the successor block.  loop_successor is the
  // block where control flow exits the loop normally (e.g., via failure of
  // the condition) and break_block is the block where control flow breaks
  // from the loop.  All blocks except loop_entry can be NULL.  The return
  // value is the new successor block which is the join of loop_successor
  // and break_block, or NULL.
  HBasicBlock* CreateLoop(IterationStatement* statement,
                          HBasicBlock* loop_entry,
                          HBasicBlock* body_exit,
                          HBasicBlock* loop_successor,
                          HBasicBlock* break_block);

  HBasicBlock* JoinContinue(IterationStatement* statement,
                            HBasicBlock* exit_block,
                            HBasicBlock* continue_block);

  HValue* Top() const { return environment()->Top(); }
  void Drop(int n) { environment()->Drop(n); }
  void Bind(Variable* var, HValue* value) { environment()->Bind(var, value); }

  // The value of the arguments object is allowed in some but not most value
  // contexts.  (It's allowed in all effect contexts and disallowed in all
  // test contexts.)
  void VisitForValue(Expression* expr,
                     ArgumentsAllowedFlag flag = ARGUMENTS_NOT_ALLOWED);
  void VisitForTypeOf(Expression* expr);
  void VisitForEffect(Expression* expr);
  void VisitForControl(Expression* expr,
                       HBasicBlock* true_block,
                       HBasicBlock* false_block);

  // Visit an argument subexpression and emit a push to the outgoing
  // arguments.  Returns the hydrogen value that was pushed.
  HValue* VisitArgument(Expression* expr);

  void VisitArgumentList(ZoneList<Expression*>* arguments);

  // Visit a list of expressions from left to right, each in a value context.
  void VisitExpressions(ZoneList<Expression*>* exprs);

  void AddPhi(HPhi* phi);

  void PushAndAdd(HInstruction* instr);

  // Remove the arguments from the bailout environment and emit instructions
  // to push them as outgoing parameters.
  template <class Instruction> HInstruction* PreProcessCall(Instruction* call);

  void TraceRepresentation(Token::Value op,
                           TypeInfo info,
                           HValue* value,
                           Representation rep);
  static Representation ToRepresentation(TypeInfo info);

  void SetUpScope(Scope* scope);
  virtual void VisitStatements(ZoneList<Statement*>* statements);

#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  HBasicBlock* CreateBasicBlock(HEnvironment* env);
  HBasicBlock* CreateLoopHeaderBlock();

  // Helpers for flow graph construction.
  enum GlobalPropertyAccess {
    kUseCell,
    kUseGeneric
  };
  GlobalPropertyAccess LookupGlobalProperty(Variable* var,
                                            LookupResult* lookup,
                                            bool is_store);

  void EnsureArgumentsArePushedForAccess();
  bool TryArgumentsAccess(Property* expr);

  // Try to optimize fun.apply(receiver, arguments) pattern.
  bool TryCallApply(Call* expr);

  int InliningAstSize(Handle<JSFunction> target);
  bool TryInline(CallKind call_kind,
                 Handle<JSFunction> target,
                 ZoneList<Expression*>* arguments,
                 HValue* receiver,
                 int ast_id,
                 int return_id,
                 ReturnHandlingFlag return_handling);

  bool TryInlineCall(Call* expr, bool drop_extra = false);
  bool TryInlineConstruct(CallNew* expr, HValue* receiver);
  bool TryInlineBuiltinMethodCall(Call* expr,
                                  HValue* receiver,
                                  Handle<Map> receiver_map,
                                  CheckType check_type);
  bool TryInlineBuiltinFunctionCall(Call* expr, bool drop_extra);

  // If --trace-inlining, print a line of the inlining trace.  Inlining
  // succeeded if the reason string is NULL and failed if there is a
  // non-NULL reason string.
  void TraceInline(Handle<JSFunction> target,
                   Handle<JSFunction> caller,
                   const char* failure_reason);

  void HandleGlobalVariableAssignment(Variable* var,
                                      HValue* value,
                                      int position,
                                      int ast_id);

  void HandlePropertyAssignment(Assignment* expr);
  void HandleCompoundAssignment(Assignment* expr);
  void HandlePolymorphicLoadNamedField(Property* expr,
                                       HValue* object,
                                       SmallMapList* types,
                                       Handle<String> name);
  void HandlePolymorphicStoreNamedField(Assignment* expr,
                                        HValue* object,
                                        HValue* value,
                                        SmallMapList* types,
                                        Handle<String> name);
  void HandlePolymorphicCallNamed(Call* expr,
                                  HValue* receiver,
                                  SmallMapList* types,
                                  Handle<String> name);
  void HandleLiteralCompareTypeof(CompareOperation* expr,
                                  HTypeof* typeof_expr,
                                  Handle<String> check);
  void HandleLiteralCompareNil(CompareOperation* expr,
                               HValue* value,
                               NilValue nil);

  HStringCharCodeAt* BuildStringCharCodeAt(HValue* context,
                                           HValue* string,
                                           HValue* index);
  HInstruction* BuildBinaryOperation(BinaryOperation* expr,
                                     HValue* left,
                                     HValue* right);
  HInstruction* BuildIncrement(bool returns_original_input,
                               CountOperation* expr);
  HLoadNamedField* BuildLoadNamedField(HValue* object,
                                       Property* expr,
                                       Handle<Map> type,
                                       LookupResult* result,
                                       bool smi_and_map_check);
  HInstruction* BuildLoadNamedGeneric(HValue* object, Property* expr);
  HInstruction* BuildLoadKeyedGeneric(HValue* object,
                                      HValue* key);
  HInstruction* BuildExternalArrayElementAccess(
      HValue* external_elements,
      HValue* checked_key,
      HValue* val,
      ElementsKind elements_kind,
      bool is_store);
  HInstruction* BuildFastElementAccess(HValue* elements,
                                       HValue* checked_key,
                                       HValue* val,
                                       ElementsKind elements_kind,
                                       bool is_store);

  HInstruction* BuildMonomorphicElementAccess(HValue* object,
                                              HValue* key,
                                              HValue* val,
                                              HValue* dependency,
                                              Handle<Map> map,
                                              bool is_store);
  HValue* HandlePolymorphicElementAccess(HValue* object,
                                         HValue* key,
                                         HValue* val,
                                         Expression* prop,
                                         int ast_id,
                                         int position,
                                         bool is_store,
                                         bool* has_side_effects);

  HValue* HandleKeyedElementAccess(HValue* obj,
                                   HValue* key,
                                   HValue* val,
                                   Expression* expr,
                                   int ast_id,
                                   int position,
                                   bool is_store,
                                   bool* has_side_effects);

  HInstruction* BuildLoadNamed(HValue* object,
                               Property* prop,
                               Handle<Map> map,
                               Handle<String> name);
  HInstruction* BuildStoreNamed(HValue* object,
                                HValue* value,
                                Expression* expr);
  HInstruction* BuildStoreNamed(HValue* object,
                                HValue* value,
                                ObjectLiteral::Property* prop);
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

  HValue* BuildContextChainWalk(Variable* var);

  void AddCheckConstantFunction(Call* expr,
                                HValue* receiver,
                                Handle<Map> receiver_map,
                                bool smi_and_map_check);

  Zone* zone() { return zone_; }

  // The translation state of the currently-being-translated function.
  FunctionState* function_state_;

  // The base of the function state stack.
  FunctionState initial_function_state_;

  // Expression context of the currently visited subexpression. NULL when
  // visiting statements.
  AstContext* ast_context_;

  // A stack of breakable statements entered.
  BreakAndContinueScope* break_scope_;

  HGraph* graph_;
  HBasicBlock* current_block_;

  int inlined_count_;
  ZoneList<Handle<Object> > globals_;

  Zone* zone_;

  bool inline_bailout_;

  friend class FunctionState;  // Pushes and pops the state stack.
  friend class AstContext;  // Pushes and pops the AST context stack.

  DISALLOW_COPY_AND_ASSIGN(HGraphBuilder);
};


Zone* AstContext::zone() { return owner_->zone(); }


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

  void Kill(GVNFlagSet flags);

  void Add(HValue* value) {
    present_flags_.Add(value->gvn_flags());
    Insert(value);
  }

  HValue* Lookup(HValue* value) const;

  HValueMap* Copy(Zone* zone) const {
    return new(zone) HValueMap(zone, this);
  }

  bool IsEmpty() const { return count_ == 0; }

 private:
  // A linked list of HValue* values.  Stored in arrays.
  struct HValueMapListElement {
    HValue* value;
    int next;  // Index in the array of the next list element.
  };
  static const int kNil = -1;  // The end of a linked list

  // Must be a power of 2.
  static const int kInitialSize = 16;

  HValueMap(Zone* zone, const HValueMap* other);

  void Resize(int new_size);
  void ResizeLists(int new_size);
  void Insert(HValue* value);
  uint32_t Bound(uint32_t value) const { return value & (array_size_ - 1); }

  int array_size_;
  int lists_size_;
  int count_;  // The number of values stored in the HValueMap.
  GVNFlagSet present_flags_;  // All flags that are in any value in the
                              // HValueMap.
  HValueMapListElement* array_;  // Primary store - contains the first value
  // with a given hash.  Colliding elements are stored in linked lists.
  HValueMapListElement* lists_;  // The linked lists containing hash collisions.
  int free_list_head_;  // Unused elements in lists_ are on the free list.
};


class HSideEffectMap BASE_EMBEDDED {
 public:
  HSideEffectMap();
  explicit HSideEffectMap(HSideEffectMap* other);

  void Kill(GVNFlagSet flags);

  void Store(GVNFlagSet flags, HInstruction* instr);

  bool IsEmpty() const { return count_ == 0; }

  inline HInstruction* operator[](int i) const {
    ASSERT(0 <= i);
    ASSERT(i < kNumberOfTrackedSideEffects);
    return data_[i];
  }
  inline HInstruction* at(int i) const { return operator[](i); }

 private:
  int count_;
  HInstruction* data_[kNumberOfTrackedSideEffects];
};


class HStatistics: public Malloced {
 public:
  void Initialize(CompilationInfo* info);
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
        full_code_gen_(0),
        source_size_(0) { }

  List<int64_t> timing_;
  List<const char*> names_;
  List<unsigned> sizes_;
  int64_t total_;
  unsigned total_size_;
  int64_t full_code_gen_;
  double source_size_;
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
