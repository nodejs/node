// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_

#include <optional>
#include <type_traits>
#include <utility>

#include "src/base/base-export.h"
#include "src/base/functional/function-ref.h"
#include "src/base/logging.h"
#include "src/base/vector.h"
#include "src/codegen/source-position-table.h"
#include "src/common/globals.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/processed-feedback.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/flags/flags.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-reducer.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/elements-kind.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {
namespace maglev {

class CallArguments;

struct CatchBlockDetails {
  BasicBlockRef* ref = nullptr;
  bool exception_handler_was_used = false;
  bool block_already_exists = false;
  int deopt_frame_distance = 0;
};

struct MaglevCallerDetails {
  base::Vector<ValueNode*> arguments;
  DeoptFrame* deopt_frame;
  KnownNodeAspects* known_node_aspects;
  LoopEffects* loop_effects;
  ZoneUnorderedMap<KnownNodeAspects::LoadedContextSlotsKey, Node*>
      unobserved_context_slot_stores;
  CatchBlockDetails catch_block;
  bool is_inside_loop;
  bool is_eager_inline;
  float call_frequency;
};

struct MaglevCallSiteInfo {
  MaglevCallerDetails caller_details;
  CallKnownJSFunction* generic_call_node;
  compiler::FeedbackCellRef feedback_cell;
  float score;
  int bytecode_length;
};

class MaglevGraphBuilder {
 public:
  class EagerDeoptFrameScope;
  class LazyDeoptFrameScope;

  class V8_NODISCARD LazyDeoptResultLocationScope {
   public:
    LazyDeoptResultLocationScope(MaglevGraphBuilder* builder,
                                 interpreter::Register result_location,
                                 int result_size);
    ~LazyDeoptResultLocationScope();

    interpreter::Register result_location() { return result_location_; }

    int result_size() const { return result_size_; }

   private:
    MaglevGraphBuilder* builder_;
    LazyDeoptResultLocationScope* previous_;
    interpreter::Register result_location_;
    int result_size_;
  };

  explicit MaglevGraphBuilder(LocalIsolate* local_isolate,
                              MaglevCompilationUnit* compilation_unit,
                              Graph* graph,
                              MaglevCallerDetails* caller_details = nullptr);

  bool Build();

  ReduceResult BuildInlineFunction(SourcePosition call_site_position,
                                   ValueNode* context, ValueNode* function,
                                   ValueNode* new_target);

  void StartPrologue();
  void SetArgument(int i, ValueNode* value);
  void InitializeRegister(interpreter::Register reg, ValueNode* value);
  ValueNode* GetArgument(int i);
  ValueNode* GetInlinedArgument(int i);
  void BuildRegisterFrameInitialization(ValueNode* context = nullptr,
                                        ValueNode* closure = nullptr,
                                        ValueNode* new_target = nullptr);
  void BuildMergeStates();
  BasicBlock* EndPrologue();
  void PeelLoop();
  void BuildLoopForPeeling();

  void OsrAnalyzePrequel();

  void BuildBody();

  SmiConstant* GetSmiConstant(int constant) const {
    return graph()->GetSmiConstant(constant);
  }
  TaggedIndexConstant* GetTaggedIndexConstant(int constant) {
    return graph()->GetTaggedIndexConstant(constant);
  }
  Int32Constant* GetInt32Constant(int32_t constant) {
    return graph()->GetInt32Constant(constant);
  }
  IntPtrConstant* GetIntPtrConstant(intptr_t constant) {
    return graph()->GetIntPtrConstant(constant);
  }
  Uint32Constant* GetUint32Constant(int constant) {
    return graph()->GetUint32Constant(constant);
  }
  Float64Constant* GetFloat64Constant(double constant) {
    return graph()->GetFloat64Constant(constant);
  }
  Float64Constant* GetFloat64Constant(Float64 constant) {
    return graph()->GetFloat64Constant(constant);
  }
  RootConstant* GetRootConstant(RootIndex index) {
    return graph()->GetRootConstant(index);
  }
  RootConstant* GetBooleanConstant(bool value) {
    return graph()->GetBooleanConstant(value);
  }
  ValueNode* GetConstant(compiler::ObjectRef ref) {
    return graph()->GetConstant(ref);
  }
  ValueNode* GetTrustedConstant(compiler::HeapObjectRef ref,
                                IndirectPointerTag tag) {
    return graph()->GetTrustedConstant(ref, tag);
  }

  Graph* graph() const { return graph_; }
  Zone* zone() const { return compilation_unit_->zone(); }
  MaglevCompilationUnit* compilation_unit() const { return compilation_unit_; }
  const InterpreterFrameState& current_interpreter_frame() const {
    return current_interpreter_frame_;
  }
  MaglevCallerDetails* caller_details() const { return caller_details_; }
  const LazyDeoptFrameScope* current_lazy_deopt_scope() const {
    return current_lazy_deopt_scope_;
  }
  compiler::JSHeapBroker* broker() const { return broker_; }
  LocalIsolate* local_isolate() const { return local_isolate_; }

  bool has_graph_labeller() const { return graph_->has_graph_labeller(); }
  MaglevGraphLabeller* graph_labeller() const {
    if (graph_->has_graph_labeller()) return graph_->graph_labeller();
    return nullptr;
  }

  MaglevReducer<MaglevGraphBuilder>& reducer() { return reducer_; }

  // True when this graph builder is building the subgraph of an inlined
  // function.
  bool is_inline() const { return caller_details_ != nullptr; }
  int inlining_depth() const { return compilation_unit_->inlining_depth(); }

  bool is_eager_inline() const {
    DCHECK(is_inline());
    DCHECK_IMPLIES(!caller_details_->is_eager_inline,
                   v8_flags.maglev_non_eager_inlining ||
                       v8_flags.turbolev_non_eager_inlining);
    return caller_details_->is_eager_inline;
  }

  DeoptFrame* GetLatestCheckpointedFrame();
  DeoptFrame* GetDeoptFrameForEagerDeopt() {
    return GetLatestCheckpointedFrame();
  }
  std::tuple<DeoptFrame*, interpreter::Register, int>
  GetDeoptFrameForLazyDeopt();

  bool need_checkpointed_loop_entry() {
    return v8_flags.maglev_speculative_hoist_phi_untagging ||
           v8_flags.maglev_licm;
  }

  SourcePosition GetCurrentSourcePosition() const;

  BasicBlock* current_block() const;
  void set_current_block(BasicBlock* block);
  BasicBlock* FinishInlinedBlockForCaller(
      ControlNode* control_node, ZoneVector<Node*> rem_nodes_in_call_block);

  uint32_t NewObjectId() { return graph_->NewObjectId(); }

  bool is_turbolev() const { return is_turbolev_; }

  bool should_abort_compilation() const { return should_abort_compilation_; }

  bool is_non_eager_inlining_enabled() const {
    if (is_turbolev()) {
      return v8_flags.turbolev_non_eager_inlining;
    }
    return v8_flags.maglev_non_eager_inlining;
  }

  // Inlining configuration. For Maglev, we use the Maglev flags, and for
  // Turbolev, we use the Turbofan flags.
  int max_inlined_bytecode_size() {
    if (is_turbolev()) {
      return v8_flags.max_inlined_bytecode_size;
    } else {
      return v8_flags.max_maglev_inlined_bytecode_size;
    }
  }
  int max_inlined_bytecode_size_small() {
    if (is_turbolev()) {
      return v8_flags.max_inlined_bytecode_size_small;
    } else {
      return v8_flags.max_maglev_inlined_bytecode_size_small;
    }
  }
  int max_inlined_bytecode_size_small_with_heapnum_in_out() {
    if (is_turbolev()) {
      return v8_flags.max_inlined_bytecode_size_small_with_heapnum_in_out;
    } else {
      return v8_flags
          .max_maglev_inlined_bytecode_size_small_with_heapnum_in_out;
    }
  }
  float min_inlining_frequency() {
    if (is_turbolev()) {
      return v8_flags.min_inlining_frequency;
    } else {
      return v8_flags.min_maglev_inlining_frequency;
    }
  }
  int max_inlined_bytecode_size_cumulative() {
    if (is_turbolev()) {
      return v8_flags.max_inlined_bytecode_size_cumulative;
    } else {
      return v8_flags.max_maglev_inlined_bytecode_size_cumulative;
    }
  }
  int max_inlined_bytecode_size_small_total() {
    if (is_turbolev()) {
      return v8_flags.max_inlined_bytecode_size_small_total;
    } else {
      return v8_flags.max_maglev_inlined_bytecode_size_small_total;
    }
  }
  int max_inline_depth() {
    if (is_turbolev()) {
      // This is just to avoid some corner cases, especially since we allow
      // recursive inlining.
      constexpr int kMaxDepthForInlining = 50;
      return kMaxDepthForInlining;
    } else {
      return v8_flags.max_maglev_inline_depth;
    }
  }
  // We allow small functions to be inlined deeper than regular functions.
  int max_inline_depth_small() {
    if (is_turbolev()) {
      // For Turbolev, small and normal functions can all be inlined at the same
      // depth.
      return max_inline_depth();
    } else {
      return v8_flags.max_maglev_hard_inline_depth;
    }
  }

  bool is_inline_api_calls_enabled() const {
    // TODO(victorgomes): Inline API calls are still not supported by Turbolev.
    return !is_turbolev() && v8_flags.maglev_inline_api_calls;
  }

  bool is_tracing_enabled() const {
    return compilation_unit_->info()->is_tracing_enabled();
  }

  KnownNodeAspects& known_node_aspects() {
    return *current_interpreter_frame_.known_node_aspects();
  }

  DeoptFrame* AddInlinedArgumentsToDeoptFrame(DeoptFrame* deopt_frame,
                                              const MaglevCompilationUnit* unit,
                                              ValueNode* closure,
                                              base::Vector<ValueNode*> args);

  std::pair<interpreter::Register, int> GetResultLocationAndSize() const;

  void AttachExceptionHandlerInfo(Node* node);

  template <typename NodeT>
  void MarkPossibleSideEffect(NodeT* node);

  template <bool is_possible_map_change = true>
  void ResetBuilderCachedState();

 private:
  class DeoptFrameScopeBase;

  // Helper class for building a subgraph with its own control flow, that is not
  // attached to any bytecode.
  //
  // It does this by creating a fake dummy compilation unit and frame state, and
  // wrapping up all the places where it pretends to be interpreted but isn't.
  class MaglevSubGraphBuilder {
   public:
    class Variable;
    class Label;
    class LoopLabel;

