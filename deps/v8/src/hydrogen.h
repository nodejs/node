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

#include "accessors.h"
#include "allocation.h"
#include "ast.h"
#include "compiler.h"
#include "hydrogen-instructions.h"
#include "zone.h"
#include "scopes.h"

namespace v8 {
namespace internal {

// Forward declarations.
class BitVector;
class FunctionState;
class HEnvironment;
class HGraph;
class HLoopInformation;
class HOsrBuilder;
class HTracer;
class LAllocator;
class LChunk;
class LiveRange;


class HBasicBlock V8_FINAL : public ZoneObject {
 public:
  explicit HBasicBlock(HGraph* graph);
  ~HBasicBlock() { }

  // Simple accessors.
  int block_id() const { return block_id_; }
  void set_block_id(int id) { block_id_ = id; }
  HGraph* graph() const { return graph_; }
  Isolate* isolate() const;
  const ZoneList<HPhi*>* phis() const { return &phis_; }
  HInstruction* first() const { return first_; }
  HInstruction* last() const { return last_; }
  void set_last(HInstruction* instr) { last_ = instr; }
  HControlInstruction* end() const { return end_; }
  HLoopInformation* loop_information() const { return loop_information_; }
  HLoopInformation* current_loop() const {
    return IsLoopHeader() ? loop_information()
                          : (parent_loop_header() != NULL
                            ? parent_loop_header()->loop_information() : NULL);
  }
  const ZoneList<HBasicBlock*>* predecessors() const { return &predecessors_; }
  bool HasPredecessor() const { return predecessors_.length() > 0; }
  const ZoneList<HBasicBlock*>* dominated_blocks() const {
    return &dominated_blocks_;
  }
  const ZoneList<int>* deleted_phis() const {
    return &deleted_phis_;
  }
  void RecordDeletedPhi(int merge_index) {
    deleted_phis_.Add(merge_index, zone());
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
  bool is_osr_entry() { return is_osr_entry_; }
  void set_osr_entry() { is_osr_entry_ = true; }

  void AttachLoopInformation();
  void DetachLoopInformation();
  bool IsLoopHeader() const { return loop_information() != NULL; }
  bool IsStartBlock() const { return block_id() == 0; }
  void PostProcessLoopHeader(IterationStatement* stmt);

  bool IsFinished() const { return end_ != NULL; }
  void AddPhi(HPhi* phi);
  void RemovePhi(HPhi* phi);
  void AddInstruction(HInstruction* instr, HSourcePosition position);
  bool Dominates(HBasicBlock* other) const;
  bool EqualToOrDominates(HBasicBlock* other) const;
  int LoopNestingDepth() const;

  void SetInitialEnvironment(HEnvironment* env);
  void ClearEnvironment() {
    ASSERT(IsFinished());
    ASSERT(end()->SuccessorCount() == 0);
    last_environment_ = NULL;
  }
  bool HasEnvironment() const { return last_environment_ != NULL; }
  void UpdateEnvironment(HEnvironment* env);
  HBasicBlock* parent_loop_header() const { return parent_loop_header_; }

  void set_parent_loop_header(HBasicBlock* block) {
    ASSERT(parent_loop_header_ == NULL);
    parent_loop_header_ = block;
  }

  bool HasParentLoopHeader() const { return parent_loop_header_ != NULL; }

  void SetJoinId(BailoutId ast_id);

  int PredecessorIndexOf(HBasicBlock* predecessor) const;
  HPhi* AddNewPhi(int merged_index);
  HSimulate* AddNewSimulate(BailoutId ast_id,
                            HSourcePosition position,
                            RemovableSimulate removable = FIXED_SIMULATE) {
    HSimulate* instr = CreateSimulate(ast_id, removable);
    AddInstruction(instr, position);
    return instr;
  }
  void AssignCommonDominator(HBasicBlock* other);
  void AssignLoopSuccessorDominators();

  // If a target block is tagged as an inline function return, all
  // predecessors should contain the inlined exit sequence:
  //
  // LeaveInlined
  // Simulate (caller's environment)
  // Goto (target block)
  bool IsInlineReturnTarget() const { return is_inline_return_target_; }
  void MarkAsInlineReturnTarget(HBasicBlock* inlined_entry_block) {
    is_inline_return_target_ = true;
    inlined_entry_block_ = inlined_entry_block;
  }
  HBasicBlock* inlined_entry_block() { return inlined_entry_block_; }

  bool IsDeoptimizing() const {
    return end() != NULL && end()->IsDeoptimize();
  }

  void MarkUnreachable();
  bool IsUnreachable() const { return !is_reachable_; }
  bool IsReachable() const { return is_reachable_; }

  bool IsLoopSuccessorDominator() const {
    return dominates_loop_successors_;
  }
  void MarkAsLoopSuccessorDominator() {
    dominates_loop_successors_ = true;
  }

  void MarkSuccEdgeUnreachable(int succ);

  inline Zone* zone() const;

#ifdef DEBUG
  void Verify();
#endif

 protected:
  friend class HGraphBuilder;

  HSimulate* CreateSimulate(BailoutId ast_id, RemovableSimulate removable);
  void Finish(HControlInstruction* last, HSourcePosition position);
  void FinishExit(HControlInstruction* instruction, HSourcePosition position);
  void Goto(HBasicBlock* block,
            HSourcePosition position,
            FunctionState* state = NULL,
            bool add_simulate = true);
  void GotoNoSimulate(HBasicBlock* block, HSourcePosition position) {
    Goto(block, position, NULL, false);
  }

  // Add the inlined function exit sequence, adding an HLeaveInlined
  // instruction and updating the bailout environment.
  void AddLeaveInlined(HValue* return_value,
                       FunctionState* state,
                       HSourcePosition position);

 private:
  void RegisterPredecessor(HBasicBlock* pred);
  void AddDominatedBlock(HBasicBlock* block);

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
  // For blocks marked as inline return target: the block with HEnterInlined.
  HBasicBlock* inlined_entry_block_;
  bool is_inline_return_target_ : 1;
  bool is_reachable_ : 1;
  bool dominates_loop_successors_ : 1;
  bool is_osr_entry_ : 1;
};


class HPredecessorIterator V8_FINAL BASE_EMBEDDED {
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


class HInstructionIterator V8_FINAL BASE_EMBEDDED {
 public:
  explicit HInstructionIterator(HBasicBlock* block)
      : instr_(block->first()) {
    next_ = Done() ? NULL : instr_->next();
  }

  inline bool Done() const { return instr_ == NULL; }
  inline HInstruction* Current() { return instr_; }
  inline void Advance() {
    instr_ = next_;
    next_ = Done() ? NULL : instr_->next();
  }

 private:
  HInstruction* instr_;
  HInstruction* next_;
};


class HLoopInformation V8_FINAL : public ZoneObject {
 public:
  HLoopInformation(HBasicBlock* loop_header, Zone* zone)
      : back_edges_(4, zone),
        loop_header_(loop_header),
        blocks_(8, zone),
        stack_check_(NULL) {
    blocks_.Add(loop_header, zone);
  }
  ~HLoopInformation() {}

  const ZoneList<HBasicBlock*>* back_edges() const { return &back_edges_; }
  const ZoneList<HBasicBlock*>* blocks() const { return &blocks_; }
  HBasicBlock* loop_header() const { return loop_header_; }
  HBasicBlock* GetLastBackEdge() const;
  void RegisterBackEdge(HBasicBlock* block);

  HStackCheck* stack_check() const { return stack_check_; }
  void set_stack_check(HStackCheck* stack_check) {
    stack_check_ = stack_check;
  }

  bool IsNestedInThisLoop(HLoopInformation* other) {
    while (other != NULL) {
      if (other == this) {
        return true;
      }
      other = other->parent_loop();
    }
    return false;
  }
  HLoopInformation* parent_loop() {
    HBasicBlock* parent_header = loop_header()->parent_loop_header();
    return parent_header != NULL ? parent_header->loop_information() : NULL;
  }

 private:
  void AddBlock(HBasicBlock* block);

  ZoneList<HBasicBlock*> back_edges_;
  HBasicBlock* loop_header_;
  ZoneList<HBasicBlock*> blocks_;
  HStackCheck* stack_check_;
};


class BoundsCheckTable;
class InductionVariableBlocksTable;
class HGraph V8_FINAL : public ZoneObject {
 public:
  explicit HGraph(CompilationInfo* info);

  Isolate* isolate() const { return isolate_; }
  Zone* zone() const { return zone_; }
  CompilationInfo* info() const { return info_; }

  const ZoneList<HBasicBlock*>* blocks() const { return &blocks_; }
  const ZoneList<HPhi*>* phi_list() const { return phi_list_; }
  HBasicBlock* entry_block() const { return entry_block_; }
  HEnvironment* start_environment() const { return start_environment_; }

  void FinalizeUniqueness();
  bool ProcessArgumentsObject();
  void OrderBlocks();
  void AssignDominators();
  void RestoreActualValues();

  // Returns false if there are phi-uses of the arguments-object
  // which are not supported by the optimizing compiler.
  bool CheckArgumentsPhiUses();

  // Returns false if there are phi-uses of an uninitialized const
  // which are not supported by the optimizing compiler.
  bool CheckConstPhiUses();

  void CollectPhis();

  HConstant* GetConstantUndefined();
  HConstant* GetConstant0();
  HConstant* GetConstant1();
  HConstant* GetConstantMinus1();
  HConstant* GetConstantTrue();
  HConstant* GetConstantFalse();
  HConstant* GetConstantHole();
  HConstant* GetConstantNull();
  HConstant* GetInvalidContext();

  bool IsConstantUndefined(HConstant* constant);
  bool IsConstant0(HConstant* constant);
  bool IsConstant1(HConstant* constant);
  bool IsConstantMinus1(HConstant* constant);
  bool IsConstantTrue(HConstant* constant);
  bool IsConstantFalse(HConstant* constant);
  bool IsConstantHole(HConstant* constant);
  bool IsConstantNull(HConstant* constant);
  bool IsStandardConstant(HConstant* constant);

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
    ASSERT(!disallow_adding_new_values_);
    values_.Add(value, zone());
    return values_.length() - 1;
  }
  HValue* LookupValue(int id) const {
    if (id >= 0 && id < values_.length()) return values_[id];
    return NULL;
  }
  void DisallowAddingNewValues() {
    disallow_adding_new_values_ = true;
  }

