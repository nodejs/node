// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_

#include <optional>
#include <type_traits>
#include <utility>

#include "src/base/base-export.h"
#include "src/base/enum-set.h"
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
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-node-type.h"
#include "src/maglev/maglev-reducer.h"
#include "src/maglev/maglev-tracer.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/elements-kind.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {
namespace maglev {

class CallArguments;
class MaglevGraphBuilder;

template <typename ReducerT>
class MapInference;

struct CatchBlockDetails {
  BasicBlockRef* ref = nullptr;
  MaglevGraphBuilder* handler_builder = nullptr;
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
  int loop_depth;
  int peeled_iteration_count;
  bool is_eager_inline;
  bool is_small_function;
  float call_frequency;
  InliningTreeDebugInfo* parent_inlining_tree_debug_info;
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
  template <typename T>
  friend class MapInference;

  using MapInference = maglev::MapInference<MaglevGraphBuilder>;
  using CallArguments = ::v8::internal::maglev::CallArguments;

  using EagerDeoptFrameScope =
      MaglevReducer<MaglevGraphBuilder>::EagerDeoptFrameScope;

  using LazyDeoptFrameScope =
      MaglevReducer<MaglevGraphBuilder>::LazyDeoptFrameScope;

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

  V8_EXPORT_PRIVATE explicit MaglevGraphBuilder(
      LocalIsolate* local_isolate, MaglevCompilationUnit* compilation_unit,
      Graph* graph, MaglevCallerDetails* caller_details = nullptr);

  V8_EXPORT_PRIVATE bool Build();

  ReduceResult BuildInlineFunction(SourcePosition call_site_position,
                                   ValueNode* context, ValueNode* function,
                                   ValueNode* new_target);