    MaglevSubGraphBuilder(MaglevGraphBuilder* builder, int variable_count);
    LoopLabel BeginLoop(std::initializer_list<Variable*> loop_vars);
    template <typename ControlNodeT, typename... Args>
    void GotoIfTrue(Label* true_target,
                    std::initializer_list<ValueNode*> control_inputs,
                    Args&&... args);
    template <typename ControlNodeT, typename... Args>
    void GotoIfFalse(Label* false_target,
                     std::initializer_list<ValueNode*> control_inputs,
                     Args&&... args);
    void GotoOrTrim(Label* label);
    void Goto(Label* label);
    void ReducePredecessorCount(Label* label, unsigned num = 1);
    void EndLoop(LoopLabel* loop_label);
    void Bind(Label* label);
    V8_NODISCARD ReduceResult TrimPredecessorsAndBind(Label* label);
    void set(Variable& var, ValueNode* value);
    ValueNode* get(const Variable& var) const;

    template <typename FCond, typename FTrue, typename FFalse>
    ReduceResult Branch(std::initializer_list<Variable*> vars, FCond cond,
                        FTrue if_true, FFalse if_false);

    void MergeIntoLabel(Label* label, BasicBlock* predecessor);

   private:
    class BorrowParentKnownNodeAspectsAndVOs;
    void TakeKnownNodeAspectsAndVOsFromParent();
    void MoveKnownNodeAspectsAndVOsToParent();

    MaglevGraphBuilder* builder_;
    MaglevCompilationUnit* compilation_unit_;
    InterpreterFrameState pseudo_frame_;
  };

  // TODO(olivf): Currently identifying dead code relies on the fact that loops
  // must be entered through the loop header by at least one of the
  // predecessors. We might want to re-evaluate this in case we want to be able
  // to OSR into nested loops while compiling the full continuation.
  static constexpr bool kLoopsMustBeEnteredThroughHeader = true;

  class CallSpeculationScope;
  class SaveCallSpeculationScope;

  bool CheckType(ValueNode* node, NodeType type, NodeType* old = nullptr) {
    return known_node_aspects().CheckType(broker(), node, type, old);
  }
  NodeType CheckTypes(ValueNode* node, std::initializer_list<NodeType> types) {
    return known_node_aspects().CheckTypes(broker(), node, types);
  }
  bool EnsureType(ValueNode* node, NodeType type, NodeType* old = nullptr) {
    return known_node_aspects().EnsureType(broker(), node, type, old);
  }
  template <typename Function>
  bool EnsureType(ValueNode* node, NodeType type, Function ensure_new_type) {
    return known_node_aspects().EnsureType<Function>(broker(), node, type,
                                                     ensure_new_type);
  }
  NodeType GetType(ValueNode* node) {
    return known_node_aspects().GetType(broker(), node);
  }
  NodeInfo* GetOrCreateInfoFor(ValueNode* node) {
    return known_node_aspects().GetOrCreateInfoFor(broker(), node);
  }

  // Returns true if we statically know that {lhs} and {rhs} have disjoint
  // types.
  bool HaveDisjointTypes(ValueNode* lhs, ValueNode* rhs) {
    return known_node_aspects().HaveDisjointTypes(broker(), lhs, rhs);
  }
  bool HasDisjointType(ValueNode* lhs, NodeType rhs_type) {
    return known_node_aspects().HasDisjointType(broker(), lhs, rhs_type);
  }

  void SetKnownValue(ValueNode* node, compiler::ObjectRef constant,
                     NodeType new_node_type);
  bool ShouldEmitInterruptBudgetChecks();
  bool ShouldEmitOsrInterruptBudgetChecks();

  bool MaglevIsTopTier() const { return !v8_flags.turbofan && v8_flags.maglev; }
  BasicBlock* CreateEdgeSplitBlock(BasicBlockRef& jump_targets,
                                   BasicBlock* predecessor);

  void ProcessMergePointAtExceptionHandlerStart(int offset);

  void ProcessMergePoint(int offset, bool preserve_known_node_aspects);

  // Splits incoming critical edges and labels predecessors.
  void ProcessMergePointPredecessors(
      MergePointInterpreterFrameState& merge_state,
      BasicBlockRef& jump_targets);

  void RegisterPhisWithGraphLabeller(
      MergePointInterpreterFrameState& merge_state);

  // Return true if the given offset is a merge point, i.e. there are jumps
  // targetting it.
  bool IsOffsetAMergePoint(int offset) {
    return merge_states_[offset] != nullptr;
  }

  bool IsOffsetAMergePointOrLoopHeapder(int offset) {
    return IsOffsetAMergePoint(offset) ||
           bytecode_analysis().IsLoopHeader(offset);
  }

  ValueNode* GetContextAtDepth(ValueNode* context, size_t depth);
  bool CheckContextExtensions(size_t depth);

  // Called when a block is killed by an unconditional eager deopt.
  ReduceResult EmitUnconditionalDeopt(DeoptimizeReason reason);

  void KillPeeledLoopTargets(int peelings);

  void MarkBytecodeDead();

  void UpdateSourceAndBytecodePosition(int offset);

  void PrintVirtualObjects();

  ReduceResult VisitSingleBytecode();

#define BYTECODE_VISITOR(name, ...) ReduceResult Visit##name();
  BYTECODE_LIST(BYTECODE_VISITOR, BYTECODE_VISITOR)
#undef BYTECODE_VISITOR

#define DECLARE_VISITOR(name, ...) \
  ReduceResult VisitIntrinsic##name(interpreter::RegisterList args);
  INTRINSICS_LIST(DECLARE_VISITOR)
#undef DECLARE_VISITOR

  // Add a new node with a dynamic set of inputs which are initialized by the
  // `post_create_input_initializer` function before the node is added to the
  // graph.
  template <typename NodeT, typename Function, typename... Args>
  NodeT* AddNewNode(size_t input_count,
                    Function&& post_create_input_initializer, Args&&... args);
  // Add a new node with a static set of inputs.
  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(std::initializer_list<ValueNode*> inputs, Args&&... args);
  template <typename NodeT, typename... Args>
  NodeT* AddNewNodeNoInputConversion(std::initializer_list<ValueNode*> inputs,
                                     Args&&... args);

  // Bytecode iterator of the current graph builder is inside a try-block
  // region.
  bool IsInsideTryBlock() const { return catch_block_stack_.size() > 0; }

  MergePointInterpreterFrameState* GetCatchBlockFrameState() {
    DCHECK(IsInsideTryBlock());
    return merge_states_[catch_block_stack_.top().handler];
  }
  CatchBlockDetails GetCurrentTryCatchBlock();
  CatchBlockDetails GetTryCatchBlockForNonEagerInlining(
      ExceptionHandlerInfo* info);

  bool ContextMayAlias(ValueNode* context,
                       compiler::OptionalScopeInfoRef scope_info);
  bool TrySpecializeLoadContextSlotToFunctionContext(
      ValueNode* context, int slot_index,
      ContextSlotMutability slot_mutability);
  ValueNode* TrySpecializeLoadContextSlot(ValueNode* context, int index);
  ValueNode* LoadAndCacheContextSlot(ValueNode* context, int offset,
                                     ContextSlotMutability slot_mutability,
                                     ContextMode context_mode);
  MaybeReduceResult TrySpecializeStoreContextSlot(ValueNode* context, int index,
                                                  ValueNode* value,
                                                  Node** store);
  ReduceResult StoreAndCacheContextSlot(ValueNode* context, int index,
                                        ValueNode* value,
                                        ContextMode context_mode);
  ValueNode* TryGetParentContext(ValueNode* node);
  void MinimizeContextChainDepth(ValueNode** context, size_t* depth);
  void EscapeContext();
  void BuildLoadContextSlot(ValueNode* context, size_t depth, int slot_index,
                            ContextSlotMutability slot_mutability,
                            ContextMode context_mode);
  ReduceResult BuildStoreContextSlot(ValueNode* context, size_t depth,
                                     int slot_index, ValueNode* value,
                                     ContextMode context_mode);

  ReduceResult BuildStoreMap(ValueNode* object, compiler::MapRef map,
                             StoreMap::Kind kind);

  ReduceResult BuildExtendPropertiesBackingStore(compiler::MapRef map,
                                                 ValueNode* receiver,
                                                 ValueNode* property_array);

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(std::initializer_list<ValueNode*> inputs);

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(
      std::initializer_list<ValueNode*> inputs,
      compiler::FeedbackSource const& feedback,
      CallBuiltin::FeedbackSlotType slot_type = CallBuiltin::kTaggedIndex);

  CallCPPBuiltin* BuildCallCPPBuiltin(Builtin builtin, ValueNode* target,
                                      ValueNode* new_target,
                                      std::initializer_list<ValueNode*> inputs);

  ReduceResult BuildLoadGlobal(compiler::NameRef name,
                               compiler::FeedbackSource& feedback_source,
                               TypeofMode typeof_mode);

  ValueNode* BuildToString(ValueNode* value, ToString::ConversionMode mode);

  constexpr bool RuntimeFunctionCanThrow(Runtime::FunctionId function_id) {
#define BAILOUT(name, ...)               \
  if (function_id == Runtime::k##name) { \
    return true;                         \
  }
    FOR_EACH_THROWING_INTRINSIC(BAILOUT)
#undef BAILOUT
    return false;
  }

  ReduceResult BuildCallRuntime(Runtime::FunctionId function_id,
                                std::initializer_list<ValueNode*> inputs);

  ReduceResult BuildAbort(AbortReason reason);

  void Print(const char* str);
  void Print(ValueNode* value);
  void Print(const char* str, ValueNode* value);

  ValueNode* GetFeedbackCell() {
    return graph()->GetConstant(
        compilation_unit_->GetTopLevelCompilationUnit()->feedback_cell());
  }

  ValueNode* GetClosure() const {
    return current_interpreter_frame_.get(
        interpreter::Register::function_closure());
  }

  ValueNode* GetContext() const {
    return current_interpreter_frame_.get(
        interpreter::Register::current_context());
  }

  void SetContext(ValueNode* context) {
    current_interpreter_frame_.set(interpreter::Register::current_context(),
                                   context);
  }

  FeedbackSlot GetSlotOperand(int operand_index) const {
    return iterator_.GetSlotOperand(operand_index);
  }

  uint32_t GetFlag8Operand(int operand_index) const {
    return iterator_.GetFlag8Operand(operand_index);
  }

  uint32_t GetFlag16Operand(int operand_index) const {
    return iterator_.GetFlag16Operand(operand_index);
  }

  template <class T>
  typename compiler::ref_traits<T>::ref_type GetRefOperand(int operand_index)
    requires(is_taggable_v<T>)
  {
    // The BytecodeArray itself was fetched by using a barrier so all reads
    // from the constant pool are safe.
    return MakeRefAssumeMemoryFence(
        broker(), broker()->CanonicalPersistentHandle(
                      Cast<T>(iterator_.GetConstantForIndexOperand(
                          operand_index, local_isolate()))));
  }

  MaybeReduceResult GetConstantSingleCharacterStringFromCode(uint16_t);