  bool Optimize(BailoutReason* bailout_reason);

#ifdef DEBUG
  void Verify(bool do_full_verify) const;
#endif

  bool has_osr() {
    return osr_ != NULL;
  }

  void set_osr(HOsrBuilder* osr) {
    osr_ = osr;
  }

  HOsrBuilder* osr() {
    return osr_;
  }

  int update_type_change_checksum(int delta) {
    type_change_checksum_ += delta;
    return type_change_checksum_;
  }

  void update_maximum_environment_size(int environment_size) {
    if (environment_size > maximum_environment_size_) {
      maximum_environment_size_ = environment_size;
    }
  }
  int maximum_environment_size() { return maximum_environment_size_; }

  bool use_optimistic_licm() {
    return use_optimistic_licm_;
  }

  void set_use_optimistic_licm(bool value) {
    use_optimistic_licm_ = value;
  }

  void MarkRecursive() {
    is_recursive_ = true;
  }

  bool is_recursive() const {
    return is_recursive_;
  }

  void MarkDependsOnEmptyArrayProtoElements() {
    // Add map dependency if not already added.
    if (depends_on_empty_array_proto_elements_) return;
    isolate()->initial_object_prototype()->map()->AddDependentCompilationInfo(
        DependentCode::kElementsCantBeAddedGroup, info());
    isolate()->initial_array_prototype()->map()->AddDependentCompilationInfo(
        DependentCode::kElementsCantBeAddedGroup, info());
    depends_on_empty_array_proto_elements_ = true;
  }

  bool depends_on_empty_array_proto_elements() {
    return depends_on_empty_array_proto_elements_;
  }

  bool has_uint32_instructions() {
    ASSERT(uint32_instructions_ == NULL || !uint32_instructions_->is_empty());
    return uint32_instructions_ != NULL;
  }

  ZoneList<HInstruction*>* uint32_instructions() {
    ASSERT(uint32_instructions_ == NULL || !uint32_instructions_->is_empty());
    return uint32_instructions_;
  }

  void RecordUint32Instruction(HInstruction* instr) {
    ASSERT(uint32_instructions_ == NULL || !uint32_instructions_->is_empty());
    if (uint32_instructions_ == NULL) {
      uint32_instructions_ = new(zone()) ZoneList<HInstruction*>(4, zone());
    }
    uint32_instructions_->Add(instr, zone());
  }

  void IncrementInNoSideEffectsScope() { no_side_effects_scope_count_++; }
  void DecrementInNoSideEffectsScope() { no_side_effects_scope_count_--; }
  bool IsInsideNoSideEffectsScope() { return no_side_effects_scope_count_ > 0; }

  // If we are tracking source positions then this function assigns a unique
  // identifier to each inlining and dumps function source if it was inlined
  // for the first time during the current optimization.
  int TraceInlinedFunction(Handle<SharedFunctionInfo> shared,
                           HSourcePosition position);

  // Converts given HSourcePosition to the absolute offset from the start of
  // the corresponding script.
  int SourcePositionToScriptPosition(HSourcePosition position);

 private:
  HConstant* ReinsertConstantIfNecessary(HConstant* constant);
  HConstant* GetConstant(SetOncePointer<HConstant>* pointer,
                         int32_t integer_value);

  template<class Phase>
  void Run() {
    Phase phase(this);
    phase.Run();
  }

  void EliminateRedundantBoundsChecksUsingInductionVariables();

  Isolate* isolate_;
  int next_block_id_;
  HBasicBlock* entry_block_;
  HEnvironment* start_environment_;
  ZoneList<HBasicBlock*> blocks_;
  ZoneList<HValue*> values_;
  ZoneList<HPhi*>* phi_list_;
  ZoneList<HInstruction*>* uint32_instructions_;
  SetOncePointer<HConstant> constant_undefined_;
  SetOncePointer<HConstant> constant_0_;
  SetOncePointer<HConstant> constant_1_;
  SetOncePointer<HConstant> constant_minus1_;
  SetOncePointer<HConstant> constant_true_;
  SetOncePointer<HConstant> constant_false_;
  SetOncePointer<HConstant> constant_the_hole_;
  SetOncePointer<HConstant> constant_null_;
  SetOncePointer<HConstant> constant_invalid_context_;
  SetOncePointer<HArgumentsObject> arguments_object_;

  HOsrBuilder* osr_;

  CompilationInfo* info_;
  Zone* zone_;

  bool is_recursive_;
  bool use_optimistic_licm_;
  bool depends_on_empty_array_proto_elements_;
  int type_change_checksum_;
  int maximum_environment_size_;
  int no_side_effects_scope_count_;
  bool disallow_adding_new_values_;

  class InlinedFunctionInfo {
   public:
    explicit InlinedFunctionInfo(Handle<SharedFunctionInfo> shared)
      : shared_(shared), start_position_(shared->start_position()) {
    }

    Handle<SharedFunctionInfo> shared() const { return shared_; }
    int start_position() const { return start_position_; }

   private:
    Handle<SharedFunctionInfo> shared_;
    int start_position_;
  };

  int next_inline_id_;
  ZoneList<InlinedFunctionInfo> inlined_functions_;

  DISALLOW_COPY_AND_ASSIGN(HGraph);
};


Zone* HBasicBlock::zone() const { return graph_->zone(); }


// Type of stack frame an environment might refer to.
enum FrameType {
  JS_FUNCTION,
  JS_CONSTRUCT,
  JS_GETTER,
  JS_SETTER,
  ARGUMENTS_ADAPTOR,
  STUB
};


class HEnvironment V8_FINAL : public ZoneObject {
 public:
  HEnvironment(HEnvironment* outer,
               Scope* scope,
               Handle<JSFunction> closure,
               Zone* zone);

  HEnvironment(Zone* zone, int parameter_count);

  HEnvironment* arguments_environment() {
    return outer()->frame_type() == ARGUMENTS_ADAPTOR ? outer() : this;
  }

  // Simple accessors.
  Handle<JSFunction> closure() const { return closure_; }
  const ZoneList<HValue*>* values() const { return &values_; }
  const GrowableBitVector* assigned_variables() const {
    return &assigned_variables_;
  }
  FrameType frame_type() const { return frame_type_; }
  int parameter_count() const { return parameter_count_; }
  int specials_count() const { return specials_count_; }
  int local_count() const { return local_count_; }
  HEnvironment* outer() const { return outer_; }
  int pop_count() const { return pop_count_; }
  int push_count() const { return push_count_; }

  BailoutId ast_id() const { return ast_id_; }
  void set_ast_id(BailoutId id) { ast_id_ = id; }

  HEnterInlined* entry() const { return entry_; }
  void set_entry(HEnterInlined* entry) { entry_ = entry; }

  int length() const { return values_.length(); }

  int first_expression_index() const {
    return parameter_count() + specials_count() + local_count();
  }

  int first_local_index() const {
    return parameter_count() + specials_count();
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

  HValue* context() const {
    // Return first special.
    return Lookup(parameter_count());
  }

  void Push(HValue* value) {
    ASSERT(value != NULL);
    ++push_count_;
    values_.Add(value, zone());
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
                                InliningKind inlining_kind) const;

  HEnvironment* DiscardInlined(bool drop_extra) {
    HEnvironment* outer = outer_;
    while (outer->frame_type() != JS_FUNCTION) outer = outer->outer_;
    if (drop_extra) outer->Drop(1);
    return outer;
  }

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

  bool is_local_index(int i) const {
    return i >= first_local_index() && i < first_expression_index();
  }

  bool is_parameter_index(int i) const {
    return i >= 0 && i < parameter_count();
  }

  bool is_special_index(int i) const {
    return i >= parameter_count() && i < parameter_count() + specials_count();
  }

  void PrintTo(StringStream* stream);
  void PrintToStd();

  Zone* zone() const { return zone_; }

 private:
  HEnvironment(const HEnvironment* other, Zone* zone);

  HEnvironment(HEnvironment* outer,
               Handle<JSFunction> closure,
               FrameType frame_type,
               int arguments,
               Zone* zone);

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

  Handle<JSFunction> closure_;
  // Value array [parameters] [specials] [locals] [temporaries].
  ZoneList<HValue*> values_;
  GrowableBitVector assigned_variables_;
  FrameType frame_type_;
  int parameter_count_;
  int specials_count_;
  int local_count_;
  HEnvironment* outer_;
  HEnterInlined* entry_;
  int pop_count_;
  int push_count_;
  BailoutId ast_id_;
  Zone* zone_;
};


class HOptimizedGraphBuilder;

enum ArgumentsAllowedFlag {
  ARGUMENTS_NOT_ALLOWED,
  ARGUMENTS_ALLOWED
};


class HIfContinuation;

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
  virtual void ReturnInstruction(HInstruction* instr, BailoutId ast_id) = 0;

  // Finishes the current basic block and materialize a boolean for
  // value context, nothing for effect, generate a branch for test context.
  // Call this function in tail position in the Visit functions for
  // expressions.
  virtual void ReturnControl(HControlInstruction* instr, BailoutId ast_id) = 0;

  // Finishes the current basic block and materialize a boolean for
  // value context, nothing for effect, generate a branch for test context.
  // Call this function in tail position in the Visit functions for
  // expressions that use an IfBuilder.
  virtual void ReturnContinuation(HIfContinuation* continuation,
                                  BailoutId ast_id) = 0;

  void set_for_typeof(bool for_typeof) { for_typeof_ = for_typeof; }
  bool is_for_typeof() { return for_typeof_; }

 protected:
  AstContext(HOptimizedGraphBuilder* owner, Expression::Context kind);
  virtual ~AstContext();

  HOptimizedGraphBuilder* owner() const { return owner_; }

  inline Zone* zone() const;