  void StartPrologue();
  void SetArgument(int i, ValueNode* value);
  ValueNode* GetArgument(int i);
  ValueNode* GetInlinedArgument(int i);
  void BuildRegisterFrameInitialization(ValueNode* context = nullptr,
                                        ValueNode* closure = nullptr,
                                        ValueNode* new_target = nullptr);
  void BuildMergeStates();
  BasicBlock* EndPrologue();
  void PeelLoop();
  void BuildLoopForPeeling();
  void BuildLoopHeader(int offset);

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
  Uint32Constant* GetUint32Constant(uint32_t constant) {
    return graph()->GetUint32Constant(constant);
  }
  Float64Constant* GetFloat64Constant(double constant) {
    return graph()->GetFloat64Constant(constant);
  }
  Float64Constant* GetFloat64Constant(Float64 constant) {
    return graph()->GetFloat64Constant(constant);
  }
  HoleyFloat64Constant* GetHoleyFloat64Constant(Float64 constant) {
    return graph()->GetHoleyFloat64Constant(constant);
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

  compiler::ScopeInfoRef GetScopeInfo(interpreter::Register reg) const {
    auto opt_scope_info = register_scope_infos_[reg];
    DCHECK(opt_scope_info.has_value());
    return opt_scope_info.value();
  }

  compiler::ScopeInfoRef GetCurrentScopeInfo() const {
    return GetScopeInfo(interpreter::Register::current_context());
  }

  void SetCurrentScopeInfo(compiler::OptionalScopeInfoRef scope_info);

  MaglevCompilationUnit* compilation_unit() const { return compilation_unit_; }
  const InterpreterFrameState& current_interpreter_frame() const {
    return current_interpreter_frame_;
  }
  MaglevCallerDetails* caller_details() const { return caller_details_; }
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

  DeoptFrame* GetLatestCheckpointedFrame();
  DeoptFrame* GetDeoptFrameForEagerDeopt() {
    return GetLatestCheckpointedFrame();
  }
  std::tuple<DeoptFrame*, interpreter::Register, int> GetDeoptFrameForLazyDeopt(
      bool can_throw);

  void OnBeginDeoptFrameScope() {
    current_interpreter_frame_.virtual_objects().Snapshot();
  }
  void OnEndDeoptFrameScope() {
    // We might have cached a checkpointed frame which includes this scope;
    // reset it just in case.
    latest_checkpointed_frame_ = nullptr;
  }

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

  bool is_tracing() const {
    return v8_flags.trace_maglev_graph_building &&
           compilation_unit_->info()->is_tracing_enabled();
  }

  maglev::Tracer tracer() const {
    return maglev::Tracer(compilation_unit_->info());
  }

  KnownNodeAspects& known_node_aspects() {
    return *current_interpreter_frame_.known_node_aspects();
  }

  LoopEffects* loop_effects() { return loop_effects_; }

  DeoptFrame* AddInlinedArgumentsToDeoptFrame(DeoptFrame* deopt_frame,
                                              const MaglevCompilationUnit* unit,
                                              ValueNode* closure,
                                              base::Vector<ValueNode*> args);

  std::pair<interpreter::Register, int> GetResultLocationAndSize() const;

  void AttachExceptionHandlerInfo(NodeBase* node);

  template <typename NodeT>
  void MarkPossibleSideEffect(NodeT* node);

  // CRTP hook for MaglevReducer::EmitAbruptBlockEnd: registers the
  // just-finished block in the graph and snapshots the interpreter frame.
  void OnAbruptBlockEnd(BasicBlock* block);

  template <bool is_possible_map_change = true>
  void ResetBuilderCachedState();

  static SpeculationMode GetSpeculationMode(
      compiler::JSHeapBroker* broker, compiler::FeedbackSource feedback_source);

 private:
  friend class Subgraph<MaglevGraphBuilder>;

  void InitializeScopeInfo();

  // Helper class for building a subgraph with its own control flow, that is not
  // attached to any bytecode.
  //
  // It does this by creating a fake dummy compilation unit and frame state, and
  // wrapping up all the places where it pretends to be interpreted but isn't.
  class MaglevSubGraphBuilder
      : public SubgraphBase<MaglevSubGraphBuilder, MaglevGraphBuilder> {
   public:
    using Base = SubgraphBase<MaglevSubGraphBuilder, MaglevGraphBuilder>;
    using Variable = Base::Variable;
    using Label = Base::Label;
    using LabelForTrackingInterpreterFrameState =
        Base::LabelForTrackingInterpreterFrameState;
    class LoopLabel;

    MaglevSubGraphBuilder(MaglevGraphBuilder* builder, int variable_count);
    LoopLabel BeginLoop(std::initializer_list<Variable*> loop_vars);
    template <typename ControlNodeT, typename... Args>
    ReduceResult GotoIfTrue(Label* true_target,
                            std::initializer_list<ValueNode*> control_inputs,
                            Args&&... args);
    template <typename ControlNodeT, typename... Args>
    ReduceResult GotoIfFalse(Label* false_target,
                             std::initializer_list<ValueNode*> control_inputs,
                             Args&&... args);
    void Goto(Label* label);
    void EndLoop(LoopLabel* loop_label);
    void Bind(Label* label);

    void MergeIntoLabel(Label* label, BasicBlock* predecessor);
    void MergeDeadInterpreterFrameState(Label* label, unsigned num);

   protected:
    class BorrowParentKnownNodeAspectsAndVOs;
    void TakeKnownNodeAspectsAndVOsFromParent();
    void MoveKnownNodeAspectsAndVOsToParent();

    MaglevCompilationUnit* compilation_unit() const {
      return builder_->compilation_unit();
    }

    MaglevGraphBuilder* builder_;
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

  NodeType GetType(ValueNode* node) { return reducer_.GetType(node); }
  NodeInfo* GetOrCreateInfoFor(ValueNode* node) {
    return known_node_aspects().GetOrCreateInfoFor(broker(), node);
  }

  // Returns true if we statically know that {lhs} and {rhs} have disjoint
  // types.
  bool HaveDisjointTypes(ValueNode* lhs, ValueNode* rhs) {
    return reducer_.HaveDisjointTypes(lhs, rhs);
  }
  bool HasDisjointType(ValueNode* lhs, NodeType rhs_type) {
    return reducer_.HasDisjointType(lhs, rhs_type);
  }

 public:
  enum class OsrFromMaglevStrategy {
    kOnOsrCompile,
    kIfLoopOsrd,
    kAlways,
  };
  using OsrFromMaglevStrategies = base::EnumSet<OsrFromMaglevStrategy>;

  bool ShouldEmitInterruptBudgetChecks();
  bool ShouldEmitOsrInterruptBudgetChecks(FeedbackSlot feedback_slot,
                                          BytecodeOffset osr_offset);

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

  // Return true if the given offset is a loop header. Their merge state is
  // only created once all the forward edges have been merged, so this can be
  // true for offsets the graph builder hasn't reached yet.
  bool IsLoopHeader(int offset) const { return loop_headers_.Contains(offset); }

  // Return true if the given offset is a merge point, i.e. there are jumps
  // targetting it.
  bool IsOffsetAMergePoint(int offset) const {
    return merge_states_[offset] != nullptr;
  }

  ValueNode* GetContextAtDepth(ValueNode* context, size_t depth,
                               compiler::ScopeInfoRef* out_scope_info);
  bool CheckContextExtensions(size_t depth);

  void KillPeeledLoopTargets(int peelings);

  void MarkBytecodeDead();

  void UpdateSourceAndBytecodePosition(int offset);

  ReduceResult VisitSingleBytecode();

  void HandleTryBlock(int offset, bool set_context_scope_info = true);

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
  ReduceResult AddNewNode(size_t input_count,
                          Function&& post_create_input_initializer,
                          Args&&... args);
  // Add a new node with a static set of inputs.
  template <typename NodeT, typename... Args>
  ReduceResult AddNewNode(std::initializer_list<ValueNode*> inputs,
                          Args&&... args);

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

  MaybeReduceResult TrySpecializeLoadContextSlotToFunctionContext(
      ValueNode* context, int index, VariableMode mode,
      MaybeAssignedFlag assigned);
  ValueNode* TrySpecializeLoadContextCell(ValueNode* context, int index,
                                          MaybeAssignedFlag assigned);
  ReduceResult LoadAndCacheContextSlot(ValueNode* context, int index,
                                       ContextMode context_mode,
                                       compiler::ScopeInfoRef scope_info);
  MaybeReduceResult TrySpecializeStoreContextCell(ValueNode* context, int index,
                                                  ValueNode* value,
                                                  MaybeAssignedFlag assigned);
  ReduceResult StoreAndCacheContextSlot(ValueNode* context, int index,
                                        ValueNode* value,
                                        ContextMode context_mode,
                                        compiler::ScopeInfoRef scope_info);
  ValueNode* TryGetParentContext(ValueNode* node);
  void MinimizeContextChainDepth(ValueNode** context, size_t* depth,
                                 compiler::ScopeInfoRef* scope_info);
  void EscapeContext();
  ReduceResult BuildLoadContextSlot(ValueNode* context, size_t depth,
                                    int slot_index, ContextMode context_mode,
                                    compiler::ScopeInfoRef scope_info);
  ReduceResult BuildStoreContextSlot(ValueNode* context, size_t depth,
                                     int slot_index, ValueNode* value,
                                     ContextMode context_mode,
                                     compiler::ScopeInfoRef scope_info);
  ReduceResult BuildExtendPropertiesBackingStore(compiler::MapRef map,
                                                 ValueNode* receiver,
                                                 ValueNode* property_array);

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(std::initializer_list<ValueNode*> inputs);

  template <Builtin kBuiltin>
  ReduceResult BuildCallBuiltinWithTaggedInputs(
      std::initializer_list<ValueNode*> inputs);

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(
      std::initializer_list<ValueNode*> inputs,
      compiler::FeedbackSource const& feedback,
      CallBuiltin::FeedbackSlotType slot_type = CallBuiltin::kTaggedIndex);

  template <Builtin kBuiltin>
  ReduceResult BuildCallBuiltinWithTaggedInputs(
      std::initializer_list<ValueNode*> inputs,
      compiler::FeedbackSource const& feedback,
      CallBuiltin::FeedbackSlotType slot_type = CallBuiltin::kTaggedIndex);

  ReduceResult BuildLoadGlobal(compiler::NameRef name,
                               compiler::FeedbackSource& feedback_source,
                               TypeofMode typeof_mode);

  ReduceResult BuildToString(ValueNode* value, ToString::ConversionMode mode);

  constexpr bool RuntimeFunctionWillThrow(Runtime::FunctionId function_id) {
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
        broker(),
        broker()->CanonicalPersistentHandle(Cast<T>(
            iterator_.GetConstantForOperand(operand_index, local_isolate()))));
  }

  ValueNode* GetRegisterInput(Register reg);

  // Move an existing ValueNode between two registers. You can pass
  // virtual_accumulator as the src or dst to move in or out of the accumulator.
  void MoveNodeBetweenRegisters(interpreter::Register src,
                                interpreter::Register dst) {
    // We shouldn't be moving newly created nodes between registers.
    DCHECK(!IsNodeCreatedForThisBytecode(current_interpreter_frame_.get(src)));
    DCHECK_NOT_NULL(current_interpreter_frame_.get(src));

    current_interpreter_frame_.set(dst, current_interpreter_frame_.get(src));

    compiler::OptionalScopeInfoRef src_scope_info;
    if (src == interpreter::Register::virtual_accumulator()) {
      src_scope_info = accumulator_scope_info_;
    } else {
      src_scope_info = register_scope_infos_[src];
    }

    if (dst == interpreter::Register::virtual_accumulator()) {
      accumulator_scope_info_ = src_scope_info;
    } else {
      register_scope_infos_[dst] = src_scope_info;
    }
  }

  ReduceResult GetTaggedValue(ValueNode* value,
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

  ReduceResult GetTaggedValue(interpreter::Register reg,
                              UseReprHintRecording record_use_repr_hint =
                                  UseReprHintRecording::kRecord);

  ReduceResult GetInternalizedString(interpreter::Register reg);

  // Get an Int32 representation node whose value is equivalent to the ToInt32
  // truncation of the given node (including a ToNumber call). Only trivial
  // ToNumber is allowed -- values that are already numeric, and optionally
  // oddballs.
  //
  // Deopts if the ToNumber is non-trivial.
  ReduceResult GetTruncatedInt32ForToNumber(ValueNode* value,
                                            NodeType assumed_input_type);

  ReduceResult GetTruncatedInt32ForToNumber(interpreter::Register reg,
                                            NodeType assumed_input_type) {
    return GetTruncatedInt32ForToNumber(current_interpreter_frame_.get(reg),
                                        assumed_input_type);
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

  template <class T>
  compiler::OptionalRef<typename compiler::ref_traits<T>::ref_type>
  TryGetConstant(ValueNode* node, ValueNode** constant_node = nullptr) {
    return reducer_.TryGetConstant<T>(node, constant_node);
  }

  enum CheckType GetCheckType(NodeType type) {
    return reducer_.GetCheckType(type);
  }

  ReduceResult BuildAbort(AbortReason reason) {
    return reducer_.BuildAbort(reason);
  }

  std::optional<int32_t> TryGetInt32Constant(ValueNode* value);
  std::optional<uint32_t> TryGetUint32Constant(ValueNode* value);
  std::optional<Float64> TryGetFloat64Constant(ValueNode* value,
                                               NodeType assumed_input_type);
  MaybeHandle<String> TryGetStringConstant(ValueNode* value);

  // Get an Int32 representation node whose value is equivalent to the given
  // node.
  ReduceResult GetInt32(ValueNode* value, bool can_be_heap_number = false);
  ReduceResult GetInt32(ReduceResult value_result,
                        bool can_be_heap_number = false);

  ReduceResult EnsureInt32(ValueNode* value, bool can_be_heap_number = false);
  ReduceResult EnsureInt32(interpreter::Register reg);

  // Get a Float64 representation node whose value is equivalent to the given
  // node.
  //
  // Deopts if the value is not exactly representable as a Float64.
  ReduceResult GetFloat64(ValueNode* value);
  ReduceResult GetFloat64(interpreter::Register reg);

  ReduceResult GetHoleyFloat64(ValueNode* value);

  // Get a Float64 representation node whose value is the result of ToNumber on
  // the given node. Only trivial ToNumber is allowed -- values that are already
  // numeric, and optionally oddballs.
  //
  // Deopts if the ToNumber value is not exactly representable as a Float64, or
  // the ToNumber is non-trivial.
  ReduceResult GetFloat64ForToNumber(ValueNode* value,
                                     NodeType assumed_input_type);
  ReduceResult GetFloat64ForToNumber(interpreter::Register reg,
                                     NodeType assumed_input_type);

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

  ReduceResult GetAccumulatorTruncatedInt32ForToNumber(
      NodeType assumed_input_type) {
    return GetTruncatedInt32ForToNumber(
        interpreter::Register::virtual_accumulator(), assumed_input_type);
  }

  ValueNode* GetAccumulatorUint8ClampedForToNumber() {
    return GetUint8ClampedForToNumber(
        interpreter::Register::virtual_accumulator());
  }

  ReduceResult GetAccumulatorFloat64ForToNumber(NodeType assumed_input_type) {
    return GetFloat64ForToNumber(interpreter::Register::virtual_accumulator(),
                                 assumed_input_type);
  }

  // An operand that is already HoleyFloat64 can keep its undefined and let the
  // arithmetic turn it into a NaN, instead of deopting on it.
  NodeType AllowUndefinedInputForArithmetic(interpreter::Register reg,
                                            NodeType assumed_input_type) {
    if (NodeTypeIs(assumed_input_type, NodeType::kNumber) &&
        current_interpreter_frame_.get(reg)->value_representation() ==
            ValueRepresentation::kHoleyFloat64) {
      return NodeType::kNumberOrUndefined;
    }
    return assumed_input_type;
  }

  ReduceResult GetAccumulatorFloat64ForArithmetic(NodeType assumed_input_type) {
    interpreter::Register reg = interpreter::Register::virtual_accumulator();
    return GetFloat64ForToNumber(
        reg, AllowUndefinedInputForArithmetic(reg, assumed_input_type));
  }

  bool IsRegisterEqualToAccumulator(int operand_index) {
    interpreter::Register source = iterator_.GetRegisterOperand(operand_index);
    return current_interpreter_frame_.get(source) ==
           current_interpreter_frame_.accumulator();
  }

  ValueNode* LoadRegister(int operand_index) {
    return current_interpreter_frame_.get(
        iterator_.GetRegisterOperand(operand_index));
  }

  ReduceResult LoadRegisterFloat64ForToNumber(int operand_index,
                                              NodeType assumed_input_type) {
    return GetFloat64ForToNumber(iterator_.GetRegisterOperand(operand_index),
                                 assumed_input_type);
  }

  ReduceResult LoadRegisterFloat64ForArithmetic(int operand_index,
                                                NodeType assumed_input_type) {
    interpreter::Register reg = iterator_.GetRegisterOperand(operand_index);
    return GetFloat64ForToNumber(
        reg, AllowUndefinedInputForArithmetic(reg, assumed_input_type));
  }

  template <typename NodeT>
  void SetAccumulator(NodeT* node) {
    // Accumulator stores are equivalent to stores to the virtual accumulator
    // register.
    StoreRegister(interpreter::Register::virtual_accumulator(), node);
  }

  ReduceResult SetAccumulator(ReduceResult result) {
    ValueNode* node;
    GET_VALUE_OR_ABORT(node, result);
    // Accumulator stores are equivalent to stores to the virtual accumulator
    // register.
    StoreRegister(interpreter::Register::virtual_accumulator(), node);
    return ReduceResult::Done();
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
    // Don't use StoreRegister for the second returned value of a call --
    // instead, use StoreRegisterPair.
    DCHECK_IMPLIES(value->properties().can_lazy_deopt(),
                   value->opcode() != Opcode::kGetSecondReturnedValue);
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
      LazyDeoptFrameScope* scope, bool mark_accumulator_dead, bool can_throw);
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
  std::optional<BasicBlock*> FinishBlock(
      std::initializer_list<ValueNode*> control_inputs, Args&&... args);

  // This version can be used when the control node doesn't have inputs, or the
  // input conversion will always succeed, and thus we cannot bail out.
  template <typename ControlNodeT, typename... Args>
  BasicBlock* FinishBlockNoAbort(
      std::initializer_list<ValueNode*> control_inputs, Args&&... args);

  ValueNode* GetValueOrUndefined(ValueNode* maybe_value) {
    if (maybe_value == nullptr) {
      return GetRootConstant(RootIndex::kUndefinedValue);
    }
    return maybe_value;
  }

  ReduceResult GetConvertReceiver(compiler::SharedFunctionInfoRef shared,
                                  const CallArguments& args);
  std::pair<ReduceResult, base::Vector<ValueNode*>>
  GetArgumentsAsArrayOfValueNodes(compiler::SharedFunctionInfoRef shared,
                                  const CallArguments& args);

// Builtins that are only reduced in the MaglevGraphBuilder (i.e. eagerly, at
// graph-building time). Builtins reduced in the MaglevReducer (and thus also
// reduced post-inlining by the optimizer) live in MAGLEV_REDUCER_BUILTIN.
#define MAGLEV_GRAPH_BUILDER_REDUCED_BUILTINS(V) \
  V(ArrayConstructor)                            \
  V(ArrayForEach)                                \
  V(ArrayIteratorPrototypeNext)                  \
  V(GeneratorPrototypeNext)                      \
  V(ArrayMap)                                    \
  V(ArrayPrototypeSlice)                         \
  V(ArrayPrototypePush)                          \
  V(ArrayPrototypePop)                           \
  V(ArrayPrototypeSort)                          \
  V(BooleanConstructor)                          \
  V(DataViewPrototypeGetByteLength)              \
  V(FunctionPrototypeApply)                      \
  V(FunctionPrototypeCall)                       \
  V(MapIteratorPrototypeNext)                    \
  V(MapPrototypeGet)                             \
  V(SetIteratorPrototypeNext)                    \
  V(WeakMapPrototypeGet)                         \
  V(ObjectPrototypeGetProto)                     \
  V(ObjectGetPrototypeOf)                        \
  V(ReflectGet)                                  \
  V(ReflectGetPrototypeOf)                       \
  V(ReflectHas)                                  \
  V(ReflectApply)                                \
  V(ObjectPrototypeHasOwnProperty)               \
  V(NumberParseInt)                              \
  V(SetPrototypeHas)                             \
  V(StringConstructor)                           \
  V(StringPrototypeStartsWith)

#define DEFINE_BUILTIN_REDUCER(Name, ...)                           \
  MaybeReduceResult TryReduce##Name(compiler::JSFunctionRef target, \
                                    CallArguments& args);
  MAGLEV_GRAPH_BUILDER_REDUCED_BUILTINS(DEFINE_BUILTIN_REDUCER)
#undef DEFINE_BUILTIN_REDUCER

  static constexpr bool IsReducibleBuiltin(Builtin builtin) {
    switch (builtin) {
#define CASE(Name, ...) case Builtin::k##Name:
      MAGLEV_REDUCER_BUILTIN(CASE)
      MAGLEV_GRAPH_BUILDER_REDUCED_BUILTINS(CASE)
#undef CASE
      return true;
      default:
        return false;
    }
  }

  using InitialCallback = base::FunctionRef<ReduceResult(ValueNode*)>;
  using ProcessElementCallback =
      base::FunctionRef<ReduceResult(ValueNode*, ValueNode*)>;
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

  MaybeReduceResult TryReduceGetProto(ValueNode* node);

  template <typename MapKindsT, typename IndexToElementsKindFunc,
            typename BuildKindSpecificFunc>
  MaybeReduceResult BuildJSArrayBuiltinMapSwitchOnElementsKind(
      ValueNode* receiver, const MapKindsT& map_kinds,
      MaglevSubGraphBuilder& sub_graph,
      std::optional<MaglevSubGraphBuilder::Label>& do_return,
      int unique_kind_count, IndexToElementsKindFunc&& index_to_elements_kind,
      BuildKindSpecificFunc&& build_kind_specific,
      bool make_smi_fallthrough_to_object = false);

  template <typename CallNode, typename... Args>
  ReduceResult AddNewCallNode(const CallArguments& args, Args&&... extra_args);

  MaybeReduceResult TryReduceGetIterator(ValueNode* receiver, int load_slot,
                                         int call_slot);

  MaybeReduceResult TryReduceForOfNext(ValueNode* iterator,
                                       ValueNode* next_method, int call_slot);

  MaybeReduceResult TryReduceArrayIteratorForOfNext(
      ValueNode* iterator, ValueNode* next_method,
      compiler::FeedbackSource& feedback_source,
      compiler::FeedbackSource& done_feedback,
      compiler::FeedbackSource& value_feedback,
      compiler::FeedbackSource& iterated_object_feedback);

  ReduceResult BuildForOfNextFallback(ValueNode* iterator,
                                      ValueNode* next_method,
                                      compiler::FeedbackSource& feedback_source,
                                      compiler::FeedbackSource& done_feedback,
                                      compiler::FeedbackSource& value_feedback);

  MaybeReduceResult BuildCallSelf(ValueNode* context, ValueNode* function,
                                  ValueNode* new_target,
                                  compiler::SharedFunctionInfoRef shared,
                                  CallArguments& args);
  MaybeReduceResult TryReduceBuiltin(
      compiler::JSFunctionRef target, compiler::SharedFunctionInfoRef shared,
      CallArguments& args, const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryReduceAsyncFunctionEnter(ValueNode* closure,
                                                ValueNode* reducer);
  MaybeReduceResult TryReduceAsyncFunctionReject(
      ValueNode* async_function_object, ValueNode* reason);
  MaybeReduceResult TryReduceAsyncFunctionResolve(
      ValueNode* async_function_object, ValueNode* value);
  bool TargetIsCurrentCompilingUnit(compiler::JSFunctionRef target);
  bool IsTheHoleConstant(ValueNode* node);
  ReduceResult BuildCallKnownJSFunction(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
      JSDispatchHandle dispatch_handle, compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult BuildCallKnownJSFunction(ValueNode* context, ValueNode* function,
                                        ValueNode* new_target,
                                        JSDispatchHandle dispatch_handle,
                                        compiler::SharedFunctionInfoRef shared,
                                        base::Vector<ValueNode*> arguments);
  MaybeReduceResult TryBuildCallKnownJSFunction(
      compiler::JSFunctionRef function, ValueNode* new_target,
      CallArguments& args, const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryBuildCallKnownJSFunction(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
      JSDispatchHandle dispatch_handle, compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);

  bool IsFunctionCandidateForEagerInlining(
      compiler::SharedFunctionInfoRef shared, CallArguments& args);
  bool ShouldEagerInlineCall(compiler::SharedFunctionInfoRef shared,
                             CallArguments& args);
  ReduceResult BuildEagerInlineCall(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
      compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell,
      const base::Vector<ValueNode*> arguments_vector, float call_frequency);
  MaybeReduceResult TryBuildInlineCall(
      ValueNode* context, ValueNode* function, ValueNode* new_target,
      JSDispatchHandle dispatch_handle,
      compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult BuildGenericCall(
      ValueNode* target, Call::TargetType target_type,
      const CallArguments& args,
      const compiler::FeedbackSource& feedback_source = {});
  MaybeReduceResult TryReduceCallForConstant(
      compiler::JSFunctionRef target, CallArguments& args,
      const compiler::FeedbackSource& feedback_source =
          compiler::FeedbackSource());
  MaybeReduceResult TryReduceCallForTarget(
      ValueNode* target_node, compiler::JSFunctionRef target,
      CallArguments& args, const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryReduceCallForNewClosure(
      ValueNode* target_node, ValueNode* target_context,
      JSDispatchHandle dispatch_handle,
      compiler::SharedFunctionInfoRef shared,
      compiler::FeedbackCellRef feedback_cell, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryBuildCallKnownApiFunction(
      compiler::JSFunctionRef function, compiler::SharedFunctionInfoRef shared,
      CallArguments& args);
  // Returns either a result or nullopt. The latter case must be treated like
  // IsDoneWithAbort.
  std::optional<compiler::HolderLookupResult> TryInferApiHolderValue(
      compiler::FunctionTemplateInfoRef function_template_info,
      ValueNode* receiver);
  MaybeReduceResult TryReduceCallForApiFunction(
      compiler::FunctionTemplateInfoRef api_callback,
      compiler::OptionalSharedFunctionInfoRef maybe_shared,
      CallArguments& args);
  MaybeReduceResult TryReduceFunctionPrototypeApplyCallWithReceiver(
      compiler::OptionalHeapObjectRef maybe_receiver, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);

  template <typename CallNode, typename... Args>
  ReduceResult BuildCallForwardArgumentsElements(ValueNode* target_node,
                                                 CallArguments& args,
                                                 ArgumentsElements* elements,
                                                 Args&&... extra_arg);
  std::optional<base::SmallVector<ValueNode*, 8>>
  TryExtractArgumentsFromElements(VirtualObject* arguments_object,
                                  const CallArguments& args,
                                  size_t num_args_to_copy);
  ReduceResult ReduceCallWithArrayLikeForArgumentsObject(
      ValueNode* target_node, CallArguments& args,
      VirtualObject* arguments_object,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult ReduceCallWithArrayLike(
      ValueNode* target_node, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryReduceCallWithSpreadForArgumentsObject(
      ValueNode* target_node, CallArguments& args,
      VirtualObject* arguments_object,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult ReduceCallWithSpread(
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

  ValueNode* BuildElementsArray(ElementsKind elements_kind, int length);
  ValueNode* BuildElementsArray(ElementsKind elements_kind,
                                base::Vector<ValueNode*> values);
  ReduceResult BuildAndAllocateKeyValueArray(ValueNode* key, ValueNode* value);

  MaybeReduceResult TryReduceCollectionIteratorPrototypeNext(
      compiler::JSFunctionRef target, CallArguments& args,
      CollectionKind collection_kind, int entry_size,
      RootIndex empty_collection_root);

  using BuildIteratorStepResultCallback =
      base::FunctionRef<ReduceResult(ValueNode* value, ValueNode* is_done)>;
  MaybeReduceResult BuildCollectionIteratorStep(
      ValueNode* receiver, CollectionKind collection_kind, int entry_size,
      RootIndex empty_collection_root,
      BuildIteratorStepResultCallback build_result);
  ReduceResult BuildAndAllocateJSArray(
      compiler::MapRef map, ValueNode* length, ValueNode* elements,
      const compiler::SlackTrackingPrediction& slack_tracking_prediction,
      AllocationType allocation_type);

  MaybeReduceResult TryBuildAndAllocateJSGeneratorObject(ValueNode* closure,
                                                         ValueNode* receiver);
  MaybeReduceResult TryBuildAndAllocateJSAsyncFunctionObject(
      ValueNode* closure, ValueNode* receiver, VirtualObject* promise);

  ReduceResult BuildGenericConstruct(
      ValueNode* target, ValueNode* new_target, ValueNode* context,
      const CallArguments& args,
      const compiler::FeedbackSource& feedback_source =
          compiler::FeedbackSource());

  MaybeReduceResult TryReduceConstructArrayConstructor(
      compiler::JSFunctionRef target_function, ValueNode* new_target,
      CallArguments& args,
      compiler::OptionalAllocationSiteRef maybe_allocation_site = {});
  MaybeReduceResult TryReduceConstructBuiltin(
      compiler::JSFunctionRef target_function,
      compiler::SharedFunctionInfoRef target_sfi, ValueNode* target,
      ValueNode* new_target, CallArguments& args);
  MaybeReduceResult TryReduceJSConstructStub(
      compiler::JSFunctionRef function,
      compiler::SharedFunctionInfoRef shared_function_info, ValueNode* target,
      ValueNode* new_target, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryReduceConstruct(
      compiler::HeapObjectRef target_constant, ValueNode* target,
      ValueNode* new_target, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  MaybeReduceResult TryReduceConstructWithSpreadForArgumentsObject(
      ValueNode* target, ValueNode* new_target, CallArguments& args,
      VirtualObject* arguments_object,
      const compiler::FeedbackSource& feedback_source);
  ReduceResult BuildConstruct(ValueNode* target, ValueNode* new_target,
                              CallArguments& args,
                              const compiler::FeedbackSource& feedback_source);

  MaybeReduceResult TryBuildScriptContextStore(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildPropertyCellStore(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildGlobalStore(
      const compiler::GlobalAccessFeedback& global_access_feedback);

  MaybeReduceResult TryBuildScriptContextLoad(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildPropertyCellLoad(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  MaybeReduceResult TryBuildGlobalLoad(
      const compiler::GlobalAccessFeedback& global_access_feedback);

  MaybeReduceResult TryBuildFindNonDefaultConstructorOrConstruct(
      ValueNode* this_function, ValueNode* new_target,
      std::pair<interpreter::Register, interpreter::Register> result);

  ReduceResult BuildSmiUntag(ValueNode* node);
  ReduceResult BuildGetCharCodeAt(ValueNode* string, ValueNode* index) {
    return reducer_.BuildGetCharCodeAt(string, index);
  }

  ReduceResult BuildCheckSmi(ValueNode* object);
  ReduceResult BuildCheckNumber(ValueNode* object);
  ReduceResult BuildCheckHeapObject(ValueNode* object);
  ReduceResult BuildCheckInstanceType(ValueNode* object, NodeType target_type,
                                      InstanceType first, InstanceType last);
  ReduceResult BuildCheckJSFunction(ValueNode* object);
  ReduceResult BuildCheckJSReceiver(ValueNode* object);
  ReduceResult BuildCheckJSReceiverOrNullOrUndefined(ValueNode* object);
  ReduceResult BuildCheckString(ValueNode* object) {
    return reducer_.BuildCheckString(object);
  }
  ReduceResult BuildCheckStringOrStringWrapper(ValueNode* object);
  ReduceResult BuildCheckStringOrOddball(ValueNode* object);
  ReduceResult BuildCheckSymbol(ValueNode* object);
  ReduceResult BuildCheckBigInt(ValueNode* object);
  ReduceResult BuildCheckMaps(
      ValueNode* object, base::Vector<const compiler::MapRef> maps,
      std::optional<ValueNode*> map = {},
      bool has_deprecated_map_without_migration_target = false,
      bool migration_done_outside = false) {
    return reducer_.BuildCheckMaps(object, maps, map,
                                   has_deprecated_map_without_migration_target,
                                   migration_done_outside);
  }
  ReduceResult BuildTransitionElementsKindOrCheckMap(
      ValueNode* heap_object, ValueNode* object_map,
      const ZoneVector<compiler::MapRef>& transition_sources,
      compiler::MapRef transition_target);
  ReduceResult BuildCompareMaps(
      ValueNode* heap_object, ValueNode* object_map,
      base::Vector<const compiler::MapRef> maps,
      MaglevSubGraphBuilder* sub_graph,
      std::optional<MaglevSubGraphBuilder::Label>& if_not_matched,
      std::optional<int> future_bind_offset = std::nullopt);
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

  ReduceResult BuildConvertHoleToUndefined(ValueNode* node);
  ReduceResult BuildCheckNotHole(ValueNode* node);

  ReduceResult BuildLogicalNot(ValueNode* value);
  ReduceResult BuildTestUndetectable(ValueNode* value);
  ReduceResult BuildToNumberOrToNumeric(Object::Conversion mode);

  enum class TrackObjectMode { kLoad, kStore };
  bool CanTrackObjectChanges(ValueNode* object, TrackObjectMode mode);
  bool IsFieldConstant(ValueNode* object, int offset);

  bool CanElideWriteBarrier(ValueNode* object, ValueNode* value) {
    return reducer_.CanElideWriteBarrier(object, value);
  }

  ReduceResult BuildLoadMap(ValueNode* object);

  void TryBuildStoreTaggedFieldToAllocation(ValueNode* object, ValueNode* value,
                                            int offset);
  std::optional<ValueNode*> TryBuildLoadTaggedFieldFromAllocation(
      ValueNode* object, int offset);
  MaybeReduceResult TryBuildLoadFixedDoubleArrayElementFromAllocation(
      ValueNode* elements, int index);
  bool TryElideWriteBarrierForAllocation(ValueNode* object, ValueNode* value);

  ReduceResult BuildLoadTaggedField(
      ValueNode* object, uint32_t offset, NodeType type = NodeType::kUnknown,
      bool is_const = false, PropertyKey key = PropertyKey::None(),
      IsArrayLength is_array_length = IsArrayLength::kNo,
      compiler::OptionalMapRef stable_field_map = {}) {
    return reducer_.BuildLoadTaggedField(object, offset, type, is_const, key,
                                         is_array_length, stable_field_map);
  }

  ReduceResult BuildStoreTaggedField(
      ValueNode* object, ValueNode* value, int offset,
      StoreTaggedMode store_mode,
      PropertyKey property_key = PropertyKey::None(),
      MaybeAssignedFlag maybe_assigned = kMaybeAssigned) {
    return reducer_.BuildStoreTaggedField(object, value, offset, store_mode,
                                          property_key, maybe_assigned);
  }
  ReduceResult BuildStoreTaggedFieldNoWriteBarrier(
      ValueNode* object, ValueNode* value, int offset,
      StoreTaggedMode store_mode,
      PropertyKey property_key = PropertyKey::None()) {
    return reducer_.BuildStoreTaggedFieldNoWriteBarrier(
        object, value, offset, store_mode, property_key);
  }

  ReduceResult BuildLoadFixedArrayElement(ValueNode* elements, int index,
                                          LoadType type = LoadType::kUnknown);
  ReduceResult BuildLoadFixedArrayElement(ValueNode* elements, ValueNode* index,
                                          LoadType type = LoadType::kUnknown);
  ReduceResult BuildStoreFixedArrayElement(ValueNode* elements,
                                           ValueNode* index, ValueNode* value);

  ReduceResult BuildLoadFixedDoubleArrayElement(ValueNode* elements,
                                                ValueNode* index) {
    return reducer_.BuildLoadFixedDoubleArrayElement(elements, index);
  }
  ReduceResult BuildStoreFixedDoubleArrayElement(ElementsKind elements_kind,
                                                 ValueNode* elements,
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
      compiler::PropertyAccessInfo const& access_info) {
    return reducer_.TryFoldLoadConstantDataField(holder, access_info);
  }
  std::optional<Float64> TryFoldLoadConstantDoubleField(
      compiler::JSObjectRef holder,
      compiler::PropertyAccessInfo const& access_info);

  // Returns the loaded value node but doesn't update the accumulator yet.
  ReduceResult BuildLoadField(compiler::PropertyAccessInfo const& access_info,
                              ValueNode* lookup_start_object,
                              compiler::NameRef name);
  MaybeReduceResult BuildLoadDictionaryField(
      ValueNode* receiver, compiler::PropertyAccessInfo const& access_info,
      ValueNode* lookup_start_object, compiler::NameRef name,
      compiler::FeedbackSource const& feedback_source);
  MaybeReduceResult TryBuildStoreField(
      compiler::PropertyAccessInfo const& access_info, ValueNode* receiver,
      compiler::AccessMode access_mode, compiler::NameRef name);
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
  ReduceResult BuildSetKeyedProperty(
      ValueNode* object, ValueNode* index, compiler::AccessMode access_mode,
      const compiler::FeedbackSource& feedback_source,
      const compiler::ProcessedFeedback& processed_feedback,
      base::FunctionRef<ReduceResult()> generic_setter);

  ValueNode* BuildLoadFixedArrayLength(ValueNode* fixed_array);
  ReduceResult BuildLoadJSArrayLength(ValueNode* js_array,
                                      NodeType length_type = NodeType::kSmi) {
    return reducer_.BuildLoadJSArrayLength(js_array, length_type);
  }
  ReduceResult BuildLoadJSDataViewByteLength(ValueNode* js_data_view);
  ReduceResult BuildLoadJSDataViewDataPointer(ValueNode* js_data_view);
  ReduceResult BuildLoadElements(
      ValueNode* object, std::optional<ElementsKind> kind = std::nullopt) {
    return reducer_.BuildLoadElements(object, kind);
  }

  ReduceResult BuildLoadJSFunctionFeedbackCell(ValueNode* closure);
  ReduceResult BuildLoadJSFunctionContext(ValueNode* closure);

  ReduceResult TryBuildCheckInt32Condition(ValueNode* lhs, ValueNode* rhs,
                                           AssertCondition condition,
                                           DeoptimizeReason reason) {
    return reducer_.TryBuildCheckInt32Condition(lhs, rhs, condition, reason);
  }

  MaybeReduceResult TryBuildPropertyLoad(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info,
      compiler::FeedbackSource const& feedback_source);
  MaybeReduceResult TryBuildPropertyStore(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info,
      compiler::AccessMode access_mode);
  MaybeReduceResult TryBuildPropertyAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info,
      compiler::AccessMode access_mode,
      compiler::FeedbackSource const& feedback_source);
  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildNamedAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NamedAccessFeedback const& feedback,
      compiler::FeedbackSource const& feedback_source,
      compiler::AccessMode access_mode,
      GenericAccessFunc&& build_generic_access);
  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildHomomorphicNamedAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::HomomorphicPropertyAccessFeedback const& feedback,
      compiler::FeedbackSource const& feedback_source,
      compiler::AccessMode access_mode,
      GenericAccessFunc&& build_generic_access);

  template <typename GenericAccessFunc>
  MaybeReduceResult TryBuildProxyPropertyAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::ProxyFeedback const& feedback,
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
                                           ElementsKind elements_kind,
                                           ValueNode* value);
  ReduceResult BuildStoreConstantTypedArrayElement(
      compiler::JSTypedArrayRef typed_array, ValueNode* index,
      ElementsKind elements_kind, ValueNode* value);

  MaybeReduceResult TryBuildElementAccessOnString(
      ValueNode* object, ValueNode* index,
      const compiler::ElementAccessFeedback& access_info,
      compiler::KeyedAccessMode const& keyed_mode);
  MaybeReduceResult TryBuildElementAccessOnTypedArray(
      ValueNode* object, ValueNode* index,
      const compiler::ElementAccessInfo& access_info,
      compiler::KeyedAccessMode const& keyed_mode);
  ReduceResult BuildElementLoadOnJSArrayOrJSObject(
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
      compiler::FeedbackSource const& feedback_source,
      compiler::AccessMode access_mode,
      const ZoneVector<compiler::PropertyAccessInfo>& access_infos,
      GenericAccessFunc&& build_generic_access);

  // Checks if {index_object} is a int32 and {object} is an array whose length
  // is more than {index_object}. If that's the case, then we don't need a bound
  // check to store at offset {index_object}. In that case, this function
  // returns the int32 index, otherwise, returns std::nullopt.
  std::optional<int32_t> CanElideBoundCheckAndResizing(
      ValueNode* object, ValueNode* index_object, bool is_jsarray,
      const compiler::KeyedAccessMode& keyed_mode);

  struct ContinuationOffsets {
    int last_continuation;
    int after_continuation;
  };
  std::optional<ContinuationOffsets>
  FindContinuationForPolymorphicPropertyLoad();
  std::optional<ContinuationOffsets>
  FindContinuationForPolymorphicPropertyLoadImpl();
  ReduceResult BuildContinuationForPolymorphicPropertyLoad(
      const ContinuationOffsets& offsets);
  void AdvanceThroughContinuationForPolymorphicPropertyLoad(
      const ContinuationOffsets& offsets);

  // Load elimination -- when loading or storing a simple property without
  // side effects, record its value, and allow that value to be reused on
  // subsequent loads.
  MaybeReduceResult TryReuseKnownPropertyLoad(ValueNode* lookup_start_object,
                                              compiler::NameRef name) {
    return reducer_.TryReuseKnownPropertyLoad(lookup_start_object, name);
  }
  ReduceResult BuildLoadStringLength(ValueNode* string) {
    return reducer_.BuildLoadStringLength(string);
  }

  // Converts the input node to a representation that's valid to store into an
  // array with elements kind |kind|.
  ReduceResult ConvertForStoring(ValueNode* node, ElementsKind kind);

  bool IsEscaping(InlinedAllocation* allocation);
  VirtualObject* GetObjectFromAllocation(InlinedAllocation* allocation);
  VirtualObject* GetModifiableObjectFromAllocation(
      InlinedAllocation* allocation);

  VirtualObject* DeepCopyVirtualObject(VirtualObject* vobj);

  void AddDeoptUse(ValueNode* node);
  void AddMaterializedDeoptUse(ValueNode* node);
  void AddDeoptUse(VirtualObject* alloc);

  void AddDeoptUseToScopeData(const DeoptFrame::FrameData& data);

  DeoptFrame* RecursivelyWrapDeoptFrameWithContinuations(
      const DeoptFrame& frame,
      const MaglevGraphBuilder::LazyDeoptFrameScope* parent_scope);

  std::optional<VirtualObject*> TryGetNonEscapingArgumentsOrArray(
      ValueNode* value);

  MaybeReduceResult TryBuildFastCreateObjectOrArrayLiteral(
      const compiler::LiteralFeedback& feedback);
  std::optional<VirtualObject*> TryReadBoilerplateForFastLiteral(
      compiler::JSObjectRef boilerplate, AllocationType allocation,
      int max_depth, int* max_properties);

  ValueNode* BuildInlinedArgumentsElements(int start_index, int length);
  ValueNode* BuildInlinedUnmappedArgumentsElements(int mapped_count);

  template <CreateArgumentsType type>
  VirtualObject* BuildVirtualArgumentsObject();
  template <CreateArgumentsType type>
  ReduceResult BuildAndAllocateArgumentsObject();

  bool CanAllocateSloppyArgumentElements();
  bool CanAllocateInlinedArgumentElements();

  MaybeReduceResult TryBuildInlinedAllocatedContext(
      compiler::MapRef map, int context_length, compiler::ScopeInfoRef scope,
      ValueNode* extension = nullptr);

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
      NodeType assumed_input_type);
  template <Operation kOperation>
  ReduceResult BuildInt32BinaryOperationNode();
  template <Operation kOperation>
  ReduceResult BuildInt32BinarySmiOperationNode();
  template <Operation kOperation>
  ReduceResult BuildTruncatingInt32BinaryOperationNodeForToNumber(
      NodeType assumed_input_type);
  template <Operation kOperation>
  ReduceResult BuildTruncatingInt32BinarySmiOperationNodeForToNumber(
      NodeType assumed_input_type);

  template <Operation kOperation>
  ReduceResult BuildFloat64UnaryOperationNodeForToNumber(
      NodeType assumed_input_type);
  template <Operation kOperation>
  ReduceResult BuildFloat64BinaryOperationNodeForToNumber(
      NodeType assumed_input_type);
  template <Operation kOperation>
  ReduceResult BuildFloat64BinarySmiOperationNodeForToNumber(
      NodeType assumed_input_type);

  ReduceResult BuildFloat64SpeculateSafeAdd(ValueNode* left, ValueNode* right);

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
  uint32_t StringLengthStaticLowerBound(ValueNode* string, int max_depth = 2);
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
  void MergeDeadIntoFrameState(int target, bool is_fallthrough = false);
  void MergeDeadLoopIntoFrameState(int target);
  void MergeIntoInlinedReturnFrameStateForReturn(BasicBlock* block);
  void MergeIntoInlinedReturnFrameStateForSuspendGenerator(BasicBlock* block);

  bool HasValidInitialMap(compiler::JSFunctionRef new_target,
                          compiler::JSFunctionRef constructor);

  ReduceResult BuildTaggedEqual(ValueNode* lhs, ValueNode* rhs) {
    return reducer_.BuildTaggedEqual(lhs, rhs);
  }
  ReduceResult BuildTaggedEqual(ValueNode* lhs, RootIndex rhs_index) {
    return reducer_.BuildTaggedEqual(lhs, rhs_index);
  }

  using BranchResult = ::v8::internal::maglev::BranchResult;
  using BranchType = ::v8::internal::maglev::BranchType;
  using BranchSpecializationMode =
      ::v8::internal::maglev::BranchSpecializationMode;
  using BranchBuilder = MaglevReducer<MaglevGraphBuilder>::BranchBuilder;

  // Builds bytecode-jump-target branch nodes. Owns all bytecode-mode branch
  // state and logic. The shared BuildBranchIf* helpers below are templated on
  // the builder type, so they work with this and with the label-mode
  // MaglevReducer::BranchBuilder (both satisfy the same duck-typed interface).
  class BytecodeBranchBuilder
      : public BranchBuilderBase<BytecodeBranchBuilder> {
   private:
    struct AccumulatorPatch {
      ValueNode* node;
      RootIndex root_index;
      BranchType jump_type;
    };

   public:
    BytecodeBranchBuilder(MaglevGraphBuilder* builder, BranchType jump_type)
        : BranchBuilderBase<BytecodeBranchBuilder>(jump_type),
          builder_(builder),
          jump_target_offset_(builder->iterator_.GetJumpTargetOffset()),
          fallthrough_offset_(builder->iterator_.next_offset()) {}

    BranchResult FromBool(bool value) const;
    template <typename ControlNodeT, typename... Args>
    BranchResult Build(std::initializer_list<ValueNode*> inputs,
                       Args&&... args);

    void SetBranchSpecializationMode(BranchSpecializationMode mode) {
      specialization_mode_ = mode;
    }

    // While in scope, patches the interpreter accumulator in the branch's
    // successors (JumpIf* peephole handling).
    class PatchAccumulatorInBranchScope {
     public:
      PatchAccumulatorInBranchScope(BytecodeBranchBuilder& builder,
                                    ValueNode* node, RootIndex root_index)
          : builder_(builder),
            patch_{node, root_index, builder.GetCurrentBranchType()} {
        builder_.patch_ = &patch_;
      }
      ~PatchAccumulatorInBranchScope() { builder_.patch_ = nullptr; }

     private:
      BytecodeBranchBuilder& builder_;
      AccumulatorPatch patch_;
    };

   private:
    // The value to write to the accumulator in the given branch direction.
    ValueNode* PatchedAccumulator(BranchType branch_type) const;

    MaglevGraphBuilder* builder_;
    int jump_target_offset_;
    int fallthrough_offset_;
    BranchSpecializationMode specialization_mode_ =
        BranchSpecializationMode::kDefault;
    const AccumulatorPatch* patch_ = nullptr;

    friend class PatchAccumulatorInBranchScope;
  };

  BytecodeBranchBuilder CreateBranchBuilder(
      BranchType jump_type = BranchType::kBranchIfTrue) {
    return BytecodeBranchBuilder(this, jump_type);
  }

  template <typename BranchBuilderT>
  BranchResult BuildBranchIfRootConstant(BranchBuilderT& builder,
                                         ValueNode* node, RootIndex root_index);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfToBooleanTrue(BranchBuilderT& builder,
                                          ValueNode* node);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfReferenceEqual(BranchBuilderT& builder,
                                           ValueNode* lhs, ValueNode* rhs);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfInt32Compare(BranchBuilderT& builder, Operation op,
                                         ValueNode* lhs, ValueNode* rhs);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfUint32Compare(BranchBuilderT& builder, Operation op,
                                          ValueNode* lhs, ValueNode* rhs);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfUndetectable(BranchBuilderT& builder,
                                         ValueNode* value);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfJSReceiver(BranchBuilderT& builder,
                                       ValueNode* value);

  template <typename BranchBuilderT>
  BranchResult BuildBranchIfTrue(BranchBuilderT& builder, ValueNode* node);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfNull(BranchBuilderT& builder, ValueNode* node);
  template <typename BranchBuilderT>
  BranchResult BuildBranchIfUndefined(BranchBuilderT& builder, ValueNode* node);
  BasicBlock* BuildBranchIfReferenceEqual(ValueNode* lhs, ValueNode* rhs,
                                          BasicBlockRef* true_target,
                                          BasicBlockRef* false_target);

  ReduceResult BuildInt32Max(ValueNode* a, ValueNode* b);
  ReduceResult BuildInt32Min(ValueNode* a, ValueNode* b);

  ReduceResult Select(base::FunctionRef<BranchResult(BranchBuilder&)> cond,
                      base::FunctionRef<ReduceResult()> if_true,
                      base::FunctionRef<ReduceResult()> if_false);

  void MarkBranchDeadAndJumpIfNeeded(bool is_jump_taken);

  void OsrPrewalk();
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

  bool IsInsideLoopInTheCurrentFunction() const;
  bool IsInsideLoop() const;
  int GetLoopDepth() const;

  // The fake offset used as a target for all exits of an inlined function.
  int inline_exit_offset() const {
    DCHECK(is_inline());
    return bytecode().length();
  }

  MaglevReducer<MaglevGraphBuilder> reducer_;

  LocalIsolate* const local_isolate_;
  MaglevCompilationUnit* const compilation_unit_;
  MaglevCallerDetails* caller_details_;
  InliningTreeDebugInfo* current_inlining_tree_debug_info_ = nullptr;

  // Cache the heap broker since we access it a bunch.
  compiler::JSHeapBroker* broker_ = compilation_unit_->broker();

  // Cache flags.
  const CompilationFlags flags_;

  Graph* const graph_;
  Tracer tracer_;
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
  void PrewalkBytecode();

  ZoneVector<int> decremented_predecessor_offsets_;
  // The set of loop headers reachable from the entrypoint.
  BitVector loop_headers_;
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

  BasicBlockRef* jump_targets_;
  MergePointInterpreterFrameState** merge_states_;

  RegisterFrameArray<compiler::OptionalScopeInfoRef> register_scope_infos_;
  compiler::OptionalScopeInfoRef accumulator_scope_info_;

  InterpreterFrameState current_interpreter_frame_;

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
  uint32_t next_handler_table_index_ = 0;

  LazyDeoptResultLocationScope* lazy_deopt_result_location_scope_ = nullptr;

  struct HandlerTableEntry {
    int end;
    int handler;
  };
  ZoneStack<HandlerTableEntry> catch_block_stack_;

#ifdef DEBUG
  bool IsNodeCreatedForThisBytecode(ValueNode* node) const;
#endif

  bool CanSpeculateCall() const { return reducer_.CanSpeculateCall(); }

  bool CanSpeculateCall(
      std::initializer_list<SpeculationMode> supported_modes) const {
    return reducer_.CanSpeculateCall(supported_modes);
  }

  inline void MarkNodeDead(Node* node) {
    for (int i = 0; i < node->input_count(); ++i) {
      node->input(i).clear();
    }
    node->OverwriteWith(Opcode::kDead);
  }

  ZoneUnorderedMap<KnownNodeAspects::LoadedContextSlotsKey, Node*>
      unobserved_context_slot_stores_;

  ZoneMap<int, compiler::OptionalScopeInfoRef> dead_scope_infos_;

  bool is_resumable_function_ = false;

  // When set, inline only small functions.
  bool only_inline_small_ = false;

  bool may_have_changed_maps() const { return may_have_changed_maps_; }
  bool may_have_changed_maps_ = false;
};

template <bool is_possible_map_change>
void MaglevGraphBuilder::ResetBuilderCachedState() {
  latest_checkpointed_frame_ = nullptr;

  // If a map might have changed, then we need to re-check it for for-in.
  // TODO(leszeks): Track this on merge states / known node aspects, rather
  // than on the graph, so that it can survive control flow.
  if constexpr (is_possible_map_change) {
    may_have_changed_maps_ = true;
    current_for_in_state.receiver_needs_map_check = true;
  }
}

template <typename NodeT>
void MaglevGraphBuilder::MarkPossibleSideEffect(NodeT* node) {
  // We only need to clear unstable node aspects on the current builder, not
  // the parent, since we'll anyway copy the known_node_aspects to the parent
  // once we finish the inlined function.

  if constexpr (NodeT::kProperties.can_read() ||
                NodeT::kProperties.can_deopt() ||
                NodeT::kProperties.can_throw()) {
    unobserved_context_slot_stores_.clear();
  }

  if constexpr (Node::opcode_of<NodeT> != Opcode::kAllocationBlock &&
                (NodeT::kProperties.can_deopt() ||
                 NodeT::kProperties.can_throw() ||
                 NodeT::kProperties.can_allocate())) {
    reducer_.ClearCurrentAllocationBlock();
  }

  // Don't do anything for nodes without side effects.
  if constexpr (!NodeT::kProperties.can_write()) return;

  if (is_loop_effect_tracking()) {
    if constexpr (IsElementsArrayWrite(Node::opcode_of<NodeT>)) {
      loop_effects_->keys_cleared.insert(PropertyKey::Elements());
    } else if constexpr (std::is_same_v<NodeT, StoreMap>) {
      // Only transitioning Map stores can invalidate unstable maps.
      if (node->template Cast<StoreMap>()->is_transitioning()) {
        loop_effects_->unstable_aspects_cleared = true;
      }
    } else if constexpr (std::is_same_v<NodeT, TransitionElementsKind> ||
                         std::is_same_v<NodeT,
                                        TransitionElementsKindOrCheckMap>) {
      // Elements-kind transitions only invalidate unstable map facts and
      // cached elements of objects that may hold a source map.
      loop_effects_->elements_kind_transitioned = true;
    } else if constexpr (!IsSimpleFieldStore(Node::opcode_of<NodeT>) &&
                         !IsTypedArrayStore(Node::opcode_of<NodeT>)) {
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

// Builder-side Subgraph: thin facade over the existing MaglevSubGraphBuilder.
// Each primitive mutates the live graph as the bytecode visitor would.
// TODO(victorgomes): Remove MaglevSubGraphBuilder and consolidate
// everything in the Subgraph specialization.
template <>
class Subgraph<MaglevGraphBuilder>
    : public MaglevGraphBuilder::MaglevSubGraphBuilder {
 public:
  using MaglevSubGraphBuilder = MaglevGraphBuilder::MaglevSubGraphBuilder;
  using Variable = MaglevSubGraphBuilder::Variable;
  using Label = MaglevSubGraphBuilder::Label;
  using LoopLabel = MaglevSubGraphBuilder::LoopLabel;

  Subgraph(MaglevReducer<MaglevGraphBuilder>* reducer, int variable_count)
      : MaglevSubGraphBuilder(reducer->base_, variable_count) {}

  template <typename FCond, typename FTrue, typename FFalse>
  ReduceResult Branch(std::initializer_list<Variable*> vars, FCond cond,
                      FTrue if_true, FFalse if_false);
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