  ValueNode* GetRegisterInput(Register reg);

#define DEFINE_IS_ROOT_OBJECT(type, name, CamelName)               \
  bool Is##CamelName(ValueNode* value) const {                     \
    if (RootConstant* constant = value->TryCast<RootConstant>()) { \
      return constant->index() == RootIndex::k##CamelName;         \
    }                                                              \
    return false;                                                  \
  }
  ROOT_LIST(DEFINE_IS_ROOT_OBJECT)
#undef DEFINE_IS_ROOT_OBJECT

  // Move an existing ValueNode between two registers. You can pass
  // virtual_accumulator as the src or dst to move in or out of the accumulator.
  void MoveNodeBetweenRegisters(interpreter::Register src,
                                interpreter::Register dst) {
    // We shouldn't be moving newly created nodes between registers.
    DCHECK(!IsNodeCreatedForThisBytecode(current_interpreter_frame_.get(src)));
    DCHECK_NOT_NULL(current_interpreter_frame_.get(src));

    current_interpreter_frame_.set(dst, current_interpreter_frame_.get(src));
  }

  ValueNode* GetTaggedValue(ValueNode* value,
                            UseReprHintRecording record_use_repr_hint =
                                UseReprHintRecording::kRecord);

  ReduceResult GetSmiValue(ValueNode* value,
                           UseReprHintRecording record_use_repr_hint =
                               UseReprHintRecording::kRecord);
  ReduceResult GetSmiValue(ReduceResult value_result,
                           UseReprHintRecording record_use_repr_hint =
                               UseReprHintRecording::kRecord);

  MaybeReduceResult GetSmiValue(interpreter::Register reg,
                                UseReprHintRecording record_use_repr_hint =
                                    UseReprHintRecording::kRecord) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    return GetSmiValue(value, record_use_repr_hint);
  }

  ValueNode* GetTaggedValue(interpreter::Register reg,
                            UseReprHintRecording record_use_repr_hint =
                                UseReprHintRecording::kRecord);

  ReduceResult GetInternalizedString(interpreter::Register reg);

  // Get an Int32 representation node whose value is equivalent to the ToInt32
  // truncation of the given node (including a ToNumber call). Only trivial
  // ToNumber is allowed -- values that are already numeric, and optionally
  // oddballs.
  //
  // Deopts if the ToNumber is non-trivial.
  ValueNode* GetTruncatedInt32ForToNumber(ValueNode* value,
                                          NodeType allowed_input_type);

  ValueNode* GetTruncatedInt32ForToNumber(interpreter::Register reg,
                                          NodeType allowed_input_type) {
    return GetTruncatedInt32ForToNumber(current_interpreter_frame_.get(reg),
                                        allowed_input_type);
  }

  // Get an Int32 representation node whose value is equivalent to the ToUint8
  // truncation of the given node (including a ToNumber call). Only trivial
  // ToNumber is allowed -- values that are already numeric, and optionally
  // oddballs.
  //
  // Deopts if the ToNumber is non-trivial.
  ValueNode* GetUint8ClampedForToNumber(ValueNode* value);

  ValueNode* GetUint8ClampedForToNumber(interpreter::Register reg) {
    return GetUint8ClampedForToNumber(current_interpreter_frame_.get(reg));
  }

  std::optional<int32_t> TryGetInt32Constant(ValueNode* value);
  std::optional<uint32_t> TryGetUint32Constant(ValueNode* value);
  MaybeHandle<String> TryGetStringConstant(ValueNode* value);

  // Get an Int32 representation node whose value is equivalent to the given
  // node.
  ValueNode* GetInt32(ValueNode* value, bool can_be_heap_number = false);
  ReduceResult GetInt32(ReduceResult value_result,
                        bool can_be_heap_number = false);

  void EnsureInt32(ValueNode* value, bool can_be_heap_number = false);

  void EnsureInt32(interpreter::Register reg);

  std::optional<double> TryGetFloat64Constant(
      ValueNode* value, TaggedToFloat64ConversionType conversion_type);

#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
  std::optional<double> TryGetHoleyFloat64Constant(ValueNode* value);