  // We want to be able to assert, in a context-specific way, that the stack
  // height makes sense when the context is filled.
#ifdef DEBUG
  int original_length_;
#endif

 private:
  HOptimizedGraphBuilder* owner_;
  Expression::Context kind_;
  AstContext* outer_;
  bool for_typeof_;
};


class EffectContext V8_FINAL : public AstContext {
 public:
  explicit EffectContext(HOptimizedGraphBuilder* owner)
      : AstContext(owner, Expression::kEffect) {
  }
  virtual ~EffectContext();

  virtual void ReturnValue(HValue* value) V8_OVERRIDE;
  virtual void ReturnInstruction(HInstruction* instr,
                                 BailoutId ast_id) V8_OVERRIDE;
  virtual void ReturnControl(HControlInstruction* instr,
                             BailoutId ast_id) V8_OVERRIDE;
  virtual void ReturnContinuation(HIfContinuation* continuation,
                                  BailoutId ast_id) V8_OVERRIDE;
};


class ValueContext V8_FINAL : public AstContext {
 public:
  ValueContext(HOptimizedGraphBuilder* owner, ArgumentsAllowedFlag flag)
      : AstContext(owner, Expression::kValue), flag_(flag) {
  }
  virtual ~ValueContext();

  virtual void ReturnValue(HValue* value) V8_OVERRIDE;
  virtual void ReturnInstruction(HInstruction* instr,
                                 BailoutId ast_id) V8_OVERRIDE;
  virtual void ReturnControl(HControlInstruction* instr,
                             BailoutId ast_id) V8_OVERRIDE;
  virtual void ReturnContinuation(HIfContinuation* continuation,
                                  BailoutId ast_id) V8_OVERRIDE;

  bool arguments_allowed() { return flag_ == ARGUMENTS_ALLOWED; }

 private:
  ArgumentsAllowedFlag flag_;
};


class TestContext V8_FINAL : public AstContext {
 public:
  TestContext(HOptimizedGraphBuilder* owner,
              Expression* condition,
              HBasicBlock* if_true,
              HBasicBlock* if_false)
      : AstContext(owner, Expression::kTest),
        condition_(condition),
        if_true_(if_true),
        if_false_(if_false) {
  }

  virtual void ReturnValue(HValue* value) V8_OVERRIDE;
  virtual void ReturnInstruction(HInstruction* instr,
                                 BailoutId ast_id) V8_OVERRIDE;
  virtual void ReturnControl(HControlInstruction* instr,
                             BailoutId ast_id) V8_OVERRIDE;
  virtual void ReturnContinuation(HIfContinuation* continuation,
                                  BailoutId ast_id) V8_OVERRIDE;

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


class FunctionState V8_FINAL {
 public:
  FunctionState(HOptimizedGraphBuilder* owner,
                CompilationInfo* info,
                InliningKind inlining_kind,
                int inlining_id);
  ~FunctionState();

  CompilationInfo* compilation_info() { return compilation_info_; }
  AstContext* call_context() { return call_context_; }
  InliningKind inlining_kind() const { return inlining_kind_; }
  HBasicBlock* function_return() { return function_return_; }
  TestContext* test_context() { return test_context_; }
  void ClearInlinedTestContext() {
    delete test_context_;
    test_context_ = NULL;
  }

  FunctionState* outer() { return outer_; }

  HEnterInlined* entry() { return entry_; }
  void set_entry(HEnterInlined* entry) { entry_ = entry; }

  HArgumentsObject* arguments_object() { return arguments_object_; }
  void set_arguments_object(HArgumentsObject* arguments_object) {
    arguments_object_ = arguments_object;
  }

  HArgumentsElements* arguments_elements() { return arguments_elements_; }
  void set_arguments_elements(HArgumentsElements* arguments_elements) {
    arguments_elements_ = arguments_elements;
  }

  bool arguments_pushed() { return arguments_elements() != NULL; }

  int inlining_id() const { return inlining_id_; }

 private:
  HOptimizedGraphBuilder* owner_;

  CompilationInfo* compilation_info_;

  // During function inlining, expression context of the call being
  // inlined. NULL when not inlining.
  AstContext* call_context_;

  // The kind of call which is currently being inlined.
  InliningKind inlining_kind_;

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

  HArgumentsObject* arguments_object_;
  HArgumentsElements* arguments_elements_;

  int inlining_id_;
  HSourcePosition outer_source_position_;

  FunctionState* outer_;
};


class HIfContinuation V8_FINAL {
 public:
  HIfContinuation()
    : continuation_captured_(false),
      true_branch_(NULL),
      false_branch_(NULL) {}
  HIfContinuation(HBasicBlock* true_branch,
                  HBasicBlock* false_branch)
      : continuation_captured_(true), true_branch_(true_branch),
        false_branch_(false_branch) {}
  ~HIfContinuation() { ASSERT(!continuation_captured_); }

  void Capture(HBasicBlock* true_branch,
               HBasicBlock* false_branch) {
    ASSERT(!continuation_captured_);
    true_branch_ = true_branch;
    false_branch_ = false_branch;
    continuation_captured_ = true;
  }

  void Continue(HBasicBlock** true_branch,
                HBasicBlock** false_branch) {
    ASSERT(continuation_captured_);
    *true_branch = true_branch_;
    *false_branch = false_branch_;
    continuation_captured_ = false;
  }

  bool IsTrueReachable() { return true_branch_ != NULL; }
  bool IsFalseReachable() { return false_branch_ != NULL; }
  bool TrueAndFalseReachable() {
    return IsTrueReachable() || IsFalseReachable();
  }

  HBasicBlock* true_branch() const { return true_branch_; }
  HBasicBlock* false_branch() const { return false_branch_; }

 private:
  bool continuation_captured_;
  HBasicBlock* true_branch_;
  HBasicBlock* false_branch_;
};


class HAllocationMode V8_FINAL BASE_EMBEDDED {
 public:
  explicit HAllocationMode(Handle<AllocationSite> feedback_site)
      : current_site_(NULL), feedback_site_(feedback_site),
        pretenure_flag_(NOT_TENURED) {}
  explicit HAllocationMode(HValue* current_site)
      : current_site_(current_site), pretenure_flag_(NOT_TENURED) {}
  explicit HAllocationMode(PretenureFlag pretenure_flag)
      : current_site_(NULL), pretenure_flag_(pretenure_flag) {}
  HAllocationMode()
      : current_site_(NULL), pretenure_flag_(NOT_TENURED) {}

  HValue* current_site() const { return current_site_; }
  Handle<AllocationSite> feedback_site() const { return feedback_site_; }

  bool CreateAllocationMementos() const V8_WARN_UNUSED_RESULT {
    return current_site() != NULL;
  }

  PretenureFlag GetPretenureMode() const V8_WARN_UNUSED_RESULT {
    if (!feedback_site().is_null()) return feedback_site()->GetPretenureMode();
    return pretenure_flag_;
  }

 private:
  HValue* current_site_;
  Handle<AllocationSite> feedback_site_;
  PretenureFlag pretenure_flag_;
};


class HGraphBuilder {
 public:
  explicit HGraphBuilder(CompilationInfo* info)
      : info_(info),
        graph_(NULL),
        current_block_(NULL),
        position_(HSourcePosition::Unknown()),
        start_position_(0) {}
  virtual ~HGraphBuilder() {}

  HBasicBlock* current_block() const { return current_block_; }
  void set_current_block(HBasicBlock* block) { current_block_ = block; }
  HEnvironment* environment() const {
    return current_block()->last_environment();
  }
  Zone* zone() const { return info_->zone(); }
  HGraph* graph() const { return graph_; }
  Isolate* isolate() const { return graph_->isolate(); }
  CompilationInfo* top_info() { return info_; }

  HGraph* CreateGraph();

  // Bailout environment manipulation.
  void Push(HValue* value) { environment()->Push(value); }
  HValue* Pop() { return environment()->Pop(); }

  virtual HValue* context() = 0;

  // Adding instructions.
  HInstruction* AddInstruction(HInstruction* instr);
  void FinishCurrentBlock(HControlInstruction* last);
  void FinishExitCurrentBlock(HControlInstruction* instruction);

  void Goto(HBasicBlock* from,
            HBasicBlock* target,
            FunctionState* state = NULL,
            bool add_simulate = true) {
    from->Goto(target, source_position(), state, add_simulate);
  }
  void Goto(HBasicBlock* target,
            FunctionState* state = NULL,
            bool add_simulate = true) {
    Goto(current_block(), target, state, add_simulate);
  }
  void GotoNoSimulate(HBasicBlock* from, HBasicBlock* target) {
    Goto(from, target, NULL, false);
  }
  void GotoNoSimulate(HBasicBlock* target) {
    Goto(target, NULL, false);
  }
  void AddLeaveInlined(HBasicBlock* block,
                       HValue* return_value,
                       FunctionState* state) {
    block->AddLeaveInlined(return_value, state, source_position());
  }
  void AddLeaveInlined(HValue* return_value, FunctionState* state) {
    return AddLeaveInlined(current_block(), return_value, state);
  }

  template<class I>
  HInstruction* NewUncasted() { return I::New(zone(), context()); }

  template<class I>
  I* New() { return I::New(zone(), context()); }

  template<class I>
  HInstruction* AddUncasted() { return AddInstruction(NewUncasted<I>());}

  template<class I>
  I* Add() { return AddInstructionTyped(New<I>());}

  template<class I, class P1>
  HInstruction* NewUncasted(P1 p1) {
    return I::New(zone(), context(), p1);
  }

  template<class I, class P1>
  I* New(P1 p1) { return I::New(zone(), context(), p1); }

  template<class I, class P1>
  HInstruction* AddUncasted(P1 p1) {
    HInstruction* result = AddInstruction(NewUncasted<I>(p1));
    // Specializations must have their parameters properly casted
    // to avoid landing here.
    ASSERT(!result->IsReturn() && !result->IsSimulate() &&
           !result->IsDeoptimize());
    return result;
  }

  template<class I, class P1>
  I* Add(P1 p1) {
    I* result = AddInstructionTyped(New<I>(p1));
    // Specializations must have their parameters properly casted
    // to avoid landing here.
    ASSERT(!result->IsReturn() && !result->IsSimulate() &&
           !result->IsDeoptimize());
    return result;
  }

  template<class I, class P1, class P2>
  HInstruction* NewUncasted(P1 p1, P2 p2) {
    return I::New(zone(), context(), p1, p2);
  }

  template<class I, class P1, class P2>
  I* New(P1 p1, P2 p2) {
    return I::New(zone(), context(), p1, p2);
  }

  template<class I, class P1, class P2>
  HInstruction* AddUncasted(P1 p1, P2 p2) {
    HInstruction* result = AddInstruction(NewUncasted<I>(p1, p2));
    // Specializations must have their parameters properly casted
    // to avoid landing here.
    ASSERT(!result->IsSimulate());
    return result;
  }

  template<class I, class P1, class P2>
  I* Add(P1 p1, P2 p2) {
    I* result = AddInstructionTyped(New<I>(p1, p2));
    // Specializations must have their parameters properly casted
    // to avoid landing here.
    ASSERT(!result->IsSimulate());
    return result;
  }

  template<class I, class P1, class P2, class P3>
  HInstruction* NewUncasted(P1 p1, P2 p2, P3 p3) {
    return I::New(zone(), context(), p1, p2, p3);
  }

  template<class I, class P1, class P2, class P3>
  I* New(P1 p1, P2 p2, P3 p3) {
    return I::New(zone(), context(), p1, p2, p3);
  }

  template<class I, class P1, class P2, class P3>
  HInstruction* AddUncasted(P1 p1, P2 p2, P3 p3) {
    return AddInstruction(NewUncasted<I>(p1, p2, p3));
  }

  template<class I, class P1, class P2, class P3>
  I* Add(P1 p1, P2 p2, P3 p3) {
    return AddInstructionTyped(New<I>(p1, p2, p3));
  }

  template<class I, class P1, class P2, class P3, class P4>
  HInstruction* NewUncasted(P1 p1, P2 p2, P3 p3, P4 p4) {
    return I::New(zone(), context(), p1, p2, p3, p4);
  }

  template<class I, class P1, class P2, class P3, class P4>
  I* New(P1 p1, P2 p2, P3 p3, P4 p4) {
    return I::New(zone(), context(), p1, p2, p3, p4);
  }

  template<class I, class P1, class P2, class P3, class P4>
  HInstruction* AddUncasted(P1 p1, P2 p2, P3 p3, P4 p4) {
    return AddInstruction(NewUncasted<I>(p1, p2, p3, p4));
  }

  template<class I, class P1, class P2, class P3, class P4>
  I* Add(P1 p1, P2 p2, P3 p3, P4 p4) {
    return AddInstructionTyped(New<I>(p1, p2, p3, p4));
  }

  template<class I, class P1, class P2, class P3, class P4, class P5>
  HInstruction* NewUncasted(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5);
  }

  template<class I, class P1, class P2, class P3, class P4, class P5>
  I* New(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5);
  }

  template<class I, class P1, class P2, class P3, class P4, class P5>
  HInstruction* AddUncasted(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    return AddInstruction(NewUncasted<I>(p1, p2, p3, p4, p5));
  }

  template<class I, class P1, class P2, class P3, class P4, class P5>
  I* Add(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    return AddInstructionTyped(New<I>(p1, p2, p3, p4, p5));
  }

  template<class I, class P1, class P2, class P3, class P4, class P5, class P6>
  HInstruction* NewUncasted(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5, p6);
  }

  template<class I, class P1, class P2, class P3, class P4, class P5, class P6>
  I* New(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5, p6);
  }

  template<class I, class P1, class P2, class P3, class P4, class P5, class P6>
  HInstruction* AddUncasted(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6) {
    return AddInstruction(NewUncasted<I>(p1, p2, p3, p4, p5, p6));
  }

  template<class I, class P1, class P2, class P3, class P4, class P5, class P6>
  I* Add(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6) {
    return AddInstructionTyped(New<I>(p1, p2, p3, p4, p5, p6));
  }