#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

  // Get a Float64 representation node whose value is equivalent to the given
  // node.
  //
  // Deopts if the value is not exactly representable as a Float64.
  ValueNode* GetFloat64(ValueNode* value);
  ValueNode* GetFloat64(interpreter::Register reg);

  ValueNode* GetHoleyFloat64(ValueNode* value,
                             TaggedToFloat64ConversionType conversion_type,
                             bool convert_hole_to_undefined);

  // Get a Float64 representation node whose value is the result of ToNumber on
  // the given node. Only trivial ToNumber is allowed -- values that are already
  // numeric, and optionally oddballs.
  //
  // Deopts if the ToNumber value is not exactly representable as a Float64, or
  // the ToNumber is non-trivial.
  ValueNode* GetFloat64ForToNumber(ValueNode* value,
                                   NodeType allowed_input_type);
  ValueNode* GetFloat64ForToNumber(interpreter::Register reg,
                                   NodeType allowed_input_type);

  ValueNode* GetHoleyFloat64ForToNumber(ValueNode* value,
                                        NodeType allowed_input_type);
  ValueNode* GetHoleyFloat64ForToNumber(interpreter::Register reg,
                                        NodeType allowed_input_type) {
    return GetHoleyFloat64ForToNumber(current_interpreter_frame_.get(reg),
                                      allowed_input_type);
  }

  ValueNode* GetAccumulator() {
    return current_interpreter_frame_.get(
        interpreter::Register::virtual_accumulator());
  }

  MaybeReduceResult GetAccumulatorSmi(
      UseReprHintRecording record_use_repr_hint =
          UseReprHintRecording::kRecord) {
    return GetSmiValue(interpreter::Register::virtual_accumulator(),
                       record_use_repr_hint);
  }

  ValueNode* GetAccumulatorTruncatedInt32ForToNumber(
      NodeType allowed_input_type) {
    return GetTruncatedInt32ForToNumber(
        interpreter::Register::virtual_accumulator(), allowed_input_type);
  }

  ValueNode* GetAccumulatorUint8ClampedForToNumber() {
    return GetUint8ClampedForToNumber(
        interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorHoleyFloat64ForToNumber(
      NodeType allowed_input_type) {
    return GetHoleyFloat64ForToNumber(
        interpreter::Register::virtual_accumulator(), allowed_input_type);
  }

  ValueNode* GetSilencedNaN(ValueNode* value);

  bool IsRegisterEqualToAccumulator(int operand_index) {
    interpreter::Register source = iterator_.GetRegisterOperand(operand_index);
    return current_interpreter_frame_.get(source) ==
           current_interpreter_frame_.accumulator();
  }

  ValueNode* LoadRegister(int operand_index) {
    return current_interpreter_frame_.get(
        iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterHoleyFloat64ForToNumber(int operand_index,
                                                 NodeType allowed_input_type) {
    return GetHoleyFloat64ForToNumber(
        iterator_.GetRegisterOperand(operand_index), allowed_input_type);
  }

  template <typename NodeT>
  void SetAccumulator(NodeT* node) {
    // Accumulator stores are equivalent to stores to the virtual accumulator
    // register.
    StoreRegister(interpreter::Register::virtual_accumulator(), node);
  }

  void ClobberAccumulator() {
    DCHECK(interpreter::Bytecodes::ClobbersAccumulator(
        iterator_.current_bytecode()));
    current_interpreter_frame_.set_accumulator(
        graph()->GetRootConstant(RootIndex::kOptimizedOut));
  }

  ValueNode* GetSecondValue(ValueNode* result);

  template <typename NodeT>
  void StoreRegister(interpreter::Register target, NodeT* value) {
    static_assert(std::is_base_of_v<ValueNode, NodeT>);
    DCHECK(HasOutputRegister(target));
    current_interpreter_frame_.set(target, value);

    // Make sure the lazy deopt info of this value, if any, is registered as
    // mutating this register.
    DCHECK_IMPLIES(value->properties().can_lazy_deopt() &&
                       IsNodeCreatedForThisBytecode(value),
                   value->lazy_deopt_info()->IsResultRegister(target));
  }

  void SetAccumulatorInBranch(ValueNode* value) {
    DCHECK_IMPLIES(value->properties().can_lazy_deopt(),
                   !IsNodeCreatedForThisBytecode(value));
    current_interpreter_frame_.set(interpreter::Register::virtual_accumulator(),
                                   value);
  }

  template <typename NodeT>
  void StoreRegisterPair(
      std::pair<interpreter::Register, interpreter::Register> target,
      NodeT* value);

#ifdef DEBUG
  bool HasOutputRegister(interpreter::Register reg) const;
#endif

  DeoptFrame* GetCallerDeoptFrame();
  DeoptFrame* GetDeoptFrameForEagerCall(const MaglevCompilationUnit* unit,
                                        ValueNode* closure,
                                        base::Vector<ValueNode*> args);
  DeoptFrame* GetDeoptFrameForLazyDeoptHelper(
      interpreter::Register result_location, int result_size,
      LazyDeoptFrameScope* scope, bool mark_accumulator_dead);
  InterpretedDeoptFrame* GetDeoptFrameForEntryStackCheck();

  int next_offset() const {
    return iterator_.current_offset() + iterator_.current_bytecode_size();
  }
  const compiler::BytecodeLivenessState* GetInLiveness() const {
    return GetInLivenessFor(iterator_.current_offset());
  }
  const compiler::BytecodeLivenessState* GetInLivenessFor(int offset) const {
    return bytecode_analysis().GetInLivenessFor(offset);
  }
  const compiler::BytecodeLivenessState* GetOutLiveness() const {
    return GetOutLivenessFor(iterator_.current_offset());
  }
  const compiler::BytecodeLivenessState* GetOutLivenessFor(int offset) const {
    return bytecode_analysis().GetOutLivenessFor(offset);
  }

  void StartNewBlock(int offset, BasicBlock* predecessor);
  void StartNewBlock(BasicBlock* predecessor,
                     MergePointInterpreterFrameState* merge_state,
                     BasicBlockRef& refs_to_block);
  void StartFallthroughBlock(int next_block_offset, BasicBlock* predecessor);
  template <typename ControlNodeT, typename... Args>
  BasicBlock* FinishBlock(std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args);

  ValueNode* GetValueOrUndefined(ValueNode* maybe_value) {
    if (maybe_value == nullptr) {
      return GetRootConstant(RootIndex::kUndefinedValue);
    }
    return maybe_value;
  }

  ValueNode* GetConvertReceiver(compiler::SharedFunctionInfoRef shared,
                                const CallArguments& args);
  base::Vector<ValueNode*> GetArgumentsAsArrayOfValueNodes(
      compiler::SharedFunctionInfoRef shared, const CallArguments& args);

  compiler::OptionalHeapObjectRef TryGetConstant(
      ValueNode* node, ValueNode** constant_node = nullptr);
  std::optional<ValueNode*> TryGetConstantAlternative(ValueNode* node);

  template <typename LoadNode>
  MaybeReduceResult TryBuildLoadDataView(const CallArguments& args,
                                         ExternalArrayType type);
  template <typename StoreNode, typename Function>
  MaybeReduceResult TryBuildStoreDataView(const CallArguments& args,
                                          ExternalArrayType type,
                                          Function&& getValue);

  MaybeReduceResult TryReduceDatePrototypeGetField(
      compiler::JSFunctionRef target, CallArguments& args,
      JSDate::FieldIndex field);

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
#define CONTINUATION_PRESERVED_EMBEDDER_DATA_LIST(V) \
  V(GetContinuationPreservedEmbedderData)            \
  V(SetContinuationPreservedEmbedderData)
#else
#define CONTINUATION_PRESERVED_EMBEDDER_DATA_LIST(V)
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

#define MAGLEV_REDUCED_BUILTIN(V)              \
  V(ArrayConstructor)                          \
  V(ArrayForEach)                              \
  V(ArrayIsArray)                              \
  V(ArrayIteratorPrototypeNext)                \
  V(ArrayMap)                                  \
  V(ArrayPrototypeAt)                          \
  V(ArrayPrototypeEntries)                     \
  V(ArrayPrototypeSlice)                       \
  V(ArrayPrototypeKeys)                        \
  V(ArrayPrototypeValues)                      \
  V(ArrayPrototypePush)                        \
  V(ArrayPrototypePop)                         \
  V(DataViewPrototypeGetInt8)                  \
  V(DataViewPrototypeSetInt8)                  \
  V(DataViewPrototypeGetInt16)                 \
  V(DataViewPrototypeSetInt16)                 \
  V(DataViewPrototypeGetInt32)                 \
  V(DataViewPrototypeSetInt32)                 \
  V(DataViewPrototypeGetFloat64)               \
  V(DataViewPrototypeSetFloat64)               \
  V(DatePrototypeGetFullYear)                  \
  V(DatePrototypeGetMonth)                     \
  V(DatePrototypeGetDate)                      \
  V(DatePrototypeGetDay)                       \
  V(DatePrototypeGetHours)                     \
  V(DatePrototypeGetMinutes)                   \
  V(DatePrototypeGetSeconds)                   \
  V(FunctionPrototypeApply)                    \
  V(FunctionPrototypeCall)                     \
  V(FunctionPrototypeHasInstance)              \
  V(MapPrototypeGet)                           \
  V(ObjectPrototypeGetProto)                   \
  V(ObjectGetPrototypeOf)                      \
  V(ReflectGetPrototypeOf)                     \
  V(ObjectPrototypeHasOwnProperty)             \
  V(NumberParseInt)                            \
  V(MathCeil)                                  \
  V(MathFloor)                                 \
  V(MathAbs)                                   \
  V(MathRound)                                 \
  V(MathSqrt)                                  \
  V(MathClz32)                                 \
  V(SetPrototypeHas)                           \
  V(StringConstructor)                         \
  V(StringFromCharCode)                        \
  V(StringPrototypeCharAt)                     \
  V(StringPrototypeCharCodeAt)                 \
  V(StringPrototypeCodePointAt)                \
  V(StringPrototypeStartsWith)                 \
  V(StringPrototypeIterator)                   \
  IF_INTL(V, StringPrototypeLocaleCompareIntl) \
  CONTINUATION_PRESERVED_EMBEDDER_DATA_LIST(V) \
  IEEE_754_UNARY_LIST(V)                       \
  IEEE_754_BINARY_LIST(V)

#define DEFINE_BUILTIN_REDUCER(Name, ...)                           \
  MaybeReduceResult TryReduce##Name(compiler::JSFunctionRef target, \
                                    CallArguments& args);
  MAGLEV_REDUCED_BUILTIN(DEFINE_BUILTIN_REDUCER)
#undef DEFINE_BUILTIN_REDUCER

  using InitialCallback = base::FunctionRef<ReduceResult(ValueNode*)>;
  using ProcessElementCallback =
      base::FunctionRef<void(ValueNode*, ValueNode*)>;
  using GetEagerDeoptScopeCallback = base::FunctionRef<EagerDeoptFrameScope(
      compiler::JSFunctionRef, ValueNode*, ValueNode*, ValueNode*, ValueNode*,
      ValueNode*, ValueNode*)>;
  using GetLazyDeoptScopeCallback = base::FunctionRef<LazyDeoptFrameScope(
      compiler::JSFunctionRef, ValueNode*, ValueNode*, ValueNode*, ValueNode*,
      ValueNode*, ValueNode*)>;

  // Used for reduding Array.prototype.forEach and Array.prototype.map.
  // initial_callback will be called to generate code before starting the
  // iteration, and process_element_callback will be called to generate code for
  // each result element.
  MaybeReduceResult TryReduceArrayIteratingBuiltin(
      const char* name, compiler::JSFunctionRef target, CallArguments& args,
      GetEagerDeoptScopeCallback get_eager_deopt_scope,
      GetLazyDeoptScopeCallback get_lazy_deopt_scope,
      const std::optional<InitialCallback>& initial_callback = {},
      const std::optional<ProcessElementCallback>& process_element_callback =
          {});

  // OOB StringAt access behaves differently for elements (needs the elements
  // protector, positive indices, and returns undefined) and charAt (allows
  // negative indices, returns empty string).
  enum class StringAtOOBMode { kElement, kCharAt };
  MaybeReduceResult TryReduceConstantStringAt(ValueNode* object,
                                              ValueNode* index,
                                              StringAtOOBMode oob_mode);

  MaybeReduceResult TryReduceGetProto(ValueNode* node);

  template <typename MapKindsT, typename IndexToElementsKindFunc,
            typename BuildKindSpecificFunc>
  MaybeReduceResult BuildJSArrayBuiltinMapSwitchOnElementsKind(
      ValueNode* receiver, const MapKindsT& map_kinds,
      MaglevSubGraphBuilder& sub_graph,
      std::optional<MaglevSubGraphBuilder::Label>& do_return,
      int unique_kind_count, IndexToElementsKindFunc&& index_to_elements_kind,
      BuildKindSpecificFunc&& build_kind_specific);

  MaybeReduceResult DoTryReduceMathRound(CallArguments& args,
                                         Float64Round::Kind kind);

  template <typename CallNode, typename... Args>
  CallNode* AddNewCallNode(const CallArguments& args, Args&&... extra_args);

  MaybeReduceResult TryReduceGetIterator(ValueNode* receiver, int load_slot,
                                         int call_slot);

  ValueNode* BuildCallSelf(ValueNode* context, ValueNode* function,
                           ValueNode* new_target,
                           compiler::SharedFunctionInfoRef shared,
                           CallArguments& args);
  MaybeReduceResult TryReduceBuiltin(
      compiler::JSFunctionRef target, compiler::SharedFunctionInfoRef shared,
      CallArguments& args, const compiler::FeedbackSource& feedback_source);
  bool TargetIsCurrentCompilingUnit(compiler::JSFunctionRef target);
  CallKnownJSFunction* BuildCallKnownJSFunction(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
#ifdef V8_ENABLE_LEAPTIERING
      JSDispatchHandle dispatch_handle,
#endif
      compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  CallKnownJSFunction* BuildCallKnownJSFunction(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
#ifdef V8_ENABLE_LEAPTIERING
      JSDispatchHandle dispatch_handle,
#endif
      compiler::SharedFunctionInfoRef shared,
      base::Vector<ValueNode*> arguments);
  MaybeReduceResult TryBuildCallKnownJSFunction(
      compiler::JSFunctionRef function, ValueNode* new_target,
      CallArguments& args, const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryBuildCallKnownJSFunction(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
#ifdef V8_ENABLE_LEAPTIERING
      JSDispatchHandle dispatch_handle,
#endif
      compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  bool CanInlineCall(compiler::SharedFunctionInfoRef shared,
                     float call_frequency);
  bool IsFunctionSmall(compiler::SharedFunctionInfoRef shared,
                       CallArguments& args);
  bool ShouldEagerInlineCall(compiler::SharedFunctionInfoRef shared,
                             CallArguments& args);
  ReduceResult BuildEagerInlineCall(ValueNode* context, ValueNode* function,
                                    ValueNode* new_target,
                                    compiler::SharedFunctionInfoRef shared,
                                    compiler::FeedbackCellRef feedback_cell,
                                    CallArguments& args, float call_frequency);
  MaybeReduceResult TryBuildInlineCall(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
#ifdef V8_ENABLE_LEAPTIERING
      JSDispatchHandle dispatch_handle,
#endif
      compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  ValueNode* BuildGenericCall(ValueNode* target, Call::TargetType target_type,
                              const CallArguments& args);
  MaybeReduceResult TryReduceCallForConstant(
      compiler::JSFunctionRef target, CallArguments& args,
      const compiler::FeedbackSource& feedback_source =
          compiler::FeedbackSource());
  MaybeReduceResult TryReduceCallForTarget(
      ValueNode* target_node, compiler::JSFunctionRef target,
      CallArguments& args, const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryReduceCallForNewClosure(
      ValueNode* target_node, ValueNode* target_context,
#ifdef V8_ENABLE_LEAPTIERING
      JSDispatchHandle dispatch_handle,
#endif
      compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryBuildCallKnownApiFunction(
      compiler::JSFunctionRef function, compiler::SharedFunctionInfoRef shared,
      CallArguments& args);
  compiler::HolderLookupResult TryInferApiHolderValue(
      compiler::FunctionTemplateInfoRef function_template_info,
      ValueNode* receiver);
  MaybeReduceResult TryReduceCallForApiFunction(
      compiler::FunctionTemplateInfoRef api_callback,
      compiler::OptionalSharedFunctionInfoRef maybe_shared,
      CallArguments& args);
  MaybeReduceResult TryReduceFunctionPrototypeApplyCallWithReceiver(
      compiler::OptionalHeapObjectRef maybe_receiver, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult ReduceCallWithArrayLikeForArgumentsObject(
      ValueNode* target_node, CallArguments& args,
      VirtualObject* arguments_object,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult ReduceCallWithArrayLike(
      ValueNode* target_node, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult ReduceCall(ValueNode* target_node, CallArguments& args,
                          const compiler::FeedbackSource& feedback_source =
                              compiler::FeedbackSource());
  ReduceResult BuildCallWithFeedback(
      ValueNode* target_node, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult BuildCallFromRegisterList(ConvertReceiverMode receiver_mode);
  ReduceResult BuildCallFromRegisters(int argc_count,
                                      ConvertReceiverMode receiver_mode);

  ValueNode* BuildElementsArray(int length);
  ReduceResult BuildAndAllocateKeyValueArray(ValueNode* key, ValueNode* value);
  ReduceResult BuildAndAllocateJSArray(
      compiler::MapRef map, ValueNode* length, ValueNode* elements,
      const compiler::SlackTrackingPrediction& slack_tracking_prediction,
      AllocationType allocation_type);
  ValueNode* BuildAndAllocateJSArrayIterator(ValueNode* array,
                                             IterationKind iteration_kind);

  MaybeReduceResult TryBuildAndAllocateJSGeneratorObject(ValueNode* closure,
                                                         ValueNode* receiver);

  ValueNode* BuildGenericConstruct(
      ValueNode* target, ValueNode* new_target, ValueNode* context,
      const CallArguments& args,
      const compiler::FeedbackSource& feedback_source =
          compiler::FeedbackSource());

  MaybeReduceResult TryReduceConstructArrayConstructor(
      compiler::JSFunctionRef array_function, CallArguments& args,
      compiler::OptionalAllocationSiteRef maybe_allocation_site = {});
  MaybeReduceResult TryReduceConstructBuiltin(
      compiler::JSFunctionRef builtin,
      compiler::SharedFunctionInfoRef shared_function_info, ValueNode* target,
      CallArguments& args);
  MaybeReduceResult TryReduceConstructGeneric(
      compiler::JSFunctionRef function,
      compiler::SharedFunctionInfoRef shared_function_info, ValueNode* target,
      ValueNode* new_target, CallArguments& args,
      compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryReduceConstruct(
      compiler::HeapObjectRef feedback_target, ValueNode* target,
      ValueNode* new_target, CallArguments& args,
      compiler::FeedbackSource& feedback_source);
  ReduceResult BuildConstruct(ValueNode* target, ValueNode* new_target,
                              CallArguments& args,
                              compiler::FeedbackSource& feedback_source);

  MaybeReduceResult TryBuildScriptContextStore(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildPropertyCellStore(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildGlobalStore(
      const compiler::GlobalAccessFeedback& global_access_feedback);

  MaybeReduceResult TryBuildScriptContextConstantLoad(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildScriptContextLoad(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildPropertyCellLoad(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildGlobalLoad(
      const compiler::GlobalAccessFeedback& global_access_feedback);

  bool TryBuildFindNonDefaultConstructorOrConstruct(
      ValueNode* this_function, ValueNode* new_target,
      std::pair<interpreter::Register, interpreter::Register> result);

  ValueNode* BuildSmiUntag(ValueNode* node);
  ValueNode* BuildGetCharCodeAt(ValueNode* string, ValueNode* index);

  ReduceResult BuildCheckSmi(ValueNode* object, bool elidable = true);
  ReduceResult BuildCheckNumber(ValueNode* object);
  ReduceResult BuildCheckHeapObject(ValueNode* object);
  ReduceResult BuildCheckJSFunction(ValueNode* object);
  ReduceResult BuildCheckJSReceiver(ValueNode* object);
  ReduceResult BuildCheckJSReceiverOrNullOrUndefined(ValueNode* object);
  ReduceResult BuildCheckSeqOneByteString(ValueNode* object);
  ReduceResult BuildCheckString(ValueNode* object);
  ReduceResult BuildCheckStringOrStringWrapper(ValueNode* object);
  ReduceResult BuildCheckStringOrOddball(ValueNode* object);
  ReduceResult BuildCheckSymbol(ValueNode* object);
  ReduceResult BuildCheckMaps(
      ValueNode* object, base::Vector<const compiler::MapRef> maps,
      std::optional<ValueNode*> map = {},
      bool has_deprecated_map_without_migration_target = false,
      bool migration_done_outside = false);
  ReduceResult BuildTransitionElementsKindOrCheckMap(
      ValueNode* heap_object, ValueNode* object_map,
      const ZoneVector<compiler::MapRef>& transition_sources,
      compiler::MapRef transition_target);
  ReduceResult BuildCompareMaps(
      ValueNode* heap_object, ValueNode* object_map,
      base::Vector<const compiler::MapRef> maps,
      MaglevSubGraphBuilder* sub_graph,
      std::optional<MaglevSubGraphBuilder::Label>& if_not_matched);
  ReduceResult BuildTransitionElementsKindAndCompareMaps(
      ValueNode* heap_object, ValueNode* object_map,
      const ZoneVector<compiler::MapRef>& transition_sources,
      compiler::MapRef transition_target, MaglevSubGraphBuilder* sub_graph,
      std::optional<MaglevSubGraphBuilder::Label>& if_not_matched);
  // Emits an unconditional deopt and returns false if the node is a constant
  // that doesn't match the ref.
  ReduceResult BuildCheckInternalizedStringValueOrByReference(
      ValueNode* node, compiler::HeapObjectRef ref, DeoptimizeReason reason);
  ReduceResult BuildCheckNumericalValueOrByReference(ValueNode* node,
                                                     compiler::ObjectRef ref,
                                                     DeoptimizeReason reason);
  ReduceResult BuildCheckValueByReference(ValueNode* node,
                                          compiler::HeapObjectRef ref,
                                          DeoptimizeReason reason);
  ReduceResult BuildCheckNumericalValue(ValueNode* node,
                                        compiler::ObjectRef ref,
                                        DeoptimizeReason reason);

  ValueNode* BuildConvertHoleToUndefined(ValueNode* node);
  ReduceResult BuildCheckNotHole(ValueNode* node);

  template <bool flip = false>
  ValueNode* BuildToBoolean(ValueNode* node);
  ValueNode* BuildLogicalNot(ValueNode* value);
  ValueNode* BuildTestUndetectable(ValueNode* value);
  ReduceResult BuildToNumberOrToNumeric(Object::Conversion mode);

  enum class TrackObjectMode { kLoad, kStore };
  bool CanTrackObjectChanges(ValueNode* object, TrackObjectMode mode);
  bool CanElideWriteBarrier(ValueNode* object, ValueNode* value);

  ValueNode* BuildLoadMap(ValueNode* object);

  void BuildInitializeStore(InlinedAllocation* alloc, ValueNode* value,
                            int offset);
  void TryBuildStoreTaggedFieldToAllocation(ValueNode* object, ValueNode* value,
                                            int offset);
  template <typename Instruction = LoadTaggedField, typename... Args>
  ValueNode* BuildLoadTaggedField(ValueNode* object, uint32_t offset,
                                  Args&&... args);

  ReduceResult BuildStoreTaggedField(ValueNode* object, ValueNode* value,
                                     int offset, StoreTaggedMode store_mode,
                                     Node** store = nullptr);
  ReduceResult BuildStoreTaggedFieldNoWriteBarrier(ValueNode* object,
                                                   ValueNode* value, int offset,
                                                   StoreTaggedMode store_mode,
                                                   Node** store = nullptr);
  ReduceResult BuildStoreTrustedPointerField(ValueNode* object,
                                             ValueNode* value, int offset,
                                             IndirectPointerTag tag,
                                             StoreTaggedMode store_mode);

  ValueNode* BuildLoadFixedArrayElement(ValueNode* elements, int index);
  ReduceResult BuildLoadFixedArrayElement(ValueNode* elements,
                                          ValueNode* index);
  ReduceResult BuildStoreFixedArrayElement(ValueNode* elements,
                                           ValueNode* index, ValueNode* value);

  ValueNode* BuildLoadFixedDoubleArrayElement(ValueNode* elements, int index);
  ReduceResult BuildLoadFixedDoubleArrayElement(ValueNode* elements,
                                                ValueNode* index);
  ReduceResult BuildStoreFixedDoubleArrayElement(ValueNode* elements,
                                                 ValueNode* index,
                                                 ValueNode* value);

  ReduceResult BuildLoadHoleyFixedDoubleArrayElement(ValueNode* elements,
                                                     ValueNode* index,
                                                     bool convert_hole);

  ReduceResult GetInt32ElementIndex(interpreter::Register reg) {
    ValueNode* index_object = current_interpreter_frame_.get(reg);
    return GetInt32ElementIndex(index_object);
  }
  ReduceResult GetInt32ElementIndex(ValueNode* index_object);

  ReduceResult GetUint32ElementIndex(interpreter::Register reg) {
    ValueNode* index_object = current_interpreter_frame_.get(reg);
    return GetUint32ElementIndex(index_object);
  }
  ReduceResult GetUint32ElementIndex(ValueNode* index_object);

  bool CanTreatHoleAsUndefined(
      base::Vector<const compiler::MapRef> const& receiver_maps);

  compiler::OptionalObjectRef TryFoldLoadDictPrototypeConstant(
      compiler::PropertyAccessInfo const& access_info);
  compiler::OptionalJSObjectRef TryGetConstantDataFieldHolder(
      compiler::PropertyAccessInfo const& access_info,
      ValueNode* lookup_start_object);
  compiler::OptionalObjectRef TryFoldLoadConstantDataField(
      compiler::JSObjectRef holder,
      compiler::PropertyAccessInfo const& access_info);
  std::optional<Float64> TryFoldLoadConstantDoubleField(
      compiler::JSObjectRef holder,
      compiler::PropertyAccessInfo const& access_info);

  // Returns the loaded value node but doesn't update the accumulator yet.
  ReduceResult BuildLoadField(compiler::PropertyAccessInfo const& access_info,
                              ValueNode* lookup_start_object,
                              compiler::NameRef name);
  MaybeReduceResult TryBuildStoreField(
      compiler::PropertyAccessInfo const& access_info, ValueNode* receiver,
      compiler::AccessMode access_mode);
  MaybeReduceResult TryBuildPropertyGetterCall(
      compiler::PropertyAccessInfo const& access_info, ValueNode* receiver,
      ValueNode* lookup_start_object);
  MaybeReduceResult TryBuildPropertySetterCall(
      compiler::PropertyAccessInfo const& access_info, ValueNode* receiver,
      ValueNode* lookup_start_object, ValueNode* value);
  MaybeReduceResult TryBuildGetKeyedPropertyWithEnumeratedKey(
      ValueNode* object, const compiler::FeedbackSource& feedback_source,
      const compiler::ProcessedFeedback& processed_feedback);
  ReduceResult BuildGetKeyedProperty(
      ValueNode* object, const compiler::FeedbackSource& feedback_source,
      const compiler::ProcessedFeedback& processed_feedback);

  ValueNode* BuildLoadFixedArrayLength(ValueNode* fixed_array);
  ReduceResult BuildLoadJSArrayLength(ValueNode* js_array,
                                      NodeType length_type = NodeType::kSmi);
  ValueNode* BuildLoadElements(ValueNode* object);

  ValueNode* BuildLoadJSFunctionFeedbackCell(ValueNode* closure);
  ValueNode* BuildLoadJSFunctionContext(ValueNode* closure);

  ReduceResult TryBuildCheckInt32Condition(ValueNode* lhs, ValueNode* rhs,
                                           AssertCondition condition,
                                           DeoptimizeReason reason);

  MaybeReduceResult TryBuildPropertyLoad(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info);
  MaybeReduceResult TryBuildPropertyStore(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info,
      compiler::AccessMode access_mode);
  MaybeReduceResult TryBuildPropertyAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info,
      compiler::AccessMode access_mode);
  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildNamedAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NamedAccessFeedback const& feedback,
      compiler::FeedbackSource const& feedback_source,
      compiler::AccessMode access_mode,
      GenericAccessFunc&& build_generic_access);

  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildLoadNamedProperty(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::FeedbackSource& feedback_source,
      GenericAccessFunc&& build_generic_access);
  MaybeReduceResult TryBuildLoadNamedProperty(
      ValueNode* receiver, compiler::NameRef name,
      compiler::FeedbackSource& feedback_source);

  ReduceResult BuildLoadTypedArrayLength(ValueNode* object,
                                         ElementsKind elements_kind);
  ReduceResult BuildLoadTypedArrayElement(ValueNode* object, ValueNode* index,
                                          ElementsKind elements_kind);
  ReduceResult BuildLoadConstantTypedArrayElement(
      compiler::JSTypedArrayRef typed_array, ValueNode* index,
      ElementsKind elements_kind);
  ReduceResult BuildStoreTypedArrayElement(ValueNode* object, ValueNode* index,
                                           ElementsKind elements_kind);
  ReduceResult BuildStoreConstantTypedArrayElement(
      compiler::JSTypedArrayRef typed_array, ValueNode* index,
      ElementsKind elements_kind);

  MaybeReduceResult TryBuildElementAccessOnString(
      ValueNode* object, ValueNode* index,
      const compiler::ElementAccessFeedback& access_info,
      compiler::KeyedAccessMode const& keyed_mode);
  MaybeReduceResult TryBuildElementAccessOnTypedArray(
      ValueNode* object, ValueNode* index,
      const compiler::ElementAccessInfo& access_info,
      compiler::KeyedAccessMode const& keyed_mode);
  MaybeReduceResult TryBuildElementLoadOnJSArrayOrJSObject(
      ValueNode* object, ValueNode* index,
      base::Vector<const compiler::MapRef> maps, ElementsKind kind,
      KeyedAccessLoadMode load_mode);
  MaybeReduceResult TryBuildElementStoreOnJSArrayOrJSObject(
      ValueNode* object, ValueNode* index_object, ValueNode* value,
      base::Vector<const compiler::MapRef> maps, ElementsKind kind,
      const compiler::KeyedAccessMode& keyed_mode);
  MaybeReduceResult TryBuildElementAccessOnJSArrayOrJSObject(
      ValueNode* object, ValueNode* index,
      const compiler::ElementAccessInfo& access_info,
      compiler::KeyedAccessMode const& keyed_mode);
  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildElementAccess(
      ValueNode* object, ValueNode* index,
      compiler::ElementAccessFeedback const& feedback,
      compiler::FeedbackSource const& feedback_source,
      GenericAccessFunc&& build_generic_access);
  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildPolymorphicElementAccess(
      ValueNode* object, ValueNode* index,
      const compiler::KeyedAccessMode& keyed_mode,
      const ZoneVector<compiler::ElementAccessInfo>& access_infos,
      GenericAccessFunc&& build_generic_access);
  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildPolymorphicPropertyAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NamedAccessFeedback const& feedback,
      compiler::AccessMode access_mode,
      const ZoneVector<compiler::PropertyAccessInfo>& access_infos,
      GenericAccessFunc&& build_generic_access);

  struct ContinuationOffsets {
    int last_continuation;
    int after_continuation;
  };
  std::optional<ContinuationOffsets>
  FindContinuationForPolymorphicPropertyLoad();
  ReduceResult BuildContinuationForPolymorphicPropertyLoad(
      const ContinuationOffsets& offsets);
  void AdvanceThroughContinuationForPolymorphicPropertyLoad(
      const ContinuationOffsets& offsets);

  // Load elimination -- when loading or storing a simple property without
  // side effects, record its value, and allow that value to be reused on
  // subsequent loads.
  void RecordKnownProperty(ValueNode* lookup_start_object,
                           KnownNodeAspects::LoadedPropertyMapKey key,
                           ValueNode* value, bool is_const,
                           compiler::AccessMode access_mode);
  MaybeReduceResult TryReuseKnownPropertyLoad(ValueNode* lookup_start_object,
                                              compiler::NameRef name);
  ReduceResult BuildLoadStringLength(ValueNode* string);

  // Converts the input node to a representation that's valid to store into an
  // array with elements kind |kind|.
  ReduceResult ConvertForStoring(ValueNode* node, ElementsKind kind);

  enum InferHasInPrototypeChainResult {
    kMayBeInPrototypeChain,
    kIsInPrototypeChain,
    kIsNotInPrototypeChain
  };
  InferHasInPrototypeChainResult InferHasInPrototypeChain(
      ValueNode* receiver, compiler::HeapObjectRef prototype);
  MaybeReduceResult TryBuildFastHasInPrototypeChain(
      ValueNode* object, compiler::HeapObjectRef prototype);
  ReduceResult BuildHasInPrototypeChain(ValueNode* object,
                                        compiler::HeapObjectRef prototype);
  MaybeReduceResult TryBuildFastOrdinaryHasInstance(
      ValueNode* object, compiler::JSObjectRef callable,
      ValueNode* callable_node);
  ReduceResult BuildOrdinaryHasInstance(ValueNode* object,
                                        compiler::JSObjectRef callable,
                                        ValueNode* callable_node);
  MaybeReduceResult TryBuildFastInstanceOf(ValueNode* object,
                                           compiler::JSObjectRef callable_ref,
                                           ValueNode* callable_node);
  MaybeReduceResult TryBuildFastInstanceOfWithFeedback(
      ValueNode* object, ValueNode* callable,
      compiler::FeedbackSource feedback_source);

  VirtualObject* GetObjectFromAllocation(InlinedAllocation* allocation);
  VirtualObject* GetModifiableObjectFromAllocation(
      InlinedAllocation* allocation);

  VirtualObject* DeepCopyVirtualObject(VirtualObject* vobj);
  VirtualObject* CreateVirtualObject(compiler::MapRef map,
                                     uint32_t slot_count_including_map);
  VirtualObject* CreateHeapNumber(Float64 value);
  VirtualObject* CreateDoubleFixedArray(uint32_t elements_length,
                                        compiler::FixedDoubleArrayRef elements);
  VirtualObject* CreateJSObject(compiler::MapRef map);
  VirtualObject* CreateConsString(ValueNode* map, ValueNode* length,
                                  ValueNode* first, ValueNode* second);
  ReduceResult CreateJSArray(compiler::MapRef map, int instance_size,
                             ValueNode* length);
  VirtualObject* CreateJSArrayIterator(compiler::MapRef map,
                                       ValueNode* iterated_object,
                                       IterationKind kind);
  VirtualObject* CreateJSConstructor(compiler::JSFunctionRef constructor);
  VirtualObject* CreateFixedArray(compiler::MapRef map, int length);
  VirtualObject* CreateContext(compiler::MapRef map, int length,
                               compiler::ScopeInfoRef scope_info,
                               ValueNode* previous_context,
                               std::optional<ValueNode*> extension = {});
  VirtualObject* CreateArgumentsObject(compiler::MapRef map, ValueNode* length,
                                       ValueNode* elements,
                                       std::optional<ValueNode*> callee = {});
  VirtualObject* CreateMappedArgumentsElements(compiler::MapRef map,
                                               int mapped_count,
                                               ValueNode* context,
                                               ValueNode* unmapped_elements);
  VirtualObject* CreateRegExpLiteralObject(
      compiler::MapRef map, compiler::RegExpBoilerplateDescriptionRef literal);
  VirtualObject* CreateJSGeneratorObject(compiler::MapRef map,
                                         int instance_size, ValueNode* context,
                                         ValueNode* closure,
                                         ValueNode* receiver,
                                         ValueNode* register_file);
  VirtualObject* CreateJSIteratorResult(compiler::MapRef map, ValueNode* value,
                                        ValueNode* done);
  VirtualObject* CreateJSStringIterator(compiler::MapRef map,
                                        ValueNode* string);
  VirtualObject* CreateJSStringWrapper(ValueNode* value);

  InlinedAllocation* ExtendOrReallocateCurrentAllocationBlock(
      AllocationType allocation_type, VirtualObject* value);

  void ClearCurrentAllocationBlock();

  void AddDeoptUse(ValueNode* node);
  void AddDeoptUse(VirtualObject* alloc);
  void AddNonEscapingUses(InlinedAllocation* allocation, int use_count);

  std::optional<VirtualObject*> TryGetNonEscapingArgumentsObject(
      ValueNode* value);

  MaybeReduceResult TryBuildFastCreateObjectOrArrayLiteral(
      const compiler::LiteralFeedback& feedback);
  std::optional<VirtualObject*> TryReadBoilerplateForFastLiteral(
      compiler::JSObjectRef boilerplate, AllocationType allocation,
      int max_depth, int* max_properties);

  InlinedAllocation* BuildInlinedAllocationForConsString(
      VirtualObject* object, AllocationType allocation);
  InlinedAllocation* BuildInlinedAllocationForHeapNumber(
      VirtualObject* object, AllocationType allocation);
  InlinedAllocation* BuildInlinedAllocationForDoubleFixedArray(
      VirtualObject* object, AllocationType allocation);
  InlinedAllocation* BuildInlinedAllocation(VirtualObject* object,
                                            AllocationType allocation);
  ValueNode* BuildInlinedArgumentsElements(int start_index, int length);
  ValueNode* BuildInlinedUnmappedArgumentsElements(int mapped_count);

  template <CreateArgumentsType type>
  VirtualObject* BuildVirtualArgumentsObject();
  template <CreateArgumentsType type>
  ValueNode* BuildAndAllocateArgumentsObject();

  bool CanAllocateSloppyArgumentElements();
  bool CanAllocateInlinedArgumentElements();

  MaybeReduceResult TryBuildInlinedAllocatedContext(
      compiler::MapRef map, compiler::ScopeInfoRef scope, int context_length);

  template <Operation kOperation>
  ReduceResult BuildGenericUnaryOperationNode();
  template <Operation kOperation>
  ReduceResult BuildGenericBinaryOperationNode();
  template <Operation kOperation>
  ReduceResult BuildGenericBinarySmiOperationNode();

  template <Operation kOperation>
  MaybeReduceResult TryReduceCompareEqualAgainstConstant();

  template <Operation kOperation>
  ReduceResult BuildInt32UnaryOperationNode();
  ReduceResult BuildTruncatingInt32BitwiseNotForToNumber(
      NodeType allowed_input_type);
  template <Operation kOperation>
  ReduceResult BuildInt32BinaryOperationNode();
  template <Operation kOperation>
  ReduceResult BuildInt32BinarySmiOperationNode();
  template <Operation kOperation>
  ReduceResult BuildTruncatingInt32BinaryOperationNodeForToNumber(
      NodeType allowed_input_type);
  template <Operation kOperation>
  ReduceResult BuildTruncatingInt32BinarySmiOperationNodeForToNumber(
      NodeType allowed_input_type);

  template <Operation kOperation>
  ReduceResult BuildFloat64UnaryOperationNodeForToNumber(
      NodeType allowed_input_type);
  template <Operation kOperation>
  ReduceResult BuildFloat64BinaryOperationNodeForToNumber(
      NodeType allowed_input_type);
  template <Operation kOperation>
  ReduceResult BuildFloat64BinarySmiOperationNodeForToNumber(
      NodeType allowed_input_type);

  template <Operation kOperation>
  ReduceResult VisitUnaryOperation();
  template <Operation kOperation>
  ReduceResult VisitBinaryOperation();
  template <Operation kOperation>
  ReduceResult VisitBinarySmiOperation();

  ReduceResult BuildUnwrapStringWrapper(ValueNode* input);
  // Given two arbitrary inputs, this builds a string-concat operation if
  // at least one operand is a String. If so, it appropriately converts the
  // other operand if needed, and then emits the most specialized concat
  // operation possible.
  MaybeReduceResult TryBuildStringConcat(ValueNode* left, ValueNode* right);
  ReduceResult BuildStringConcat(ValueNode* left, ValueNode* right);
  ReduceResult BuildNewConsStringMap(ValueNode* left, ValueNode* right);
  size_t StringLengthStaticLowerBound(ValueNode* string, int max_depth = 2);
  MaybeReduceResult TryBuildNewConsString(
      ValueNode* left, ValueNode* right,
      AllocationType allocation_type = AllocationType::kYoung);

  template <Operation kOperation>
  ReduceResult VisitCompareOperation();

  using TypeOfLiteralFlag = interpreter::TestTypeOfFlags::LiteralFlag;
  template <typename Function>
  MaybeReduceResult TryReduceTypeOf(ValueNode* value,
                                    const Function& GetResult);
  MaybeReduceResult TryReduceTypeOf(ValueNode* value);

  void BeginLoopEffects(int loop_header);
  void EndLoopEffects(int loop_header);
  void MergeIntoFrameState(BasicBlock* block, int target);
  void MergeDeadIntoFrameState(int target);
  void MergeDeadLoopIntoFrameState(int target);
  void MergeIntoInlinedReturnFrameState(BasicBlock* block);

  bool HasValidInitialMap(compiler::JSFunctionRef new_target,
                          compiler::JSFunctionRef constructor);

  ValueNode* BuildTaggedEqual(ValueNode* lhs, ValueNode* rhs);
  ValueNode* BuildTaggedEqual(ValueNode* lhs, RootIndex rhs_index);

  class BranchBuilder;

  enum class BranchType { kBranchIfTrue, kBranchIfFalse };
  enum class BranchSpecializationMode { kDefault, kAlwaysBoolean };
  enum class BranchResult {
    kDefault,
    kAlwaysTrue,
    kAlwaysFalse,
  };

  static inline BranchType NegateBranchType(BranchType jump_type) {
    switch (jump_type) {
      case BranchType::kBranchIfTrue:
        return BranchType::kBranchIfFalse;
      case BranchType::kBranchIfFalse:
        return BranchType::kBranchIfTrue;
    }
  }

  // This class encapsulates the logic of branch nodes (using the graph builder
  // or the sub graph builder).
  class BranchBuilder {
   public:
    enum Mode {
      kBytecodeJumpTarget,
      kLabelJumpTarget,
    };

    class PatchAccumulatorInBranchScope {
     public:
      PatchAccumulatorInBranchScope(BranchBuilder& builder, ValueNode* node,
                                    RootIndex root_index)
          : builder_(builder),
            node_(node),
            root_index_(root_index),
            jump_type_(builder.GetCurrentBranchType()) {
        if (builder.mode() == kBytecodeJumpTarget) {
          builder_.data_.bytecode_target.patch_accumulator_scope = this;
        }
      }

      ~PatchAccumulatorInBranchScope() {
        builder_.data_.bytecode_target.patch_accumulator_scope = nullptr;
      }

     private:
      BranchBuilder& builder_;
      ValueNode* node_;
      RootIndex root_index_;
      BranchType jump_type_;

      friend class BranchBuilder;
    };

    struct BytecodeJumpTarget {
      BytecodeJumpTarget(int jump_target_offset, int fallthrough_offset)
          : jump_target_offset(jump_target_offset),
            fallthrough_offset(fallthrough_offset),
            patch_accumulator_scope(nullptr) {}
      int jump_target_offset;
      int fallthrough_offset;
      PatchAccumulatorInBranchScope* patch_accumulator_scope;
    };

    struct LabelJumpTarget {
      explicit LabelJumpTarget(MaglevSubGraphBuilder::Label* jump_label)
          : jump_label(jump_label), fallthrough() {}
      MaglevSubGraphBuilder::Label* jump_label;
      BasicBlockRef fallthrough;
    };

    union Data {
      Data(int jump_target_offset, int fallthrough_offset)
          : bytecode_target(jump_target_offset, fallthrough_offset) {}
      explicit Data(MaglevSubGraphBuilder::Label* jump_label)
          : label_target(jump_label) {}
      BytecodeJumpTarget bytecode_target;
      LabelJumpTarget label_target;
    };

    // Creates a branch builder for bytecode offsets.
    BranchBuilder(MaglevGraphBuilder* builder, BranchType jump_type)
        : builder_(builder),
          sub_builder_(nullptr),
          jump_type_(jump_type),
          data_(builder->iterator_.GetJumpTargetOffset(),
                builder->iterator_.next_offset()) {}

    // Creates a branch builder for subgraph label.
    BranchBuilder(MaglevGraphBuilder* builder,
                  MaglevSubGraphBuilder* sub_builder, BranchType jump_type,
                  MaglevSubGraphBuilder::Label* jump_label)
        : builder_(builder),
          sub_builder_(sub_builder),
          jump_type_(jump_type),
          data_(jump_label) {}

    Mode mode() const {
      return sub_builder_ == nullptr ? kBytecodeJumpTarget : kLabelJumpTarget;
    }

    BranchType GetCurrentBranchType() const { return jump_type_; }

    void SetBranchSpecializationMode(BranchSpecializationMode mode) {
      branch_specialization_mode_ = mode;
    }
    void SwapTargets() { jump_type_ = NegateBranchType(jump_type_); }

    BasicBlockRef* jump_target();
    BasicBlockRef* fallthrough();
    BasicBlockRef* true_target();
    BasicBlockRef* false_target();

    BranchResult FromBool(bool value) const;
    BranchResult AlwaysTrue() const { return FromBool(true); }
    BranchResult AlwaysFalse() const { return FromBool(false); }

    template <typename NodeT, typename... Args>
    BranchResult Build(std::initializer_list<ValueNode*> inputs,
                       Args&&... args);

   private:
    MaglevGraphBuilder* builder_;
    MaglevGraphBuilder::MaglevSubGraphBuilder* sub_builder_;
    BranchType jump_type_;
    BranchSpecializationMode branch_specialization_mode_ =
        BranchSpecializationMode::kDefault;
    Data data_;

    void StartFallthroughBlock(BasicBlock* predecessor);
    void SetAccumulatorInBranch(BranchType jump_type) const;
  };

  BranchBuilder CreateBranchBuilder(
      BranchType jump_type = BranchType::kBranchIfTrue) {
    return BranchBuilder(this, jump_type);
  }
  BranchBuilder CreateBranchBuilder(
      MaglevSubGraphBuilder* subgraph, MaglevSubGraphBuilder::Label* jump_label,
      BranchType jump_type = BranchType::kBranchIfTrue) {
    return BranchBuilder(this, subgraph, jump_type, jump_label);
  }

  BranchResult BuildBranchIfRootConstant(BranchBuilder& builder,
                                         ValueNode* node, RootIndex root_index);
  BranchResult BuildBranchIfToBooleanTrue(BranchBuilder& builder,
                                          ValueNode* node);
  BranchResult BuildBranchIfInt32ToBooleanTrue(BranchBuilder& builder,
                                               ValueNode* node);
  BranchResult BuildBranchIfIntPtrToBooleanTrue(BranchBuilder& builder,
                                                ValueNode* node);
  BranchResult BuildBranchIfFloat64ToBooleanTrue(BranchBuilder& builder,
                                                 ValueNode* node);
  BranchResult BuildBranchIfFloat64IsHole(BranchBuilder& builder,
                                          ValueNode* node);
#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
  BranchResult BuildBranchIfFloat64IsUndefinedOrHole(BranchBuilder& builder,
                                                     ValueNode* node);
#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
  BranchResult BuildBranchIfReferenceEqual(BranchBuilder& builder,
                                           ValueNode* lhs, ValueNode* rhs);
  BranchResult BuildBranchIfInt32Compare(BranchBuilder& builder, Operation op,
                                         ValueNode* lhs, ValueNode* rhs);
  BranchResult BuildBranchIfUint32Compare(BranchBuilder& builder, Operation op,
                                          ValueNode* lhs, ValueNode* rhs);
  BranchResult BuildBranchIfUndefinedOrNull(BranchBuilder& builder,
                                            ValueNode* node);
  BranchResult BuildBranchIfUndetectable(BranchBuilder& builder,
                                         ValueNode* value);
  BranchResult BuildBranchIfJSReceiver(BranchBuilder& builder,
                                       ValueNode* value);

  BranchResult BuildBranchIfTrue(BranchBuilder& builder, ValueNode* node);
  BranchResult BuildBranchIfNull(BranchBuilder& builder, ValueNode* node);
  BranchResult BuildBranchIfUndefined(BranchBuilder& builder, ValueNode* node);
  BasicBlock* BuildBranchIfReferenceEqual(ValueNode* lhs, ValueNode* rhs,
                                          BasicBlockRef* true_target,
                                          BasicBlockRef* false_target);

  template <typename FCond, typename FTrue, typename FFalse>
  ValueNode* Select(FCond cond, FTrue if_true, FFalse if_false);

  ValueNode* BuildInt32Max(ValueNode* a, ValueNode* b);
  ValueNode* BuildInt32Min(ValueNode* a, ValueNode* b);

  ReduceResult SelectReduction(
      base::FunctionRef<BranchResult(BranchBuilder&)> cond,
      base::FunctionRef<ReduceResult()> if_true,
      base::FunctionRef<ReduceResult()> if_false);

  void MarkBranchDeadAndJumpIfNeeded(bool is_jump_taken);

  void CalculatePredecessorCounts();

  compiler::FeedbackVectorRef feedback() const {
    return compilation_unit_->feedback();
  }
  const FeedbackNexus FeedbackNexusForOperand(int slot_operand_index) const {
    return FeedbackNexus(feedback().object(),
                         GetSlotOperand(slot_operand_index),
                         broker()->feedback_nexus_config());
  }
  const FeedbackNexus FeedbackNexusForSlot(FeedbackSlot slot) const {
    return FeedbackNexus(feedback().object(), slot,
                         broker()->feedback_nexus_config());
  }
  compiler::BytecodeArrayRef bytecode() const {
    return compilation_unit_->bytecode();
  }
  const compiler::BytecodeAnalysis& bytecode_analysis() const {
    return bytecode_analysis_;
  }
  int parameter_count() const { return compilation_unit_->parameter_count(); }
  int parameter_count_without_receiver() const { return parameter_count() - 1; }
  int register_count() const { return compilation_unit_->register_count(); }

  float GetCurrentCallFrequency() {
    if (!is_inline()) return 1.0f;
    return caller_details_->call_frequency;
  }

  int argument_count() const {
    DCHECK(is_inline());
    return static_cast<int>(caller_details_->arguments.size());
  }
  int argument_count_without_receiver() const { return argument_count() - 1; }

  bool IsInsideLoop() const;

  // The fake offset used as a target for all exits of an inlined function.
  int inline_exit_offset() const {
    DCHECK(is_inline());
    return bytecode().length();
  }

  MaglevReducer<MaglevGraphBuilder> reducer_;

  LocalIsolate* const local_isolate_;
  MaglevCompilationUnit* const compilation_unit_;
  MaglevCallerDetails* caller_details_;

  // Cache the heap broker since we access it a bunch.
  compiler::JSHeapBroker* broker_ = compilation_unit_->broker();

  Graph* const graph_;
  compiler::BytecodeAnalysis bytecode_analysis_;
  interpreter::BytecodeArrayIterator iterator_;
  SourcePositionTableIterator source_position_iterator_;

  // Change the number of predecessors when encountering a dead predecessor.
  // In case we are in a peeled iteration the decrement is undone after
  // finishing the peel. This is needed since in the next iteration the
  // predecessor might not be dead.
  void DecrementDeadPredecessorAndAccountForPeeling(uint32_t offset) {
    DCHECK_LE(offset, bytecode().length());
    DCHECK_GT(predecessor_count_[offset], 0);
    DCHECK_IMPLIES(merge_states_[offset],
                   merge_states_[offset]->predecessor_count() ==
                       predecessor_count_[offset] - 1);
    predecessor_count_[offset]--;
    if (in_peeled_iteration()) {
      decremented_predecessor_offsets_.push_back(offset);
    } else {
      DCHECK(decremented_predecessor_offsets_.empty());
    }
  }
  // Set the number of predecessors initially.
  void InitializePredecessorCount(uint32_t offset, int amount) {
    DCHECK_LE(offset, bytecode().length());
    DCHECK_NULL(merge_states_[offset]);
    predecessor_count_[offset] = amount;
  }
  void UpdatePredecessorCount(uint32_t offset, int diff) {
    DCHECK_LE(offset, bytecode().length());
    DCHECK_LE(0, static_cast<int64_t>(predecessor_count_[offset]) + diff);
    DCHECK_IMPLIES(merge_states_[offset],
                   merge_states_[offset]->predecessor_count() ==
                       predecessor_count_[offset] + diff);
    uint32_t updated_pred_count = predecessor_count_[offset] + diff;
    if (updated_pred_count > NodeBase::kMaxInputs) {
      should_abort_compilation_ = true;
      return;
    }
    predecessor_count_[offset] = updated_pred_count;
  }
  uint32_t predecessor_count(uint32_t offset) {
    DCHECK_LE(offset, bytecode().length());
    DCHECK_IMPLIES(!decremented_predecessor_offsets_.empty(),
                   in_peeled_iteration());
    uint32_t actual = predecessor_count_[offset];
    DCHECK_IMPLIES(merge_states_[offset],
                   merge_states_[offset]->predecessor_count() == actual);
    return actual;
  }
  uint32_t* predecessor_count_;

  int peeled_iteration_count_ = 0;
  bool any_peeled_loop_ = false;
  bool allow_loop_peeling_;

  bool in_peeled_iteration() const {
    DCHECK_GE(peeled_iteration_count_, 0);
    return peeled_iteration_count_ > 0;
  }

  compiler::FeedbackSource current_speculation_feedback();

  // When loop SPeeling is enabled then the second-last peeling iteration
  // is the optimistic iteration. At the end we try to compile the JumpLoop and
  // only proceed with the fallback iteration 0, if the loop state is
  // incompatible with the loop end state.
  bool in_optimistic_peeling_iteration() const {
    return v8_flags.maglev_optimistic_peeled_loops &&
           peeled_iteration_count_ == 1;
  }
  bool is_loop_effect_tracking_enabled() {
    return v8_flags.maglev_escape_analysis || v8_flags.maglev_licm;
  }
  bool is_loop_effect_tracking() { return loop_effects_; }
  LoopEffects* loop_effects_ = nullptr;
  ZoneDeque<LoopEffects*> loop_effects_stack_;

  // When processing the peeled iteration of a loop, we need to reset the
  // decremented predecessor counts inside of the loop before processing the
  // body again. For this, we record offsets where we decremented the
  // predecessor count.
  ZoneVector<int> decremented_predecessor_offsets_;
  // The set of loop headers for which we decided to do loop peeling.
  BitVector loop_headers_to_peel_;

  // Current block information.
  bool in_prologue_ = true;
  // TODO(victorgomes): I think we can merge entry_stack_check_frame_ into
  // latest_checkpointed_frame_.
  InterpretedDeoptFrame* entry_stack_check_frame_ = nullptr;
  DeoptFrame* latest_checkpointed_frame_ = nullptr;
  struct ForInState {
    ValueNode* receiver = nullptr;
    ValueNode* cache_type = nullptr;
    ValueNode* enum_cache_indices = nullptr;
    ValueNode* key = nullptr;
    ValueNode* index = nullptr;
    bool receiver_needs_map_check = false;
  };
  // TODO(leszeks): Allow having a stack of these.
  ForInState current_for_in_state = ForInState();

  AllocationBlock* current_allocation_block_ = nullptr;

  BasicBlockRef* jump_targets_;
  MergePointInterpreterFrameState** merge_states_;

  InterpreterFrameState current_interpreter_frame_;
  SpeculationMode current_speculation_mode_ =
      SpeculationMode::kDisallowSpeculation;

  ValueNode* inlined_new_target_ = nullptr;

  bool is_turbolev_ = false;
  bool should_abort_compilation_ = false;

  // Bytecode offset at which compilation should start.
  int entrypoint_;
  int bailout_for_entrypoint() {
    if (!graph_->is_osr()) return kFunctionEntryBytecodeOffset;
    return bytecode_analysis_.osr_bailout_id().ToInt();
  }

  int inlining_id_ = SourcePosition::kNotInlined;
  int next_handler_table_index_ = 0;

  EagerDeoptFrameScope* current_eager_deopt_scope_ = nullptr;
  LazyDeoptFrameScope* current_lazy_deopt_scope_ = nullptr;
  LazyDeoptResultLocationScope* lazy_deopt_result_location_scope_ = nullptr;

  struct HandlerTableEntry {
    int end;
    int handler;
  };
  ZoneStack<HandlerTableEntry> catch_block_stack_;

#ifdef DEBUG
  bool IsNodeCreatedForThisBytecode(ValueNode* node) const;
#endif

  bool CanSpeculateCall() const {
    return current_speculation_mode_ == SpeculationMode::kAllowSpeculation;
  }

  bool CanSpeculateCall(
      std::initializer_list<SpeculationMode> supported_modes) const {
    return CanSpeculateCall() ||
           std::find(supported_modes.begin(), supported_modes.end(),
                     current_speculation_mode_) != supported_modes.end();
  }

  inline void MarkNodeDead(Node* node) {
    for (int i = 0; i < node->input_count(); ++i) {
      node->input(i).clear();
    }
    node->OverwriteWith(Opcode::kDead);
  }

  ZoneUnorderedMap<KnownNodeAspects::LoadedContextSlotsKey, Node*>
      unobserved_context_slot_stores_;

  // When set, inline only small functions.
  bool only_inline_small_ = false;
};

template <bool is_possible_map_change>
void MaglevGraphBuilder::ResetBuilderCachedState() {
  latest_checkpointed_frame_ = nullptr;

  // If a map might have changed, then we need to re-check it for for-in.
  // TODO(leszeks): Track this on merge states / known node aspects, rather
  // than on the graph, so that it can survive control flow.
  if constexpr (is_possible_map_change) {
    current_for_in_state.receiver_needs_map_check = true;
  }
}

template <typename NodeT>
void MaglevGraphBuilder::MarkPossibleSideEffect(NodeT* node) {
  if constexpr (NodeT::kProperties.can_read() ||
                NodeT::kProperties.can_deopt() ||
                NodeT::kProperties.can_throw()) {
    unobserved_context_slot_stores_.clear();
  }

  if constexpr (Node::opcode_of<NodeT> != Opcode::kAllocationBlock &&
                (NodeT::kProperties.can_deopt() ||
                 NodeT::kProperties.can_throw() ||
                 NodeT::kProperties.can_allocate())) {
    ClearCurrentAllocationBlock();
  }

  // Don't do anything for nodes without side effects.
  if constexpr (!NodeT::kProperties.can_write()) return;

  // We only need to clear unstable node aspects on the current builder, not
  // the parent, since we'll anyway copy the known_node_aspects to the parent
  // once we finish the inlined function.

  if constexpr (IsElementsArrayWrite(Node::opcode_of<NodeT>)) {
    node->ClearElementsProperties(is_tracing_enabled(), known_node_aspects());
    if (is_loop_effect_tracking()) {
      loop_effects_->keys_cleared.insert(
          KnownNodeAspects::LoadedPropertyMapKey::Elements());
    }
  } else if constexpr (!IsSimpleFieldStore(Node::opcode_of<NodeT>) &&
                       !IsTypedArrayStore(Node::opcode_of<NodeT>)) {
    // Don't change known node aspects for simple field stores. The only
    // relevant side effect on these is writes to objects which invalidate
    // loaded properties and context slots, and we invalidate these already as
    // part of emitting the store.
    node->ClearUnstableNodeAspects(is_tracing_enabled(), known_node_aspects());
    if (is_loop_effect_tracking()) {
      loop_effects_->unstable_aspects_cleared = true;
    }
  }

  // Simple field stores can't possibly change or migrate the map.
  static constexpr bool is_possible_map_change =
      !IsSimpleFieldStore(Node::opcode_of<NodeT>);

  // All user-observable side effects need to clear state that is cached on
  // the builder. This reset has to be propagated up through the parents.
  // TODO(leszeks): What side effects aren't observable? Maybe migrations?
  ResetBuilderCachedState<is_possible_map_change>();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