  template<class I, class P1, class P2, class P3, class P4,
      class P5, class P6, class P7>
  HInstruction* NewUncasted(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5, p6, p7);
  }

  template<class I, class P1, class P2, class P3, class P4,
      class P5, class P6, class P7>
      I* New(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5, p6, p7);
  }

  template<class I, class P1, class P2, class P3,
           class P4, class P5, class P6, class P7>
  HInstruction* AddUncasted(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {
    return AddInstruction(NewUncasted<I>(p1, p2, p3, p4, p5, p6, p7));
  }

  template<class I, class P1, class P2, class P3,
           class P4, class P5, class P6, class P7>
  I* Add(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {
    return AddInstructionTyped(New<I>(p1, p2, p3, p4, p5, p6, p7));
  }

  template<class I, class P1, class P2, class P3, class P4,
      class P5, class P6, class P7, class P8>
  HInstruction* NewUncasted(P1 p1, P2 p2, P3 p3, P4 p4,
                            P5 p5, P6 p6, P7 p7, P8 p8) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5, p6, p7, p8);
  }

  template<class I, class P1, class P2, class P3, class P4,
      class P5, class P6, class P7, class P8>
      I* New(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8) {
    return I::New(zone(), context(), p1, p2, p3, p4, p5, p6, p7, p8);
  }

  template<class I, class P1, class P2, class P3, class P4,
           class P5, class P6, class P7, class P8>
  HInstruction* AddUncasted(P1 p1, P2 p2, P3 p3, P4 p4,
                            P5 p5, P6 p6, P7 p7, P8 p8) {
    return AddInstruction(NewUncasted<I>(p1, p2, p3, p4, p5, p6, p7, p8));
  }

  template<class I, class P1, class P2, class P3, class P4,
           class P5, class P6, class P7, class P8>
  I* Add(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8) {
    return AddInstructionTyped(New<I>(p1, p2, p3, p4, p5, p6, p7, p8));
  }

  void AddSimulate(BailoutId id, RemovableSimulate removable = FIXED_SIMULATE);

 protected:
  virtual bool BuildGraph() = 0;

  HBasicBlock* CreateBasicBlock(HEnvironment* env);
  HBasicBlock* CreateLoopHeaderBlock();

  HValue* BuildCheckHeapObject(HValue* object);
  HValue* BuildCheckMap(HValue* obj, Handle<Map> map);
  HValue* BuildCheckString(HValue* string);
  HValue* BuildWrapReceiver(HValue* object, HValue* function);

  // Building common constructs
  HValue* BuildCheckForCapacityGrow(HValue* object,
                                    HValue* elements,
                                    ElementsKind kind,
                                    HValue* length,
                                    HValue* key,
                                    bool is_js_array,
                                    PropertyAccessType access_type);

  HValue* BuildCopyElementsOnWrite(HValue* object,
                                   HValue* elements,
                                   ElementsKind kind,
                                   HValue* length);

  void BuildTransitionElementsKind(HValue* object,
                                   HValue* map,
                                   ElementsKind from_kind,
                                   ElementsKind to_kind,
                                   bool is_jsarray);

  HValue* BuildNumberToString(HValue* object, Type* type);

  HValue* BuildUncheckedDictionaryElementLoad(HValue* receiver,
                                              HValue* key);

  HValue* BuildRegExpConstructResult(HValue* length,
                                     HValue* index,
                                     HValue* input);

  // Allocates a new object according with the given allocation properties.
  HAllocate* BuildAllocate(HValue* object_size,
                           HType type,
                           InstanceType instance_type,
                           HAllocationMode allocation_mode);
  // Computes the sum of two string lengths, taking care of overflow handling.
  HValue* BuildAddStringLengths(HValue* left_length, HValue* right_length);
  // Creates a cons string using the two input strings.
  HValue* BuildCreateConsString(HValue* length,
                                HValue* left,
                                HValue* right,
                                HAllocationMode allocation_mode);
  // Copies characters from one sequential string to another.
  void BuildCopySeqStringChars(HValue* src,
                               HValue* src_offset,
                               String::Encoding src_encoding,
                               HValue* dst,
                               HValue* dst_offset,
                               String::Encoding dst_encoding,
                               HValue* length);

  // Align an object size to object alignment boundary
  HValue* BuildObjectSizeAlignment(HValue* unaligned_size, int header_size);

  // Both operands are non-empty strings.
  HValue* BuildUncheckedStringAdd(HValue* left,
                                  HValue* right,
                                  HAllocationMode allocation_mode);
  // Add two strings using allocation mode, validating type feedback.
  HValue* BuildStringAdd(HValue* left,
                         HValue* right,
                         HAllocationMode allocation_mode);

  HInstruction* BuildUncheckedMonomorphicElementAccess(
      HValue* checked_object,
      HValue* key,
      HValue* val,
      bool is_js_array,
      ElementsKind elements_kind,
      PropertyAccessType access_type,
      LoadKeyedHoleMode load_mode,
      KeyedAccessStoreMode store_mode);

  HInstruction* AddElementAccess(
      HValue* elements,
      HValue* checked_key,
      HValue* val,
      HValue* dependency,
      ElementsKind elements_kind,
      PropertyAccessType access_type,
      LoadKeyedHoleMode load_mode = NEVER_RETURN_HOLE);

  HInstruction* AddLoadStringInstanceType(HValue* string);
  HInstruction* AddLoadStringLength(HValue* string);
  HStoreNamedField* AddStoreMapNoWriteBarrier(HValue* object, HValue* map) {
    HStoreNamedField* store_map = Add<HStoreNamedField>(
        object, HObjectAccess::ForMap(), map);
    store_map->SkipWriteBarrier();
    return store_map;
  }
  HStoreNamedField* AddStoreMapConstant(HValue* object, Handle<Map> map);
  HStoreNamedField* AddStoreMapConstantNoWriteBarrier(HValue* object,
                                                      Handle<Map> map) {
    HStoreNamedField* store_map = AddStoreMapConstant(object, map);
    store_map->SkipWriteBarrier();
    return store_map;
  }
  HLoadNamedField* AddLoadElements(HValue* object);

  bool MatchRotateRight(HValue* left,
                        HValue* right,
                        HValue** operand,
                        HValue** shift_amount);

  HValue* BuildBinaryOperation(Token::Value op,
                               HValue* left,
                               HValue* right,
                               Type* left_type,
                               Type* right_type,
                               Type* result_type,
                               Maybe<int> fixed_right_arg,
                               HAllocationMode allocation_mode);

  HLoadNamedField* AddLoadFixedArrayLength(HValue *object);

  HValue* AddLoadJSBuiltin(Builtins::JavaScript builtin);

  HValue* EnforceNumberType(HValue* number, Type* expected);
  HValue* TruncateToNumber(HValue* value, Type** expected);

  void FinishExitWithHardDeoptimization(const char* reason);

  void AddIncrementCounter(StatsCounter* counter);

  class IfBuilder V8_FINAL {
   public:
    explicit IfBuilder(HGraphBuilder* builder);
    IfBuilder(HGraphBuilder* builder,
              HIfContinuation* continuation);

    ~IfBuilder() {
      if (!finished_) End();
    }

    template<class Condition>
    Condition* If(HValue *p) {
      Condition* compare = builder()->New<Condition>(p);
      AddCompare(compare);
      return compare;
    }

    template<class Condition, class P2>
    Condition* If(HValue* p1, P2 p2) {
      Condition* compare = builder()->New<Condition>(p1, p2);
      AddCompare(compare);
      return compare;
    }

    template<class Condition, class P2, class P3>
    Condition* If(HValue* p1, P2 p2, P3 p3) {
      Condition* compare = builder()->New<Condition>(p1, p2, p3);
      AddCompare(compare);
      return compare;
    }

    template<class Condition>
    Condition* IfNot(HValue* p) {
      Condition* compare = If<Condition>(p);
      compare->Not();
      return compare;
    }

    template<class Condition, class P2>
    Condition* IfNot(HValue* p1, P2 p2) {
      Condition* compare = If<Condition>(p1, p2);
      compare->Not();
      return compare;
    }

    template<class Condition, class P2, class P3>
    Condition* IfNot(HValue* p1, P2 p2, P3 p3) {
      Condition* compare = If<Condition>(p1, p2, p3);
      compare->Not();
      return compare;
    }

    template<class Condition>
    Condition* OrIf(HValue *p) {
      Or();
      return If<Condition>(p);
    }

    template<class Condition, class P2>
    Condition* OrIf(HValue* p1, P2 p2) {
      Or();
      return If<Condition>(p1, p2);
    }

    template<class Condition, class P2, class P3>
    Condition* OrIf(HValue* p1, P2 p2, P3 p3) {
      Or();
      return If<Condition>(p1, p2, p3);
    }

    template<class Condition>
    Condition* AndIf(HValue *p) {
      And();
      return If<Condition>(p);
    }

    template<class Condition, class P2>
    Condition* AndIf(HValue* p1, P2 p2) {
      And();
      return If<Condition>(p1, p2);
    }

    template<class Condition, class P2, class P3>
    Condition* AndIf(HValue* p1, P2 p2, P3 p3) {
      And();
      return If<Condition>(p1, p2, p3);
    }

    void Or();
    void And();

    // Captures the current state of this IfBuilder in the specified
    // continuation and ends this IfBuilder.
    void CaptureContinuation(HIfContinuation* continuation);

    // Joins the specified continuation from this IfBuilder and ends this
    // IfBuilder. This appends a Goto instruction from the true branch of
    // this IfBuilder to the true branch of the continuation unless the
    // true branch of this IfBuilder is already finished. And vice versa
    // for the false branch.
    //
    // The basic idea is as follows: You have several nested IfBuilder's
    // that you want to join based on two possible outcomes (i.e. success
    // and failure, or whatever). You can do this easily using this method
    // now, for example:
    //
    //   HIfContinuation cont(graph()->CreateBasicBlock(),
    //                        graph()->CreateBasicBlock());
    //   ...
    //     IfBuilder if_whatever(this);
    //     if_whatever.If<Condition>(arg);
    //     if_whatever.Then();
    //     ...
    //     if_whatever.Else();
    //     ...
    //     if_whatever.JoinContinuation(&cont);
    //   ...
    //     IfBuilder if_something(this);
    //     if_something.If<Condition>(arg1, arg2);
    //     if_something.Then();
    //     ...
    //     if_something.Else();
    //     ...
    //     if_something.JoinContinuation(&cont);
    //   ...
    //   IfBuilder if_finally(this, &cont);
    //   if_finally.Then();
    //   // continues after then code of if_whatever or if_something.
    //   ...
    //   if_finally.Else();
    //   // continues after else code of if_whatever or if_something.
    //   ...
    //   if_finally.End();
    void JoinContinuation(HIfContinuation* continuation);

    void Then();
    void Else();
    void End();

    void Deopt(const char* reason);
    void ThenDeopt(const char* reason) {
      Then();
      Deopt(reason);
    }
    void ElseDeopt(const char* reason) {
      Else();
      Deopt(reason);
    }

    void Return(HValue* value);

   private:
    HControlInstruction* AddCompare(HControlInstruction* compare);

    HGraphBuilder* builder() const { return builder_; }

    void AddMergeAtJoinBlock(bool deopt);

    void Finish();
    void Finish(HBasicBlock** then_continuation,
                HBasicBlock** else_continuation);

    class MergeAtJoinBlock : public ZoneObject {
     public:
      MergeAtJoinBlock(HBasicBlock* block,
                       bool deopt,
                       MergeAtJoinBlock* next)
        : block_(block),
          deopt_(deopt),
          next_(next) {}
      HBasicBlock* block_;
      bool deopt_;
      MergeAtJoinBlock* next_;
    };

    HGraphBuilder* builder_;
    bool finished_ : 1;
    bool did_then_ : 1;
    bool did_else_ : 1;
    bool did_else_if_ : 1;
    bool did_and_ : 1;
    bool did_or_ : 1;
    bool captured_ : 1;
    bool needs_compare_ : 1;
    bool pending_merge_block_ : 1;
    HBasicBlock* first_true_block_;
    HBasicBlock* first_false_block_;
    HBasicBlock* split_edge_merge_block_;
    MergeAtJoinBlock* merge_at_join_blocks_;
    int normal_merge_at_join_block_count_;
    int deopt_merge_at_join_block_count_;
  };

  class LoopBuilder V8_FINAL {
   public:
    enum Direction {
      kPreIncrement,
      kPostIncrement,
      kPreDecrement,
      kPostDecrement
    };

    LoopBuilder(HGraphBuilder* builder,
                HValue* context,
                Direction direction);
    LoopBuilder(HGraphBuilder* builder,
                HValue* context,
                Direction direction,
                HValue* increment_amount);

    ~LoopBuilder() {
      ASSERT(finished_);
    }

    HValue* BeginBody(
        HValue* initial,
        HValue* terminating,
        Token::Value token);

    void Break();

    void EndBody();

   private:
    Zone* zone() { return builder_->zone(); }

    HGraphBuilder* builder_;
    HValue* context_;
    HValue* increment_amount_;
    HInstruction* increment_;
    HPhi* phi_;
    HBasicBlock* header_block_;
    HBasicBlock* body_block_;
    HBasicBlock* exit_block_;
    HBasicBlock* exit_trampoline_block_;
    Direction direction_;
    bool finished_;
  };

  HValue* BuildNewElementsCapacity(HValue* old_capacity);

  void BuildNewSpaceArrayCheck(HValue* length,
                               ElementsKind kind);

  class JSArrayBuilder V8_FINAL {
   public:
    JSArrayBuilder(HGraphBuilder* builder,
                   ElementsKind kind,
                   HValue* allocation_site_payload,
                   HValue* constructor_function,
                   AllocationSiteOverrideMode override_mode);

    JSArrayBuilder(HGraphBuilder* builder,
                   ElementsKind kind,
                   HValue* constructor_function = NULL);

    enum FillMode {
      DONT_FILL_WITH_HOLE,
      FILL_WITH_HOLE
    };

    ElementsKind kind() { return kind_; }

    HValue* AllocateEmptyArray();
    HValue* AllocateArray(HValue* capacity, HValue* length_field,
                          FillMode fill_mode = FILL_WITH_HOLE);
    HValue* GetElementsLocation() { return elements_location_; }
    HValue* EmitMapCode();

   private:
    Zone* zone() const { return builder_->zone(); }
    int elements_size() const {
      return IsFastDoubleElementsKind(kind_) ? kDoubleSize : kPointerSize;
    }
    HGraphBuilder* builder() { return builder_; }
    HGraph* graph() { return builder_->graph(); }
    int initial_capacity() {
      STATIC_ASSERT(JSArray::kPreallocatedArrayElements > 0);
      return JSArray::kPreallocatedArrayElements;
    }

    HValue* EmitInternalMapCode();
    HValue* EstablishEmptyArrayAllocationSize();
    HValue* EstablishAllocationSize(HValue* length_node);
    HValue* AllocateArray(HValue* size_in_bytes, HValue* capacity,
                          HValue* length_field,
                          FillMode fill_mode = FILL_WITH_HOLE);

    HGraphBuilder* builder_;
    ElementsKind kind_;
    AllocationSiteMode mode_;
    HValue* allocation_site_payload_;
    HValue* constructor_function_;
    HInnerAllocatedObject* elements_location_;
  };

  HValue* BuildAllocateArrayFromLength(JSArrayBuilder* array_builder,
                                       HValue* length_argument);

  HValue* BuildAllocateElements(ElementsKind kind,
                                HValue* capacity);

  void BuildInitializeElementsHeader(HValue* elements,
                                     ElementsKind kind,
                                     HValue* capacity);

  HValue* BuildAllocateElementsAndInitializeElementsHeader(ElementsKind kind,
                                                           HValue* capacity);

  // array must have been allocated with enough room for
  // 1) the JSArray, 2) a AllocationMemento if mode requires it,
  // 3) a FixedArray or FixedDoubleArray.
  // A pointer to the Fixed(Double)Array is returned.
  HInnerAllocatedObject* BuildJSArrayHeader(HValue* array,
                                            HValue* array_map,
                                            AllocationSiteMode mode,
                                            ElementsKind elements_kind,
                                            HValue* allocation_site_payload,
                                            HValue* length_field);

  HValue* BuildGrowElementsCapacity(HValue* object,
                                    HValue* elements,
                                    ElementsKind kind,
                                    ElementsKind new_kind,
                                    HValue* length,
                                    HValue* new_capacity);

  void BuildFillElementsWithHole(HValue* elements,
                                 ElementsKind elements_kind,
                                 HValue* from,
                                 HValue* to);

  void BuildCopyElements(HValue* from_elements,
                         ElementsKind from_elements_kind,
                         HValue* to_elements,
                         ElementsKind to_elements_kind,
                         HValue* length,
                         HValue* capacity);

  HValue* BuildCloneShallowArray(HValue* boilerplate,
                                 HValue* allocation_site,
                                 AllocationSiteMode mode,
                                 ElementsKind kind,
                                 int length);

  HValue* BuildElementIndexHash(HValue* index);

  void BuildCompareNil(
      HValue* value,
      Type* type,
      HIfContinuation* continuation);

  void BuildCreateAllocationMemento(HValue* previous_object,
                                    HValue* previous_object_size,
                                    HValue* payload);

  HInstruction* BuildConstantMapCheck(Handle<JSObject> constant,
                                      CompilationInfo* info);
  HInstruction* BuildCheckPrototypeMaps(Handle<JSObject> prototype,
                                        Handle<JSObject> holder);

  HInstruction* BuildGetNativeContext(HValue* closure);
  HInstruction* BuildGetNativeContext();
  HInstruction* BuildGetArrayFunction();

 protected:
  void SetSourcePosition(int position) {
    ASSERT(position != RelocInfo::kNoPosition);
    position_.set_position(position - start_position_);
  }

  void EnterInlinedSource(int start_position, int id) {
    if (FLAG_hydrogen_track_positions) {
      start_position_ = start_position;
      position_.set_inlining_id(id);
    }
  }

  // Convert the given absolute offset from the start of the script to
  // the HSourcePosition assuming that this position corresponds to the
  // same function as current position_.
  HSourcePosition ScriptPositionToSourcePosition(int position) {
    HSourcePosition pos = position_;
    pos.set_position(position - start_position_);
    return pos;
  }

  HSourcePosition source_position() { return position_; }
  void set_source_position(HSourcePosition position) {
    position_ = position;
  }

  template <typename ViewClass>
  void BuildArrayBufferViewInitialization(HValue* obj,
                                          HValue* buffer,
                                          HValue* byte_offset,
                                          HValue* byte_length);

 private:
  HGraphBuilder();

  HValue* BuildUncheckedDictionaryElementLoadHelper(
      HValue* elements,
      HValue* key,
      HValue* hash,
      HValue* mask,
      int current_probe);

  template <class I>
  I* AddInstructionTyped(I* instr) {
    return I::cast(AddInstruction(instr));
  }

  CompilationInfo* info_;
  HGraph* graph_;
  HBasicBlock* current_block_;
  HSourcePosition position_;
  int start_position_;
};


template<>
inline HDeoptimize* HGraphBuilder::Add<HDeoptimize>(
    const char* reason, Deoptimizer::BailoutType type) {
  if (type == Deoptimizer::SOFT) {
    isolate()->counters()->soft_deopts_requested()->Increment();
    if (FLAG_always_opt) return NULL;
  }
  if (current_block()->IsDeoptimizing()) return NULL;
  HBasicBlock* after_deopt_block = CreateBasicBlock(
      current_block()->last_environment());
  HDeoptimize* instr = New<HDeoptimize>(reason, type, after_deopt_block);
  if (type == Deoptimizer::SOFT) {
    isolate()->counters()->soft_deopts_inserted()->Increment();
  }
  FinishCurrentBlock(instr);
  set_current_block(after_deopt_block);
  return instr;
}


template<>
inline HInstruction* HGraphBuilder::AddUncasted<HDeoptimize>(
    const char* reason, Deoptimizer::BailoutType type) {
  return Add<HDeoptimize>(reason, type);
}


template<>
inline HSimulate* HGraphBuilder::Add<HSimulate>(
    BailoutId id,
    RemovableSimulate removable) {
  HSimulate* instr = current_block()->CreateSimulate(id, removable);
  AddInstruction(instr);
  return instr;
}


template<>
inline HSimulate* HGraphBuilder::Add<HSimulate>(
    BailoutId id) {
  return Add<HSimulate>(id, FIXED_SIMULATE);
}


template<>
inline HInstruction* HGraphBuilder::AddUncasted<HSimulate>(BailoutId id) {
  return Add<HSimulate>(id, FIXED_SIMULATE);
}


template<>
inline HReturn* HGraphBuilder::Add<HReturn>(HValue* value) {
  int num_parameters = graph()->info()->num_parameters();
  HValue* params = AddUncasted<HConstant>(num_parameters);
  HReturn* return_instruction = New<HReturn>(value, params);
  FinishExitCurrentBlock(return_instruction);
  return return_instruction;
}


template<>
inline HReturn* HGraphBuilder::Add<HReturn>(HConstant* value) {
  return Add<HReturn>(static_cast<HValue*>(value));
}

template<>
inline HInstruction* HGraphBuilder::AddUncasted<HReturn>(HValue* value) {
  return Add<HReturn>(value);
}


template<>
inline HInstruction* HGraphBuilder::AddUncasted<HReturn>(HConstant* value) {
  return Add<HReturn>(value);
}


template<>
inline HCallRuntime* HGraphBuilder::Add<HCallRuntime>(
    Handle<String> name,
    const Runtime::Function* c_function,
    int argument_count) {
  HCallRuntime* instr = New<HCallRuntime>(name, c_function, argument_count);
  if (graph()->info()->IsStub()) {
    // When compiling code stubs, we don't want to save all double registers
    // upon entry to the stub, but instead have the call runtime instruction
    // save the double registers only on-demand (in the fallback case).
    instr->set_save_doubles(kSaveFPRegs);
  }
  AddInstruction(instr);
  return instr;
}


template<>
inline HInstruction* HGraphBuilder::AddUncasted<HCallRuntime>(
    Handle<String> name,
    const Runtime::Function* c_function,
    int argument_count) {
  return Add<HCallRuntime>(name, c_function, argument_count);
}


template<>
inline HContext* HGraphBuilder::New<HContext>() {
  return HContext::New(zone());
}


template<>
inline HInstruction* HGraphBuilder::NewUncasted<HContext>() {
  return New<HContext>();
}

class HOptimizedGraphBuilder : public HGraphBuilder, public AstVisitor {
 public:
  // A class encapsulating (lazily-allocated) break and continue blocks for
  // a breakable statement.  Separated from BreakAndContinueScope so that it
  // can have a separate lifetime.
  class BreakAndContinueInfo V8_FINAL BASE_EMBEDDED {
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
  class BreakAndContinueScope V8_FINAL BASE_EMBEDDED {
   public:
    BreakAndContinueScope(BreakAndContinueInfo* info,
                          HOptimizedGraphBuilder* owner)
        : info_(info), owner_(owner), next_(owner->break_scope()) {
      owner->set_break_scope(this);
    }

    ~BreakAndContinueScope() { owner_->set_break_scope(next_); }

    BreakAndContinueInfo* info() { return info_; }
    HOptimizedGraphBuilder* owner() { return owner_; }
    BreakAndContinueScope* next() { return next_; }

    // Search the break stack for a break or continue target.
    enum BreakType { BREAK, CONTINUE };
    HBasicBlock* Get(BreakableStatement* stmt, BreakType type, int* drop_extra);

   private:
    BreakAndContinueInfo* info_;
    HOptimizedGraphBuilder* owner_;
    BreakAndContinueScope* next_;
  };

  explicit HOptimizedGraphBuilder(CompilationInfo* info);

  virtual bool BuildGraph() V8_OVERRIDE;

  // Simple accessors.
  BreakAndContinueScope* break_scope() const { return break_scope_; }
  void set_break_scope(BreakAndContinueScope* head) { break_scope_ = head; }

  bool inline_bailout() { return inline_bailout_; }

  HValue* context() { return environment()->context(); }

  HOsrBuilder* osr() const { return osr_; }

  void Bailout(BailoutReason reason);

  HBasicBlock* CreateJoin(HBasicBlock* first,
                          HBasicBlock* second,
                          BailoutId join_id);

  FunctionState* function_state() const { return function_state_; }

  void VisitDeclarations(ZoneList<Declaration*>* declarations);

  void* operator new(size_t size, Zone* zone) {
    return zone->New(static_cast<int>(size));
  }
  void operator delete(void* pointer, Zone* zone) { }
  void operator delete(void* pointer) { }

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 protected:
  // Type of a member function that generates inline code for a native function.
  typedef void (HOptimizedGraphBuilder::*InlineFunctionGenerator)
      (CallRuntime* call);

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

  // Maximum depth and total number of elements and properties for literal
  // graphs to be considered for fast deep-copying.
  static const int kMaxFastLiteralDepth = 3;
  static const int kMaxFastLiteralProperties = 8;

  // Simple accessors.
  void set_function_state(FunctionState* state) { function_state_ = state; }

  AstContext* ast_context() const { return ast_context_; }
  void set_ast_context(AstContext* context) { ast_context_ = context; }

  // Accessors forwarded to the function state.
  CompilationInfo* current_info() const {
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
  StrictMode function_strict_mode() {
    return function_state()->compilation_info()->strict_mode();
  }

  // Generators for inline runtime functions.
#define INLINE_FUNCTION_GENERATOR_DECLARATION(Name, argc, ressize)      \
  void Generate##Name(CallRuntime* call);

  INLINE_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_DECLARATION)
  INLINE_OPTIMIZED_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_DECLARATION)
#undef INLINE_FUNCTION_GENERATOR_DECLARATION

  void VisitDelete(UnaryOperation* expr);
  void VisitVoid(UnaryOperation* expr);
  void VisitTypeof(UnaryOperation* expr);
  void VisitNot(UnaryOperation* expr);

  void VisitComma(BinaryOperation* expr);
  void VisitLogicalExpression(BinaryOperation* expr);
  void VisitArithmeticExpression(BinaryOperation* expr);

  bool PreProcessOsrEntry(IterationStatement* statement);
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

  // Build a loop entry
  HBasicBlock* BuildLoopEntry();

  // Builds a loop entry respectful of OSR requirements
  HBasicBlock* BuildLoopEntry(IterationStatement* statement);

  HBasicBlock* JoinContinue(IterationStatement* statement,
                            HBasicBlock* exit_block,
                            HBasicBlock* continue_block);

  HValue* Top() const { return environment()->Top(); }
  void Drop(int n) { environment()->Drop(n); }
  void Bind(Variable* var, HValue* value) { environment()->Bind(var, value); }
  bool IsEligibleForEnvironmentLivenessAnalysis(Variable* var,
                                                int index,
                                                HValue* value,
                                                HEnvironment* env) {
    if (!FLAG_analyze_environment_liveness) return false;
    // |this| and |arguments| are always live; zapping parameters isn't
    // safe because function.arguments can inspect them at any time.
    return !var->is_this() &&
           !var->is_arguments() &&
           !value->IsArgumentsObject() &&
           env->is_local_index(index);
  }
  void BindIfLive(Variable* var, HValue* value) {
    HEnvironment* env = environment();
    int index = env->IndexFor(var);
    env->Bind(index, value);
    if (IsEligibleForEnvironmentLivenessAnalysis(var, index, value, env)) {
      HEnvironmentMarker* bind =
          Add<HEnvironmentMarker>(HEnvironmentMarker::BIND, index);
      USE(bind);
#ifdef DEBUG
      bind->set_closure(env->closure());
#endif
    }
  }

  HValue* LookupAndMakeLive(Variable* var) {
    HEnvironment* env = environment();
    int index = env->IndexFor(var);
    HValue* value = env->Lookup(index);
    if (IsEligibleForEnvironmentLivenessAnalysis(var, index, value, env)) {
      HEnvironmentMarker* lookup =
          Add<HEnvironmentMarker>(HEnvironmentMarker::LOOKUP, index);
      USE(lookup);
#ifdef DEBUG
      lookup->set_closure(env->closure());
#endif
    }
    return value;
  }

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

  // Visit a list of expressions from left to right, each in a value context.
  void VisitExpressions(ZoneList<Expression*>* exprs);

  // Remove the arguments from the bailout environment and emit instructions
  // to push them as outgoing parameters.
  template <class Instruction> HInstruction* PreProcessCall(Instruction* call);
  void PushArgumentsFromEnvironment(int count);

  void SetUpScope(Scope* scope);
  virtual void VisitStatements(ZoneList<Statement*>* statements) V8_OVERRIDE;

#define DECLARE_VISIT(type) virtual void Visit##type(type* node) V8_OVERRIDE;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  Type* ToType(Handle<Map> map) { return IC::MapToType<Type>(map, zone()); }

 private:
  // Helpers for flow graph construction.
  enum GlobalPropertyAccess {
    kUseCell,
    kUseGeneric
  };
  GlobalPropertyAccess LookupGlobalProperty(Variable* var,
                                            LookupResult* lookup,
                                            PropertyAccessType access_type);

  void EnsureArgumentsArePushedForAccess();
  bool TryArgumentsAccess(Property* expr);

  // Try to optimize fun.apply(receiver, arguments) pattern.
  bool TryCallApply(Call* expr);

  HValue* ImplicitReceiverFor(HValue* function,
                              Handle<JSFunction> target);

  int InliningAstSize(Handle<JSFunction> target);
  bool TryInline(Handle<JSFunction> target,
                 int arguments_count,
                 HValue* implicit_return_value,
                 BailoutId ast_id,
                 BailoutId return_id,
                 InliningKind inlining_kind,
                 HSourcePosition position);

  bool TryInlineCall(Call* expr);
  bool TryInlineConstruct(CallNew* expr, HValue* implicit_return_value);
  bool TryInlineGetter(Handle<JSFunction> getter,
                       Handle<Map> receiver_map,
                       BailoutId ast_id,
                       BailoutId return_id);
  bool TryInlineSetter(Handle<JSFunction> setter,
                       Handle<Map> receiver_map,
                       BailoutId id,
                       BailoutId assignment_id,
                       HValue* implicit_return_value);
  bool TryInlineApply(Handle<JSFunction> function,
                      Call* expr,
                      int arguments_count);
  bool TryInlineBuiltinMethodCall(Call* expr,
                                  HValue* receiver,
                                  Handle<Map> receiver_map);
  bool TryInlineBuiltinFunctionCall(Call* expr);
  enum ApiCallType {
    kCallApiFunction,
    kCallApiMethod,
    kCallApiGetter,
    kCallApiSetter
  };
  bool TryInlineApiMethodCall(Call* expr,
                              HValue* receiver,
                              SmallMapList* receiver_types);
  bool TryInlineApiFunctionCall(Call* expr, HValue* receiver);
  bool TryInlineApiGetter(Handle<JSFunction> function,
                          Handle<Map> receiver_map,
                          BailoutId ast_id);
  bool TryInlineApiSetter(Handle<JSFunction> function,
                          Handle<Map> receiver_map,
                          BailoutId ast_id);
  bool TryInlineApiCall(Handle<JSFunction> function,
                         HValue* receiver,
                         SmallMapList* receiver_maps,
                         int argc,
                         BailoutId ast_id,
                         ApiCallType call_type);

  // If --trace-inlining, print a line of the inlining trace.  Inlining
  // succeeded if the reason string is NULL and failed if there is a
  // non-NULL reason string.
  void TraceInline(Handle<JSFunction> target,
                   Handle<JSFunction> caller,
                   const char* failure_reason);

  void HandleGlobalVariableAssignment(Variable* var,
                                      HValue* value,
                                      BailoutId ast_id);

  void HandlePropertyAssignment(Assignment* expr);
  void HandleCompoundAssignment(Assignment* expr);
  void HandlePolymorphicNamedFieldAccess(PropertyAccessType access_type,
                                         BailoutId ast_id,
                                         BailoutId return_id,
                                         HValue* object,
                                         HValue* value,
                                         SmallMapList* types,
                                         Handle<String> name);

  HValue* BuildAllocateExternalElements(
      ExternalArrayType array_type,
      bool is_zero_byte_offset,
      HValue* buffer, HValue* byte_offset, HValue* length);
  HValue* BuildAllocateFixedTypedArray(
      ExternalArrayType array_type, size_t element_size,
      ElementsKind fixed_elements_kind,
      HValue* byte_length, HValue* length);

  bool IsCallNewArrayInlineable(CallNew* expr);
  void BuildInlinedCallNewArray(CallNew* expr);

  class PropertyAccessInfo {
   public:
    PropertyAccessInfo(HOptimizedGraphBuilder* builder,
                       PropertyAccessType access_type,
                       Type* type,
                       Handle<String> name)
        : lookup_(builder->isolate()),
          builder_(builder),
          access_type_(access_type),
          type_(type),
          name_(name),
          access_(HObjectAccess::ForMap()) { }

    // Checkes whether this PropertyAccessInfo can be handled as a monomorphic
    // load named. It additionally fills in the fields necessary to generate the
    // lookup code.
    bool CanAccessMonomorphic();

    // Checks whether all types behave uniform when loading name. If all maps
    // behave the same, a single monomorphic load instruction can be emitted,
    // guarded by a single map-checks instruction that whether the receiver is
    // an instance of any of the types.
    // This method skips the first type in types, assuming that this
    // PropertyAccessInfo is built for types->first().
    bool CanAccessAsMonomorphic(SmallMapList* types);

    Handle<Map> map() {
      if (type_->Is(Type::Number())) {
        Context* context = current_info()->closure()->context();
        context = context->native_context();
        return handle(context->number_function()->initial_map());
      } else if (type_->Is(Type::Boolean())) {
        Context* context = current_info()->closure()->context();
        context = context->native_context();
        return handle(context->boolean_function()->initial_map());
      } else if (type_->Is(Type::String())) {
        Context* context = current_info()->closure()->context();
        context = context->native_context();
        return handle(context->string_function()->initial_map());
      } else {
        return type_->AsClass();
      }
    }
    Type* type() const { return type_; }
    Handle<String> name() const { return name_; }

    bool IsJSObjectFieldAccessor() {
      int offset;  // unused
      return Accessors::IsJSObjectFieldAccessor<Type>(type_, name_, &offset);
    }

    bool GetJSObjectFieldAccess(HObjectAccess* access) {
      int offset;
      if (Accessors::IsJSObjectFieldAccessor<Type>(type_, name_, &offset)) {
        if (type_->Is(Type::String())) {
          ASSERT(name_->Equals(isolate()->heap()->length_string()));
          *access = HObjectAccess::ForStringLength();
        } else if (type_->Is(Type::Array())) {
          ASSERT(name_->Equals(isolate()->heap()->length_string()));
          *access = HObjectAccess::ForArrayLength(map()->elements_kind());
        } else {
          *access = HObjectAccess::ForMapAndOffset(map(), offset);
        }
        return true;
      }
      return false;
    }

    bool has_holder() { return !holder_.is_null(); }
    bool IsLoad() const { return access_type_ == LOAD; }

    LookupResult* lookup() { return &lookup_; }
    Handle<JSObject> holder() { return holder_; }
    Handle<JSFunction> accessor() { return accessor_; }
    Handle<Object> constant() { return constant_; }
    Handle<Map> transition() { return handle(lookup_.GetTransitionTarget()); }
    HObjectAccess access() { return access_; }

   private:
    Type* ToType(Handle<Map> map) { return builder_->ToType(map); }
    Isolate* isolate() { return lookup_.isolate(); }
    CompilationInfo* current_info() { return builder_->current_info(); }

    bool LoadResult(Handle<Map> map);
    bool LookupDescriptor();
    bool LookupInPrototypes();
    bool IsCompatible(PropertyAccessInfo* other);

    void GeneralizeRepresentation(Representation r) {
      access_ = access_.WithRepresentation(
          access_.representation().generalize(r));
    }

    LookupResult lookup_;
    HOptimizedGraphBuilder* builder_;
    PropertyAccessType access_type_;
    Type* type_;
    Handle<String> name_;
    Handle<JSObject> holder_;
    Handle<JSFunction> accessor_;
    Handle<JSObject> api_holder_;
    Handle<Object> constant_;
    HObjectAccess access_;
  };

  HInstruction* BuildMonomorphicAccess(PropertyAccessInfo* info,
                                       HValue* object,
                                       HValue* checked_object,
                                       HValue* value,
                                       BailoutId ast_id,
                                       BailoutId return_id,
                                       bool can_inline_accessor = true);

  HInstruction* BuildNamedAccess(PropertyAccessType access,
                                 BailoutId ast_id,
                                 BailoutId reutrn_id,
                                 Expression* expr,
                                 HValue* object,
                                 Handle<String> name,
                                 HValue* value,
                                 bool is_uninitialized = false);

  void HandlePolymorphicCallNamed(Call* expr,
                                  HValue* receiver,
                                  SmallMapList* types,
                                  Handle<String> name);
  void HandleLiteralCompareTypeof(CompareOperation* expr,
                                  Expression* sub_expr,
                                  Handle<String> check);
  void HandleLiteralCompareNil(CompareOperation* expr,
                               Expression* sub_expr,
                               NilValue nil);

  enum PushBeforeSimulateBehavior {
    PUSH_BEFORE_SIMULATE,
    NO_PUSH_BEFORE_SIMULATE
  };

  HControlInstruction* BuildCompareInstruction(
      Token::Value op,
      HValue* left,
      HValue* right,
      Type* left_type,
      Type* right_type,
      Type* combined_type,
      HSourcePosition left_position,
      HSourcePosition right_position,
      PushBeforeSimulateBehavior push_sim_result,
      BailoutId bailout_id);

  HInstruction* BuildStringCharCodeAt(HValue* string,
                                      HValue* index);

  HValue* BuildBinaryOperation(
      BinaryOperation* expr,
      HValue* left,
      HValue* right,
      PushBeforeSimulateBehavior push_sim_result);
  HInstruction* BuildIncrement(bool returns_original_input,
                               CountOperation* expr);
  HInstruction* BuildKeyedGeneric(PropertyAccessType access_type,
                                  HValue* object,
                                  HValue* key,
                                  HValue* value);

  HInstruction* TryBuildConsolidatedElementLoad(HValue* object,
                                                HValue* key,
                                                HValue* val,
                                                SmallMapList* maps);

  LoadKeyedHoleMode BuildKeyedHoleMode(Handle<Map> map);

  HInstruction* BuildMonomorphicElementAccess(HValue* object,
                                              HValue* key,
                                              HValue* val,
                                              HValue* dependency,
                                              Handle<Map> map,
                                              PropertyAccessType access_type,
                                              KeyedAccessStoreMode store_mode);

  HValue* HandlePolymorphicElementAccess(HValue* object,
                                         HValue* key,
                                         HValue* val,
                                         SmallMapList* maps,
                                         PropertyAccessType access_type,
                                         KeyedAccessStoreMode store_mode,
                                         bool* has_side_effects);

  HValue* HandleKeyedElementAccess(HValue* obj,
                                   HValue* key,
                                   HValue* val,
                                   Expression* expr,
                                   PropertyAccessType access_type,
                                   bool* has_side_effects);

  HInstruction* BuildNamedGeneric(PropertyAccessType access,
                                  HValue* object,
                                  Handle<String> name,
                                  HValue* value,
                                  bool is_uninitialized = false);

  HCheckMaps* AddCheckMap(HValue* object, Handle<Map> map);

  void BuildLoad(Property* property,
                 BailoutId ast_id);
  void PushLoad(Property* property,
                HValue* object,
                HValue* key);

  void BuildStoreForEffect(Expression* expression,
                           Property* prop,
                           BailoutId ast_id,
                           BailoutId return_id,
                           HValue* object,
                           HValue* key,
                           HValue* value);

  void BuildStore(Expression* expression,
                  Property* prop,
                  BailoutId ast_id,
                  BailoutId return_id,
                  bool is_uninitialized = false);

  HInstruction* BuildLoadNamedField(PropertyAccessInfo* info,
                                    HValue* checked_object);
  HInstruction* BuildStoreNamedField(PropertyAccessInfo* info,
                                     HValue* checked_object,
                                     HValue* value);

  HValue* BuildContextChainWalk(Variable* var);

  HInstruction* BuildThisFunction();

  HInstruction* BuildFastLiteral(Handle<JSObject> boilerplate_object,
                                 AllocationSiteUsageContext* site_context);

  void BuildEmitObjectHeader(Handle<JSObject> boilerplate_object,
                             HInstruction* object);

  void BuildInitElementsInObjectHeader(Handle<JSObject> boilerplate_object,
                                       HInstruction* object,
                                       HInstruction* object_elements);

  void BuildEmitInObjectProperties(Handle<JSObject> boilerplate_object,
                                   HInstruction* object,
                                   AllocationSiteUsageContext* site_context,
                                   PretenureFlag pretenure_flag);

  void BuildEmitElements(Handle<JSObject> boilerplate_object,
                         Handle<FixedArrayBase> elements,
                         HValue* object_elements,
                         AllocationSiteUsageContext* site_context);

  void BuildEmitFixedDoubleArray(Handle<FixedArrayBase> elements,
                                 ElementsKind kind,
                                 HValue* object_elements);

  void BuildEmitFixedArray(Handle<FixedArrayBase> elements,
                           ElementsKind kind,
                           HValue* object_elements,
                           AllocationSiteUsageContext* site_context);

  void AddCheckPrototypeMaps(Handle<JSObject> holder,
                             Handle<Map> receiver_map);

  HInstruction* NewPlainFunctionCall(HValue* fun,
                                     int argument_count,
                                     bool pass_argument_count);

  HInstruction* NewArgumentAdaptorCall(HValue* fun, HValue* context,
                                       int argument_count,
                                       HValue* expected_param_count);

  HInstruction* BuildCallConstantFunction(Handle<JSFunction> target,
                                          int argument_count);

  // The translation state of the currently-being-translated function.
  FunctionState* function_state_;

  // The base of the function state stack.
  FunctionState initial_function_state_;

  // Expression context of the currently visited subexpression. NULL when
  // visiting statements.
  AstContext* ast_context_;

  // A stack of breakable statements entered.
  BreakAndContinueScope* break_scope_;

  int inlined_count_;
  ZoneList<Handle<Object> > globals_;

  bool inline_bailout_;

  HOsrBuilder* osr_;

  friend class FunctionState;  // Pushes and pops the state stack.
  friend class AstContext;  // Pushes and pops the AST context stack.
  friend class KeyedLoadFastElementStub;
  friend class HOsrBuilder;

  DISALLOW_COPY_AND_ASSIGN(HOptimizedGraphBuilder);
};


Zone* AstContext::zone() const { return owner_->zone(); }


class HStatistics V8_FINAL: public Malloced {
 public:
  HStatistics()
      : times_(5),
        names_(5),
        sizes_(5),
        total_size_(0),
        source_size_(0) { }

  void Initialize(CompilationInfo* info);
  void Print();
  void SaveTiming(const char* name, TimeDelta time, unsigned size);

  void IncrementFullCodeGen(TimeDelta full_code_gen) {
    full_code_gen_ += full_code_gen;
  }

  void IncrementSubtotals(TimeDelta create_graph,
                          TimeDelta optimize_graph,
                          TimeDelta generate_code) {
    create_graph_ += create_graph;
    optimize_graph_ += optimize_graph;
    generate_code_ += generate_code;
  }

 private:
  List<TimeDelta> times_;
  List<const char*> names_;
  List<unsigned> sizes_;
  TimeDelta create_graph_;
  TimeDelta optimize_graph_;
  TimeDelta generate_code_;
  unsigned total_size_;
  TimeDelta full_code_gen_;
  double source_size_;
};


class HPhase : public CompilationPhase {
 public:
  HPhase(const char* name, HGraph* graph)
      : CompilationPhase(name, graph->info()),
        graph_(graph) { }
  ~HPhase();

 protected:
  HGraph* graph() const { return graph_; }

 private:
  HGraph* graph_;

  DISALLOW_COPY_AND_ASSIGN(HPhase);
};


class HTracer V8_FINAL : public Malloced {
 public:
  explicit HTracer(int isolate_id)
      : trace_(&string_allocator_), indent_(0) {
    if (FLAG_trace_hydrogen_file == NULL) {
      OS::SNPrintF(filename_,
                   "hydrogen-%d-%d.cfg",
                   OS::GetCurrentProcessId(),
                   isolate_id);
    } else {
      OS::StrNCpy(filename_, FLAG_trace_hydrogen_file, filename_.length());
    }
    WriteChars(filename_.start(), "", 0, false);
  }

  void TraceCompilation(CompilationInfo* info);
  void TraceHydrogen(const char* name, HGraph* graph);
  void TraceLithium(const char* name, LChunk* chunk);
  void TraceLiveRanges(const char* name, LAllocator* allocator);

 private:
  class Tag V8_FINAL BASE_EMBEDDED {
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

  void TraceLiveRange(LiveRange* range, const char* type, Zone* zone);
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

  EmbeddedVector<char, 64> filename_;
  HeapStringAllocator string_allocator_;
  StringStream trace_;
  int indent_;
};


class NoObservableSideEffectsScope V8_FINAL {
 public:
  explicit NoObservableSideEffectsScope(HGraphBuilder* builder) :
      builder_(builder) {
    builder_->graph()->IncrementInNoSideEffectsScope();
  }
  ~NoObservableSideEffectsScope() {
    builder_->graph()->DecrementInNoSideEffectsScope();
  }

 private:
  HGraphBuilder* builder_;
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_H_
