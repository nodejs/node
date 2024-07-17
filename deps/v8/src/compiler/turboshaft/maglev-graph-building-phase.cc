// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/maglev-graph-building-phase.h"

#include <type_traits>

#include "src/base/logging.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/common/globals.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/frame-states.h"
#include "src/compiler/globals.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/maglev-early-lowering-reducer-inl.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/handles/global-handles-inl.h"
#include "src/handles/handles.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-phi-representation-selector.h"
#include "src/objects/elements-kind.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/objects.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

namespace {

MachineType MachineTypeFor(maglev::ValueRepresentation repr) {
  switch (repr) {
    case maglev::ValueRepresentation::kTagged:
      return MachineType::AnyTagged();
    case maglev::ValueRepresentation::kInt32:
      return MachineType::Int32();
    case maglev::ValueRepresentation::kUint32:
      return MachineType::Uint32();
    case maglev::ValueRepresentation::kIntPtr:
      return MachineType::IntPtr();
    case maglev::ValueRepresentation::kFloat64:
      return MachineType::Float64();
    case maglev::ValueRepresentation::kHoleyFloat64:
      // TODO(dmercadier): is this correct?
      return MachineType::Float64();
  }
}

// TODO(dmercadier): use simply .contains once we have access to C++20.
template <typename K, typename V>
bool MapContains(ZoneUnorderedMap<K, V> map, K key) {
  return map.find(key) != map.end();
}

int ElementsKindSize(ElementsKind element_kind) {
  switch (element_kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                           \
    DCHECK_LE(sizeof(ctype), 8);                  \
    return sizeof(ctype);
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
#undef TYPED_ARRAY_CASE
  }
}

}  // namespace

// This reducer tracks the Maglev origin of the Turboshaft blocks that we build
// during the translation. This is then used when reordering Phi inputs.
template <class Next>
class BlockOriginTrackingReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(BlockOriginTracking)
  void SetMaglevInputBlock(const maglev::BasicBlock* block) {
    maglev_input_block_ = block;
  }
#ifdef DEBUG
  const maglev::BasicBlock* maglev_input_block() const {
    return maglev_input_block_;
  }
#endif
  void Bind(Block* block) {
    Next::Bind(block);
    // The 1st block we bind doesn't exist in Maglev and is meant to hold
    // Constants (which in Maglev are not in any block), and thus
    // {maglev_input_block_} should still be nullptr. In all other cases,
    // {maglev_input_block_} should not be nullptr.
    DCHECK_EQ(maglev_input_block_ == nullptr,
              block == &__ output_graph().StartBlock());
    turboshaft_block_origins_[block->index()] = maglev_input_block_;
  }

  const maglev::BasicBlock* GetMaglevOrigin(const Block* block) {
    DCHECK_NOT_NULL(turboshaft_block_origins_[block->index()]);
    return turboshaft_block_origins_[block->index()];
  }

 private:
  const maglev::BasicBlock* maglev_input_block_ = nullptr;
  GrowingBlockSidetable<const maglev::BasicBlock*> turboshaft_block_origins_{
      __ phase_zone()};
};

class GraphBuilder {
 public:
  using AssemblerT =
      TSAssembler<BlockOriginTrackingReducer, MaglevEarlyLoweringReducer,
                  MachineOptimizationReducer, VariableReducer,
                  RequiredOptimizationReducer, ValueNumberingReducer>;

  GraphBuilder(PipelineData* data, Graph& graph, Zone* temp_zone,
               maglev::MaglevCompilationUnit* maglev_compilation_unit)
      : data_(data),
        temp_zone_(temp_zone),
        assembler_(data, graph, graph, temp_zone),
        maglev_compilation_unit_(maglev_compilation_unit),
        node_mapping_(temp_zone),
        block_mapping_(temp_zone),
        regs_to_vars_(temp_zone),
        loop_single_edge_predecessors_(temp_zone) {}

  void PreProcessGraph(maglev::Graph* graph) {
    for (maglev::BasicBlock* block : *graph) {
      block_mapping_[block] =
          block->is_loop() ? __ NewLoopHeader() : __ NewBlock();
    }
    // Constants are not in a block in Maglev but are in Turboshaft. We bind a
    // block now, so that Constants can then be emitted.
    __ Bind(__ NewBlock());

    if (maglev_compilation_unit_->bytecode()
            .incoming_new_target_or_generator_register()
            .is_valid()) {
      // The Maglev graph might contain a RegisterInput for
      // kJavaScriptCallNewTargetRegister later in the graph, which in
      // Turboshaft is represented as a Parameter. We create this Parameter
      // here, because the Instruction Selector tends to be unhappy when
      // Parameters are defined late in the graph.
      int new_target_index = Linkage::GetJSCallNewTargetParamIndex(
          maglev_compilation_unit_->parameter_count());
      new_target_param_ = __ Parameter(
          new_target_index, RegisterRepresentation::Tagged(), "%new.target");
    }

    // Maglev nodes often don't have the NativeContext as input, but instead
    // rely on the MaglevAssembler to provide it during code generation, unlike
    // Turboshaft nodes, which need the NativeContext as an explicit input if
    // they use it. We thus emit a single NativeContext constant here, which we
    // reuse later to construct Turboshaft nodes.
    native_context_ =
        __ HeapConstant(broker_->target_native_context().object());
  }

  void PostProcessGraph(maglev::Graph* graph) {}

  void PreProcessBasicBlock(maglev::BasicBlock* maglev_block) {
    // Note that it's important to call SetMaglevInputBlock before calling Bind,
    // so that BlockOriginTrackingReducer::Bind records the correct predecessor
    // for the current block.
    __ SetMaglevInputBlock(maglev_block);

    Block* turboshaft_block = Map(maglev_block);

    if (__ current_block() != nullptr) {
      // The first block for Constants doesn't end with a Jump, so we add one
      // now.
      __ Goto(turboshaft_block);
    }

#ifdef DEBUG
    loop_phis_first_input_.clear();
    loop_phis_first_input_index_ = -1;
    catch_block_begin_ = V<Object>::Invalid();
#endif

    if (maglev_block->is_loop() &&
        MapContains(loop_single_edge_predecessors_, maglev_block)) {
      EmitLoopSinglePredecessorBlock(maglev_block);
    }

    // SetMaglevInputBlock should have been called before calling Bind, and the
    // current `maglev_input_block` should thus already be `maglev_block`.
    DCHECK_EQ(__ maglev_input_block(), maglev_block);
    if (!__ Bind(turboshaft_block)) {
      // The current block is not reachable.
      return;
    }

    if (maglev_block->is_loop()) {
      // The "permutation" stuff that comes afterwards in this function doesn't
      // apply to loops, since loops always have 2 predecessors in Turboshaft,
      // and in both Turboshaft and Maglev, the backedge is always the last
      // predecessors, so we never need to reorder phi inputs.
      return;
    } else if (maglev_block->is_exception_handler_block()) {
      // We need to emit the CatchBlockBegin at the begining of this block. Note
      // that if this block has multiple predecessors (because multiple throwing
      // operations are caught by the same catch handler), then edge splitting
      // will have already created CatchBlockBegin operations in the
      // predecessors, and calling `__ CatchBlockBegin` now will actually only
      // emit a Phi of the CatchBlockBegin of the predecessors (which is exactly
      // what we want). See the comment above CatchBlockBegin in
      // TurboshaftAssemblerOpInterface.
      catch_block_begin_ = __ CatchBlockBegin();
    }

    // Because of edge splitting in Maglev (which happens on Bind rather than on
    // Goto), predecessors in the Maglev graph are not always ordered by their
    // position in the graph (ie, block 4 could be the second predecessor and
    // block 5 the first one). However, since we're processing the graph "in
    // order" (because that's how the maglev GraphProcessor works), predecessors
    // in the Turboshaft graph will be ordered by their position in the graph.
    // Additionally, optimizations during the translation (like constant folding
    // by MachineOptimizationReducer) could change control flow and remove
    // predecessors (by changing a Branch into a Goto for instance).
    // We thus compute in {predecessor_permutation_} a map from Maglev
    // predecessor index to Turboshaft predecessor index, and we'll use this
    // later when emitting Phis to reorder their inputs.
    predecessor_permutation_.clear();
    if (maglev_block->has_phi() &&
        // We ignore this for exception phis since they have no inputs in Maglev
        // anyways, and in Turboshaft we rely on {regs_to_vars_} to populate
        // their inputs (and also, Maglev exception blocks have no
        // predecessors).
        !maglev_block->is_exception_handler_block()) {
      // Collecting the Maglev predecessors.
      base::SmallVector<const maglev::BasicBlock*, 16> maglev_predecessors;
      maglev_predecessors.resize_no_init(maglev_block->predecessor_count());
      for (int i = 0; i < maglev_block->predecessor_count(); ++i) {
        maglev_predecessors[i] = maglev_block->predecessor_at(i);
      }

      predecessor_permutation_.resize_and_init(
          maglev_block->predecessor_count(), Block::kInvalidPredecessorIndex);
      int index = turboshaft_block->PredecessorCount() - 1;
      for (const Block* pred : turboshaft_block->PredecessorsIterable()) {
        // Finding out to which Maglev predecessor {pred} corresponds.
        const maglev::BasicBlock* orig = __ GetMaglevOrigin(pred);
        auto orig_it = std::find(maglev_predecessors.begin(),
                                 maglev_predecessors.end(), orig);
        DCHECK_NE(orig_it, maglev_predecessors.end());
        auto orig_index = std::distance(maglev_predecessors.begin(), orig_it);

        predecessor_permutation_[orig_index] = index;
        index--;
      }
      DCHECK_EQ(index, -1);
    }
  }

  void EmitLoopSinglePredecessorBlock(maglev::BasicBlock* block) {
    DCHECK(block->is_loop());
    DCHECK(MapContains(loop_single_edge_predecessors_, block));
    Block* loop_pred = loop_single_edge_predecessors_[block];
    __ Bind(loop_pred);

    // Now we need to emit Phis (one per loop phi in {block}, which should
    // contain the same input except for the backedge).
    loop_phis_first_input_.clear();
    loop_phis_first_input_index_ = 0;
    for (maglev::Phi* phi : *block->phis()) {
      base::SmallVector<OpIndex, 16> inputs;
      constexpr int kSkipBackedge = 1;
      for (int i = 0; i < phi->input_count() - kSkipBackedge; i++) {
        inputs.push_back(Map(phi->input(i)));
      }
      loop_phis_first_input_.push_back(
          __ Phi(base::VectorOf(inputs),
                 RegisterRepresentationFor(phi->value_representation())));
    }

    // Actually jumping to the loop.
    __ Goto(Map(block));
  }

  maglev::ProcessResult Process(maglev::Constant* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ HeapConstant(node->object().object()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::RootConstant* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ HeapConstant(MakeRef(broker_, node->DoReify(local_isolate_))
                                     .AsHeapObject()
                                     .object()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Int32Constant* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Word32Constant(node->value()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Uint32Constant* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Word32Constant(node->value()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64Constant* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Float64Constant(
                     base::bit_cast<double>(node->value().get_bits())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::SmiConstant* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ SmiConstant(node->value()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TaggedIndexConstant* node,
                                const maglev::ProcessingState& state) {
    // TODO(dmercadier): should this really be a SmiConstant, or rather a
    // Word32Constant?
    SetMap(node, __ SmiConstant(node->value().ptr()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::InitialValue* node,
                                const maglev::ProcessingState& state) {
    // TODO(dmercadier): InitialValues are much simpler in Maglev because they
    // are mapped directly to interpreter registers, whereas Turbofan changes
    // the indices, making everything more complex. We should try to have the
    // same InitialValues in Turboshaft as in Maglev, in order to simplify
    // things.
#ifdef DEBUG
    char* debug_name = strdup(node->source().ToString().c_str());
#else
    char* debug_name = nullptr;
#endif
    interpreter::Register source = node->source();
    V<Object> value;
    if (source.is_function_closure()) {
      // The function closure is a Parameter rather than an OsrValue even when
      // OSR-compiling.
      value = __ Parameter(Linkage::kJSCallClosureParamIndex,
                           RegisterRepresentation::Tagged(), debug_name);
    } else if (maglev_compilation_unit_->is_osr()) {
      int index;
      if (source.is_current_context()) {
        index = Linkage::kOsrContextSpillSlotIndex;
      } else if (source == interpreter::Register::virtual_accumulator()) {
        index = Linkage::kOsrAccumulatorRegisterIndex;
      } else if (source.is_parameter()) {
        index = source.ToParameterIndex();
      } else {
        // For registers, recreate the index computed by FillWithOsrValues in
        // BytecodeGraphBuilder.
        index = source.index() + InterpreterFrameConstants::kExtraSlotCount +
                maglev_compilation_unit_->parameter_count();
      }
      value = __ OsrValue(index);
    } else {
      int index = source.ToParameterIndex();
      if (source.is_current_context()) {
        index = Linkage::GetJSCallContextParamIndex(
            maglev_compilation_unit_->parameter_count());
      } else {
        index = source.ToParameterIndex();
      }
      value = __ Parameter(index, RegisterRepresentation::Tagged(), debug_name);
    }
    SetMap(node, value);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::RegisterInput* node,
                                const maglev::ProcessingState& state) {
    DCHECK(maglev_compilation_unit_->bytecode()
               .incoming_new_target_or_generator_register()
               .is_valid());
    DCHECK_EQ(node->input(), kJavaScriptCallNewTargetRegister);
    DCHECK(new_target_param_.valid());
    SetMap(node, new_target_param_);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::FunctionEntryStackCheck* node,
                                const maglev::ProcessingState& state) {
    __ StackCheck(StackCheckOp::Kind::kJSFunctionHeader);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Phi* node,
                                const maglev::ProcessingState& state) {
    int input_count = node->input_count();
    RegisterRepresentation rep =
        RegisterRepresentationFor(node->value_representation());
    if (node->is_exception_phi()) {
      if (node->owner() == interpreter::Register::virtual_accumulator()) {
        DCHECK(catch_block_begin_.valid());
        SetMap(node, catch_block_begin_);
      } else {
        SetMap(node, __ GetVariable(regs_to_vars_[node->owner().index()]));
      }
      return maglev::ProcessResult::kContinue;
    }
    if (__ current_block()->IsLoop()) {
      DCHECK(state.block()->is_loop());
      OpIndex first_phi_input;
      if (state.block()->predecessor_count() > 2) {
        // This loop has multiple forward edge in Maglev, so we should have
        // created an intermediate block in Turboshaft, which will be the only
        // predecessor of the Turboshaft loop, and from which we'll find the
        // first input for this loop phi.
        DCHECK_EQ(loop_phis_first_input_.size(),
                  static_cast<size_t>(state.block()->phis()->LengthForTest()));
        DCHECK_GE(loop_phis_first_input_index_, 0);
        DCHECK_LT(loop_phis_first_input_index_, loop_phis_first_input_.size());
        DCHECK(MapContains(loop_single_edge_predecessors_, state.block()));
        DCHECK_EQ(loop_single_edge_predecessors_[state.block()],
                  __ current_block()->LastPredecessor());
        first_phi_input = loop_phis_first_input_[loop_phis_first_input_index_];
        loop_phis_first_input_index_++;
      } else {
        DCHECK_EQ(input_count, 2);
        DCHECK_EQ(state.block()->predecessor_count(), 2);
        DCHECK(loop_phis_first_input_.empty());
        first_phi_input = Map(node->input(0));
      }
      SetMap(node, __ PendingLoopPhi(first_phi_input, rep));
    } else {
      DCHECK(!predecessor_permutation_.empty());
      base::SmallVector<OpIndex, 16> inputs;
      inputs.resize_and_init(__ current_block()->PredecessorCount());
      for (int i = 0; i < input_count; ++i) {
        if (predecessor_permutation_[i] != Block::kInvalidPredecessorIndex) {
          inputs[predecessor_permutation_[i]] = Map(node->input(i));
        }
      }
      SetMap(node, __ Phi(base::VectorOf(inputs), rep));
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Call* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Object> function = Map(node->function());
    V<Context> context = Map(node->context());

    Builtin builtin;
    switch (node->target_type()) {
      case maglev::Call::TargetType::kAny:
        switch (node->receiver_mode()) {
          case ConvertReceiverMode::kNullOrUndefined:
            builtin = Builtin::kCall_ReceiverIsNullOrUndefined;
            break;
          case ConvertReceiverMode::kNotNullOrUndefined:
            builtin = Builtin::kCall_ReceiverIsNotNullOrUndefined;
            break;
          case ConvertReceiverMode::kAny:
            builtin = Builtin::kCall_ReceiverIsAny;
            break;
        }
        break;
      case maglev::Call::TargetType::kJSFunction:
        switch (node->receiver_mode()) {
          case ConvertReceiverMode::kNullOrUndefined:
            builtin = Builtin::kCallFunction_ReceiverIsNullOrUndefined;
            break;
          case ConvertReceiverMode::kNotNullOrUndefined:
            builtin = Builtin::kCallFunction_ReceiverIsNotNullOrUndefined;
            break;
          case ConvertReceiverMode::kAny:
            builtin = Builtin::kCallFunction_ReceiverIsAny;
            break;
        }
        break;
    }

    base::SmallVector<OpIndex, 16> arguments;
    arguments.push_back(function);
    arguments.push_back(__ Word32Constant(node->num_args()));
    for (auto arg : node->args()) {
      arguments.push_back(Map(arg));
    }
    arguments.push_back(context);

    SetMap(node,
           GenerateBuiltinCall(node, builtin, frame_state,
                               base::VectorOf(arguments), node->num_args()));

    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CallKnownJSFunction* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Object> callee = Map(node->closure());
    int actual_parameter_count = JSParameterCount(node->num_args());

    if (node->shared_function_info().HasBuiltinId()) {
      base::SmallVector<OpIndex, 16> arguments;
      arguments.push_back(callee);
      arguments.push_back(Map(node->new_target()));
      arguments.push_back(__ Word32Constant(actual_parameter_count));
      arguments.push_back(Map(node->receiver()));
      for (int i = 0; i < node->num_args(); i++) {
        arguments.push_back(Map(node->arg(i)));
      }
      // Setting missing arguments to Undefined.
      for (int i = actual_parameter_count; i < node->expected_parameter_count();
           i++) {
        arguments.push_back(__ HeapConstant(local_factory_->undefined_value()));
      }
      arguments.push_back(Map(node->context()));
      SetMap(node, GenerateBuiltinCall(
                       node, node->shared_function_info().builtin_id(),
                       frame_state, base::VectorOf(arguments),
                       std::max<int>(actual_parameter_count,
                                     node->expected_parameter_count())));
    } else {
      ThrowingScope throwing_scope(this, node);
      base::SmallVector<OpIndex, 16> arguments;
      arguments.push_back(Map(node->receiver()));
      for (int i = 0; i < node->num_args(); i++) {
        arguments.push_back(Map(node->arg(i)));
      }
      // Setting missing arguments to Undefined.
      for (int i = actual_parameter_count; i < node->expected_parameter_count();
           i++) {
        arguments.push_back(__ HeapConstant(local_factory_->undefined_value()));
      }
      arguments.push_back(Map(node->new_target()));
      arguments.push_back(__ Word32Constant(actual_parameter_count));

      // Load the context from {callee}.
      OpIndex context =
          __ LoadField(callee, AccessBuilder::ForJSFunctionContext());
      arguments.push_back(context);

      const CallDescriptor* descriptor = Linkage::GetJSCallDescriptor(
          graph_zone(), false,
          std::max<int>(actual_parameter_count,
                        node->expected_parameter_count()),
          CallDescriptor::kNeedsFrameState | CallDescriptor::kCanUseRoots);

      LazyDeoptOnThrow lazy_deopt_on_throw = ShouldLazyDeoptOnThrow(node);

      SetMap(node, __ Call(V<CallTarget>::Cast(callee), frame_state,
                           base::VectorOf(arguments),
                           TSCallDescriptor::Create(descriptor, CanThrow::kYes,
                                                    lazy_deopt_on_throw,
                                                    graph_zone())));
    }

    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CallKnownApiFunction* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    if (node->inline_builtin()) {
      DCHECK(v8_flags.maglev_inline_api_calls);
      // TODO(dmercadier, 40912714, 42203760): The flag maglev_inline_api_calls
      // is currently experimental, and it's not clear at this point if it will
      // even become non-experimental, so we currently don't support it in the
      // Maglev->Turboshaft translation. Note that a quick-fix would be to treat
      // kNoProfilingInlined like kNoProfiling, although this would be slower
      // than desired.
      UNIMPLEMENTED();
    }

    OpIndex api_holder;
    if (node->api_holder().has_value()) {
      api_holder = __ HeapConstant(node->api_holder().value().object());
    } else {
      api_holder = Map(node->receiver());
    }

    V<Object> target =
        __ HeapConstant(node->function_template_info().AsHeapObject().object());

    ApiFunction function(node->function_template_info().callback(broker_));
    ExternalReference function_ref = ExternalReference::Create(
        &function, ExternalReference::DIRECT_API_CALL);

    base::SmallVector<OpIndex, 16> arguments;
    arguments.push_back(__ ExternalConstant(function_ref));
    arguments.push_back(__ Word32Constant(node->num_args()));
    arguments.push_back(target);
    arguments.push_back(api_holder);
    arguments.push_back(Map(node->receiver()));
    for (maglev::Input arg : node->args()) {
      arguments.push_back(Map(arg));
    }
    arguments.push_back(Map(node->context()));

    Builtin builtin;
    switch (node->mode()) {
      case maglev::CallKnownApiFunction::Mode::kNoProfiling:
        builtin = Builtin::kCallApiCallbackOptimizedNoProfiling;
        break;
      case maglev::CallKnownApiFunction::Mode::kNoProfilingInlined:
        // Handled earlier when checking `node->inline_builtin()`.
        UNREACHABLE();
      case maglev::CallKnownApiFunction::Mode::kGeneric:
        builtin = Builtin::kCallApiCallbackOptimized;
        break;
    }

    int stack_arg_count = node->num_args() + /* implicit receiver */ 1;
    V<Any> result = GenerateBuiltinCall(
        node, builtin, frame_state, base::VectorOf(arguments), stack_arg_count);
    SetMap(node, result);

    return maglev::ProcessResult::kContinue;
  }
  V<Any> GenerateBuiltinCall(
      maglev::NodeBase* node, Builtin builtin,
      OptionalV<FrameState> frame_state, base::Vector<const OpIndex> arguments,
      base::Optional<int> stack_arg_count = base::nullopt) {
    ThrowingScope throwing_scope(this, node);

    Callable callable = Builtins::CallableFor(isolate_, builtin);
    const CallInterfaceDescriptor& descriptor = callable.descriptor();
    CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
        graph_zone(), descriptor,
        stack_arg_count.has_value() ? stack_arg_count.value()
                                    : descriptor.GetStackParameterCount(),
        frame_state.valid() ? CallDescriptor::kNeedsFrameState
                            : CallDescriptor::kNoFlags);
    V<Code> stub_code = __ HeapConstant(callable.code());

    LazyDeoptOnThrow lazy_deopt_on_throw = ShouldLazyDeoptOnThrow(node);

    return __ Call(stub_code, frame_state, base::VectorOf(arguments),
                   TSCallDescriptor::Create(call_descriptor, CanThrow::kYes,
                                            lazy_deopt_on_throw, graph_zone()));
  }
  maglev::ProcessResult Process(maglev::CallBuiltin* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    base::SmallVector<OpIndex, 16> arguments;
    for (int i = 0; i < node->InputCountWithoutContext(); i++) {
      arguments.push_back(Map(node->input(i)));
    }

    if (node->has_feedback()) {
      arguments.push_back(__ TaggedIndexConstant(node->feedback().index()));
      arguments.push_back(__ HeapConstant(node->feedback().vector));
    }

    if (Builtins::CallInterfaceDescriptorFor(node->builtin())
            .HasContextParameter()) {
      arguments.push_back(Map(node->context_input()));
    }

    V<Any> call_idx = GenerateBuiltinCall(node, node->builtin(), frame_state,
                                          base::VectorOf(arguments));
    const Operation& call_op = __ output_graph().Get(call_idx);
    if (const TupleOp* tuple = call_op.TryCast<TupleOp>()) {
      // If the builtin call returned multiple values, then in Maglev, {node} is
      // used as the 1st returned value, and a GetSecondReturnedValue node is
      // used to access the 2nd value. We thus call `SetMap` with the 1st
      // projection of the call, and record the 2nd projection in
      // {second_return_value_}, which we'll use when translating
      // GetSecondReturnedValue.
      DCHECK_EQ(tuple->input_count, 2);
      SetMap(node, tuple->input(0));
      second_return_value_ = tuple->input<Object>(1);
    } else {
      SetMap(node, call_idx);
    }

    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CallRuntime* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    auto c_entry_stub = __ CEntryStubConstant(isolate_, node->ReturnCount());

    CallDescriptor* call_descriptor = Linkage::GetRuntimeCallDescriptor(
        graph_zone(), node->function_id(), node->num_args(),
        Operator::kNoProperties, CallDescriptor::kNeedsFrameState);

    base::SmallVector<OpIndex, 16> arguments;
    for (int i = 0; i < node->num_args(); i++) {
      arguments.push_back(Map(node->arg(i)));
    }

    arguments.push_back(
        __ ExternalConstant(ExternalReference::Create(node->function_id())));
    arguments.push_back(__ Word32Constant(node->num_args()));

    arguments.push_back(Map(node->context()));

    OptionalV<FrameState> frame_state = OptionalV<FrameState>::Nullopt();
    if (call_descriptor->NeedsFrameState()) {
      frame_state = BuildFrameState(node->lazy_deopt_info());
    }

    LazyDeoptOnThrow lazy_deopt_on_throw = ShouldLazyDeoptOnThrow(node);

    SetMap(node, __ Call(c_entry_stub, frame_state, base::VectorOf(arguments),
                         TSCallDescriptor::Create(
                             call_descriptor, CanThrow::kYes,
                             lazy_deopt_on_throw, graph_zone())));

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ThrowReferenceErrorIfHole* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    IF (UNLIKELY(RootEqual(node->value(), RootIndex::kTheHoleValue))) {
      V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
      __ CallRuntime_ThrowAccessedUninitializedVariable(
          isolate_, frame_state, native_context(),
          __ HeapConstant(node->name().object()));
      // TODO(dmercadier): use RuntimeAbort here instead of Unreachable.
      // However, before doing so, RuntimeAbort should be changed so that 1)
      // it's a block terminator and 2) it doesn't call the runtime when
      // v8_flags.trap_on_abort is true.
      __ Unreachable();
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ThrowIfNotSuperConstructor* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<HeapObject> constructor = Map(node->constructor());
    V<i::Map> map = __ LoadMapField(constructor);
    static_assert(Map::kBitFieldOffsetEnd + 1 - Map::kBitFieldOffset == 1);
    V<Word32> bitfield =
        __ template LoadField<Word32>(map, AccessBuilder::ForMapBitField());
    IF_NOT (LIKELY(__ Word32BitwiseAnd(bitfield,
                                       Map::Bits1::IsConstructorBit::kMask))) {
      V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
      __ CallRuntime_ThrowNotSuperConstructor(isolate_, frame_state,
                                              native_context(), constructor,
                                              Map(node->function()));
      // TODO(dmercadier): use RuntimeAbort here instead of Unreachable.
      // However, before doing so, RuntimeAbort should be changed so that 1)
      // it's a block terminator and 2) it doesn't call the runtime when
      // v8_flags.trap_on_abort is true.
      __ Unreachable();
    }

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ThrowSuperAlreadyCalledIfNotHole* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    IF_NOT (LIKELY(__ RootEqual(Map(node->value()), RootIndex::kTheHoleValue,
                                isolate_))) {
      V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
      __ CallRuntime_ThrowSuperAlreadyCalledError(isolate_, frame_state,
                                                  native_context());
      // TODO(dmercadier): use RuntimeAbort here instead of Unreachable.
      // However, before doing so, RuntimeAbort should be changed so that 1)
      // it's a block terminator and 2) it doesn't call the runtime when
      // v8_flags.trap_on_abort is true.
      __ Unreachable();
    }

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ThrowSuperNotCalledIfHole* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    IF (UNLIKELY(__ RootEqual(Map(node->value()), RootIndex::kTheHoleValue,
                              isolate_))) {
      V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
      __ CallRuntime_ThrowSuperNotCalled(isolate_, frame_state,
                                         native_context());
      // TODO(dmercadier): use RuntimeAbort here instead of Unreachable.
      // However, before doing so, RuntimeAbort should be changed so that 1)
      // it's a block terminator and 2) it doesn't call the runtime when
      // v8_flags.trap_on_abort is true.
      __ Unreachable();
    }

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ThrowIfNotCallable* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<Object> value = Map(node->value());

    IF_NOT (LIKELY(__ ObjectIsCallable(value))) {
      V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
      __ CallRuntime_ThrowCalledNonCallable(isolate_, frame_state,
                                            native_context(), value);
      // TODO(dmercadier): use RuntimeAbort here instead of Unreachable.
      // However, before doing so, RuntimeAbort should be changed so that 1)
      // it's a block terminator and 2) it doesn't call the runtime when
      // v8_flags.trap_on_abort is true.
      __ Unreachable();
    }

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CreateFunctionContext* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Context> context = Map(node->context());
    V<ScopeInfo> scope_info = __ HeapConstant(node->scope_info().object());
    if (node->scope_type() == FUNCTION_SCOPE) {
      SetMap(node, __ CallBuiltin_FastNewFunctionContextFunction(
                       isolate_, frame_state, context, scope_info,
                       node->slot_count()));
    } else {
      DCHECK_EQ(node->scope_type(), EVAL_SCOPE);
      SetMap(node, __ CallBuiltin_FastNewFunctionContextEval(
                       isolate_, frame_state, context, scope_info,
                       node->slot_count()));
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::FastCreateClosure* node,
                                const maglev::ProcessingState& state) {
    DCHECK(!node->properties().can_throw());

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Context> context = Map(node->context());
    V<SharedFunctionInfo> shared_function_info =
        __ HeapConstant(node->shared_function_info().object());
    V<FeedbackCell> feedback_cell =
        __ HeapConstant(node->feedback_cell().object());

    SetMap(node,
           __ CallBuiltin_FastNewClosure(isolate_, frame_state, context,
                                         shared_function_info, feedback_cell));

    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CreateClosure* node,
                                const maglev::ProcessingState& state) {
    DCHECK(!node->properties().can_throw());

    V<Context> context = Map(node->context());
    V<SharedFunctionInfo> shared_function_info =
        __ HeapConstant(node->shared_function_info().object());
    V<FeedbackCell> feedback_cell =
        __ HeapConstant(node->feedback_cell().object());

    V<JSFunction> closure;
    if (node->pretenured()) {
      closure = __ CallRuntime_NewClosure_Tenured(
          isolate_, context, shared_function_info, feedback_cell);
    } else {
      closure = __ CallRuntime_NewClosure(isolate_, context,
                                          shared_function_info, feedback_cell);
    }

    SetMap(node, closure);

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CallWithArrayLike* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Context> context = Map(node->context());
    V<Object> function = Map(node->function());
    V<Object> receiver = Map(node->receiver());
    V<Object> arguments_list = Map(node->arguments_list());

    SetMap(node, __ CallBuiltin_CallWithArrayLike(
                     isolate_, graph_zone(), frame_state, context, receiver,
                     function, arguments_list));

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CallWithSpread* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Context> context = Map(node->context());
    V<Object> function = Map(node->function());
    V<Object> spread = Map(node->spread());

    base::SmallVector<V<Object>, 16> arguments_no_spread;
    for (auto arg : node->args_no_spread()) {
      arguments_no_spread.push_back(Map(arg));
    }

    SetMap(node, __ CallBuiltin_CallWithSpread(
                     isolate_, graph_zone(), frame_state, context, function,
                     node->num_args_no_spread(), spread,
                     base::VectorOf(arguments_no_spread)));

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CallForwardVarargs* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<JSFunction> function = Map(node->function());
    V<Context> context = Map(node->context());

    base::SmallVector<V<Object>, 16> arguments;
    for (auto arg : node->args()) {
      arguments.push_back(Map(arg));
    }
    DCHECK_EQ(node->num_args(), arguments.size());

    V<Object> call;
    switch (node->target_type()) {
      case maglev::Call::TargetType::kJSFunction:
        call = __ CallBuiltin_CallFunctionForwardVarargs(
            isolate_, graph_zone(), frame_state, context, function,
            node->num_args(), node->start_index(), base::VectorOf(arguments));
        break;
      case maglev::Call::TargetType::kAny:
        UNIMPLEMENTED();
    }

    SetMap(node, call);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Construct* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    base::SmallVector<OpIndex, 16> arguments;

    arguments.push_back(Map(node->function()));
    arguments.push_back(Map(node->new_target()));
    arguments.push_back(__ Word32Constant(node->num_args()));

    for (auto arg : node->args()) {
      arguments.push_back(Map(arg));
    }

    arguments.push_back(Map(node->context()));

    SetMap(node,
           GenerateBuiltinCall(node, Builtin::kConstruct, frame_state,
                               base::VectorOf(arguments), node->num_args()));

    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ConstructWithSpread* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    base::SmallVector<OpIndex, 16> arguments;
    arguments.push_back(Map(node->function()));
    arguments.push_back(Map(node->new_target()));
    arguments.push_back(__ Word32Constant(node->num_args_no_spread()));
    arguments.push_back(Map(node->spread()));

    for (auto arg : node->args_no_spread()) {
      arguments.push_back(Map(arg));
    }

    arguments.push_back(Map(node->context()));

    SetMap(node, GenerateBuiltinCall(node, Builtin::kConstructWithSpread,
                                     frame_state, base::VectorOf(arguments),
                                     node->num_args_no_spread()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckConstructResult* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ CheckConstructResult(Map(node->construct_result_input()),
                                         Map(node->implicit_receiver_input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckDerivedConstructResult* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);
    V<Object> construct_result = Map(node->construct_result_input());
    __ CheckDerivedConstructResult(construct_result,
                                   BuildFrameState(node->lazy_deopt_info()),
                                   native_context());
    SetMap(node, construct_result);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::SetKeyedGeneric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->object_input()),
                           Map(node->key_input()),
                           Map(node->value_input()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kKeyedStoreIC, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::GetKeyedGeneric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->object_input()), Map(node->key_input()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kKeyedLoadIC, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::SetNamedGeneric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->object_input()),
                           __ HeapConstant(node->name().object()),
                           Map(node->value_input()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kStoreIC, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadNamedGeneric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {
        Map(node->object_input()), __ HeapConstant(node->name().object()),
        __ TaggedIndexConstant(node->feedback().index()),
        __ HeapConstant(node->feedback().vector), Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kLoadIC, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LoadNamedFromSuperGeneric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->receiver()),
                           Map(node->lookup_start_object()),
                           __ HeapConstant(node->name().object()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kLoadSuperIC, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LoadGlobal* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {__ HeapConstant(node->name().object()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    Builtin builtin;
    switch (node->typeof_mode()) {
      case TypeofMode::kInside:
        builtin = Builtin::kLoadGlobalICInsideTypeof;
        break;
      case TypeofMode::kNotInside:
        builtin = Builtin::kLoadGlobalIC;
        break;
    }

    SetMap(node, GenerateBuiltinCall(node, builtin, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::StoreGlobal* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {
        __ HeapConstant(node->name().object()), Map(node->value()),
        __ TaggedIndexConstant(node->feedback().index()),
        __ HeapConstant(node->feedback().vector), Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kStoreGlobalIC, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::DefineKeyedOwnGeneric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->object_input()),
                           Map(node->key_input()),
                           Map(node->value_input()),
                           Map(node->flags_input()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kDefineKeyedOwnIC,
                                     frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::DefineNamedOwnGeneric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->object_input()),
                           __ HeapConstant(node->name().object()),
                           Map(node->value_input()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kDefineNamedOwnIC,
                                     frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::GetIterator* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {
        Map(node->receiver()), __ TaggedIndexConstant(node->load_slot()),
        __ TaggedIndexConstant(node->call_slot()),
        __ HeapConstant(node->feedback()), Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kGetIteratorWithFeedback,
                                     frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CreateShallowObjectLiteral* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {
        __ HeapConstant(node->feedback().vector),
        __ TaggedIndexConstant(node->feedback().index()),
        __ HeapConstant(node->boilerplate_descriptor().object()),
        __ SmiConstant(Smi::FromInt(node->flags())), native_context()};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kCreateShallowObjectLiteral,
                                     frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::TestInstanceOf* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->object()), Map(node->callable()),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kInstanceOf, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::DeleteProperty* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {
        Map(node->object()), Map(node->key()),
        __ SmiConstant(Smi::FromInt(static_cast<int>(node->mode()))),
        Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kDeleteProperty,
                                     frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ToName* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {Map(node->value_input()), Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kToName, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CreateRegExpLiteral* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {__ HeapConstant(node->feedback().vector),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->pattern().object()),
                           __ SmiConstant(Smi::FromInt(node->flags())),
                           native_context()};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kCreateRegExpLiteral,
                                     frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CreateObjectLiteral* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {
        __ HeapConstant(node->feedback().vector),
        __ TaggedIndexConstant(node->feedback().index()),
        __ HeapConstant(node->boilerplate_descriptor().object()),
        __ SmiConstant(Smi::FromInt(node->flags())), native_context()};

    SetMap(node,
           GenerateBuiltinCall(node, Builtin::kCreateObjectFromSlowBoilerplate,
                               frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CreateArrayLiteral* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {__ HeapConstant(node->feedback().vector),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->constant_elements().object()),
                           __ SmiConstant(Smi::FromInt(node->flags())),
                           native_context()};

    SetMap(node,
           GenerateBuiltinCall(node, Builtin::kCreateArrayFromSlowBoilerplate,
                               frame_state, base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ForInPrepare* node,
                                const maglev::ProcessingState& state) {
    OpIndex arguments[] = {Map(node->enumerator()),
                           __ TaggedIndexConstant(node->feedback().index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    V<Any> call =
        GenerateBuiltinCall(node, Builtin::kForInPrepare,
                            OptionalV<turboshaft::FrameState>::Nullopt(),
                            base::VectorOf(arguments));
    SetMap(node, __ Projection(call, 0, RegisterRepresentation::Tagged()));
    second_return_value_ = V<Object>::Cast(
        __ Projection(call, 1, RegisterRepresentation::Tagged()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ForInNext* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());

    OpIndex arguments[] = {__ TaggedIndexConstant(node->feedback().index()),
                           Map(node->receiver()),
                           Map(node->cache_array()),
                           Map(node->cache_type()),
                           Map(node->cache_index()),
                           __ HeapConstant(node->feedback().vector),
                           Map(node->context())};

    SetMap(node, GenerateBuiltinCall(node, Builtin::kForInNext, frame_state,
                                     base::VectorOf(arguments)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckSmi* node,
                                const maglev::ProcessingState& state) {
    __ DeoptimizeIfNot(__ ObjectIsSmi(Map(node->receiver_input())),
                       BuildFrameState(node->eager_deopt_info()),
                       DeoptimizeReason::kNotASmi,
                       node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckInt32IsSmi* node,
                                const maglev::ProcessingState& state) {
    DeoptIfInt32IsNotSmi(node->input(), node);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckNumber* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    V<Object> input = Map(node->receiver_input());
    V<Word32> check;
    if (node->mode() == Object::Conversion::kToNumeric) {
      check = __ ObjectIsNumberOrBigInt(input);
    } else {
      DCHECK_EQ(node->mode(), Object::Conversion::kToNumber);
      check = __ ObjectIsNumber(input);
    }
    __ DeoptimizeIfNot(check, frame_state, DeoptimizeReason::kNotANumber,
                       node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckHeapObject* node,
                                const maglev::ProcessingState& state) {
    __ DeoptimizeIf(__ ObjectIsSmi(Map(node->receiver_input())),
                    BuildFrameState(node->eager_deopt_info()),
                    DeoptimizeReason::kSmi,
                    node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  void CheckMaps(V<Object> receiver_input, V<FrameState> frame_state,
                 const FeedbackSource& feedback,
                 const compiler::ZoneRefSet<Map>& maps, bool check_heap_object,
                 bool try_migrate) {
    Label<> done(this);
    if (check_heap_object) {
      OpIndex is_smi = __ IsSmi(receiver_input);
      if (AnyMapIsHeapNumber(maps)) {
        // Smis count as matching the HeapNumber map, so we're done.
        GOTO_IF(is_smi, done);
      } else {
        __ DeoptimizeIf(is_smi, frame_state, DeoptimizeReason::kWrongMap,
                        feedback);
      }
    }

    bool has_migration_targets = false;
    if (try_migrate) {
      for (MapRef map : maps) {
        if (map.object()->is_migration_target()) {
          has_migration_targets = true;
          break;
        }
      }
    }

    __ CheckMaps(V<HeapObject>::Cast(receiver_input), frame_state, maps,
                 has_migration_targets ? CheckMapsFlag::kTryMigrateInstance
                                       : CheckMapsFlag::kNone,
                 feedback);

    if (done.has_incoming_jump()) {
      GOTO(done);
      BIND(done);
    }
  }
  maglev::ProcessResult Process(maglev::CheckMaps* node,
                                const maglev::ProcessingState& state) {
    CheckMaps(Map(node->receiver_input()),
              BuildFrameState(node->eager_deopt_info()),
              node->eager_deopt_info()->feedback_to_update(), node->maps(),
              node->check_type() == maglev::CheckType::kCheckHeapObject,
              /* try_migrate */ false);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckMapsWithMigration* node,
                                const maglev::ProcessingState& state) {
    CheckMaps(Map(node->receiver_input()),
              BuildFrameState(node->eager_deopt_info()),
              node->eager_deopt_info()->feedback_to_update(), node->maps(),
              node->check_type() == maglev::CheckType::kCheckHeapObject,
              /* try_migrate */ true);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckValue* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ DeoptimizeIfNot(__ TaggedEqual(Map(node->target_input()),
                                      __ HeapConstant(node->value().object())),
                       frame_state, DeoptimizeReason::kWrongValue,
                       node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckValueEqualsInt32* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ DeoptimizeIfNot(__ Word32Equal(Map(node->target_input()), node->value()),
                       frame_state, DeoptimizeReason::kWrongValue,
                       node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckValueEqualsFloat64* node,
                                const maglev::ProcessingState& state) {
    DCHECK(!std::isnan(node->value()));
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ DeoptimizeIfNot(
        __ Float64Equal(Map(node->target_input()), node->value()), frame_state,
        DeoptimizeReason::kWrongValue,
        node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckString* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    ObjectIsOp::InputAssumptions input_assumptions =
        node->check_type() == maglev::CheckType::kCheckHeapObject
            ? ObjectIsOp::InputAssumptions::kNone
            : ObjectIsOp::InputAssumptions::kHeapObject;
    V<Word32> check = __ ObjectIs(Map(node->receiver_input()),
                                  ObjectIsOp::Kind::kString, input_assumptions);
    __ DeoptimizeIfNot(check, frame_state, DeoptimizeReason::kNotAString,
                       node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckSymbol* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    ObjectIsOp::InputAssumptions input_assumptions =
        node->check_type() == maglev::CheckType::kCheckHeapObject
            ? ObjectIsOp::InputAssumptions::kNone
            : ObjectIsOp::InputAssumptions::kHeapObject;
    V<Word32> check = __ ObjectIs(Map(node->receiver_input()),
                                  ObjectIsOp::Kind::kSymbol, input_assumptions);
    __ DeoptimizeIfNot(check, frame_state, DeoptimizeReason::kNotASymbol,
                       node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckInstanceType* node,
                                const maglev::ProcessingState& state) {
    __ CheckInstanceType(
        Map(node->receiver_input()), BuildFrameState(node->eager_deopt_info()),
        node->eager_deopt_info()->feedback_to_update(),
        node->first_instance_type(), node->last_instance_type(),
        node->check_type() != maglev::CheckType::kOmitHeapObjectCheck);

    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckDynamicValue* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ DeoptimizeIfNot(
        __ TaggedEqual(Map(node->first_input()), Map(node->second_input())),
        frame_state, DeoptimizeReason::kWrongValue,
        node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedSmiSizedInt32* node,
                                const maglev::ProcessingState& state) {
    DeoptIfInt32IsNotSmi(node->input(), node);
    SetMap(node, Map(node->input()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckNotHole* node,
                                const maglev::ProcessingState& state) {
    __ DeoptimizeIf(RootEqual(node->object_input(), RootIndex::kTheHoleValue),
                    BuildFrameState(node->eager_deopt_info()),
                    DeoptimizeReason::kHole,
                    node->eager_deopt_info()->feedback_to_update());
    SetMap(node, Map(node->object_input()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckConstTrackingLetCellTagged* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ CheckConstTrackingLetCellTagged(
        Map(node->context_input()), Map(node->value_input()), node->index(),
        frame_state, node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckConstTrackingLetCell* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ CheckConstTrackingLetCell(
        Map(node->context_input()), node->index(), frame_state,
        node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckInt32Condition* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    bool negate_result = false;
    V<Word32> cmp = ConvertInt32Compare(node->left_input(), node->right_input(),
                                        node->condition(), &negate_result);
    if (negate_result) {
      __ DeoptimizeIf(cmp, frame_state, node->reason(),
                      node->eager_deopt_info()->feedback_to_update());
    } else {
      __ DeoptimizeIfNot(cmp, frame_state, node->reason(),
                         node->eager_deopt_info()->feedback_to_update());
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::AllocationBlock* node,
                                const maglev::ProcessingState& state) {
    if (!node->is_used()) return maglev::ProcessResult::kRemove;
    int size = 0;
    for (auto alloc : node->allocation_list()) {
      if (!alloc->HasBeenAnalysed() || alloc->HasEscaped()) {
        alloc->set_offset(size);
        size += alloc->size();
      }
    }
    node->set_size(size);
    SetMap(node, __ FinishInitialization(
                     __ Allocate<HeapObject>(size, node->allocation_type())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::InlinedAllocation* node,
                                const maglev::ProcessingState& state) {
    if (node->HasBeenAnalysed() && node->HasBeenElided()) {
      return maglev::ProcessResult::kRemove;
    }
    V<HeapObject> alloc = Map(node->allocation_block());
    SetMap(node, __ BitcastWordPtrToHeapObject(__ WordPtrAdd(
                     __ BitcastHeapObjectToWordPtr(alloc), node->offset())));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::EnsureWritableFastElements* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ EnsureWritableFastElements(Map(node->object_input()),
                                               Map(node->elements_input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::MaybeGrowFastElements* node,
                                const maglev::ProcessingState& state) {
    GrowFastElementsMode mode =
        IsDoubleElementsKind(node->elements_kind())
            ? GrowFastElementsMode::kDoubleElements
            : GrowFastElementsMode::kSmiOrObjectElements;
    SetMap(node,
           __ MaybeGrowFastElements(
               Map(node->object_input()), Map(node->elements_input()),
               Map(node->index_input()), Map(node->elements_length_input()),
               BuildFrameState(node->eager_deopt_info()), mode,
               node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::TransitionElementsKindOrCheckMap* node,
                                const maglev::ProcessingState& state) {
    bool check_heap_object;
    switch (node->check_type()) {
      case maglev::CheckType::kCheckHeapObject:
        check_heap_object = true;
        break;
      case maglev::CheckType::kOmitHeapObjectCheck:
        check_heap_object = false;
        break;
    }
    __ TransitionElementsKindOrCheckMap(
        Map(node->object_input()), BuildFrameState(node->eager_deopt_info()),
        check_heap_object, node->transition_sources(),
        node->transition_target(),
        node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TransitionElementsKind* node,
                                const maglev::ProcessingState& state) {
    __ TransitionMultipleElementsKind(Map(node->object_input()),
                                      node->transition_sources(),
                                      node->transition_target());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::HasInPrototypeChain* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    SetMap(node, __ HasInPrototypeChain(Map(node->object()), node->prototype(),
                                        frame_state, native_context()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::UpdateJSArrayLength* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ UpdateJSArrayLength(Map(node->length_input()),
                                        Map(node->object_input()),
                                        Map(node->index_input())));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::AllocateElementsArray* node,
                                const maglev::ProcessingState& state) {
    V<Word32> length = Map(node->length_input());
    // Note that {length} cannot be negative (Maglev inserts a check before
    // AllocateElementsArray to ensure this).
    __ DeoptimizeIfNot(
        __ Uint32LessThan(length, JSArray::kInitialMaxFastElementArray),
        BuildFrameState(node->eager_deopt_info()),
        DeoptimizeReason::kGreaterThanMaxFastElementArray,
        node->eager_deopt_info()->feedback_to_update());

    SetMap(node,
           __ NewArray(__ ChangeUint32ToUintPtr(length),
                       NewArrayOp::Kind::kObject, node->allocation_type()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::StringConcat* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ StringConcat(Map(node->lhs()), Map(node->rhs())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StringEqual* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ StringEqual(Map(node->lhs()), Map(node->rhs())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StringLength* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ StringLength(Map(node->object_input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StringAt* node,
                                const maglev::ProcessingState& state) {
    V<Word32> char_code =
        __ StringCharCodeAt(Map(node->string_input()),
                            __ ChangeUint32ToUintPtr(Map(node->index_input())));
    SetMap(node, __ ConvertCharCodeToString(char_code));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedInternalizedString* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    SetMap(node, __ CheckedInternalizedString(
                     Map(node->object_input()), frame_state,
                     node->check_type() == maglev::CheckType::kCheckHeapObject,
                     node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckValueEqualsString* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ CheckValueEqualsString(Map(node->target_input()), node->value(),
                              frame_state,
                              node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BuiltinStringFromCharCode* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ConvertCharCodeToString(Map(node->code_input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(
      maglev::BuiltinStringPrototypeCharCodeOrCodePointAt* node,
      const maglev::ProcessingState& state) {
    if (node->mode() == maglev::BuiltinStringPrototypeCharCodeOrCodePointAt::
                            Mode::kCharCodeAt) {
      SetMap(node, __ StringCharCodeAt(
                       Map(node->string_input()),
                       __ ChangeUint32ToUintPtr(Map(node->index_input()))));
    } else {
      DCHECK_EQ(node->mode(),
                maglev::BuiltinStringPrototypeCharCodeOrCodePointAt::Mode::
                    kCodePointAt);
      SetMap(node, __ StringCodePointAt(
                       Map(node->string_input()),
                       __ ChangeUint32ToUintPtr(Map(node->index_input()))));
    }
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ToString* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    Label<String> done(this);

    V<Object> value = Map(node->value_input());
    GOTO_IF(__ ObjectIsString(value), done, V<String>::Cast(value));

    IF_NOT (__ IsSmi(value)) {
      if (node->mode() == maglev::ToString::ConversionMode::kConvertSymbol) {
        V<i::Map> map = __ LoadMapField(value);
        V<Word32> instance_type = __ LoadInstanceTypeField(map);
        IF (__ Word32Equal(instance_type, SYMBOL_TYPE)) {
          GOTO(done,
               __ CallRuntime_SymbolDescriptiveString(
                   isolate_, Map(node->context()), V<Symbol>::Cast(value)));
        }
      }
    }

    GOTO(done, __ CallBuiltin_ToString(isolate_,
                                       BuildFrameState(node->lazy_deopt_info()),
                                       Map(node->context()), value));

    BIND(done, result);
    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::NumberToString* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ CallBuiltin_NumberToString(isolate_, Map(node->value_input())));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ArgumentsLength* node,
                                const maglev::ProcessingState& state) {
    // TODO(dmercadier): ArgumentsLength in Maglev returns a raw Word32, while
    // in Turboshaft, it returns a Smi. We thus untag this Smi here to match
    // Maglev's behavior, but it would be more efficient to change Turboshaft's
    // ArgumentsLength operation to return a raw Word32 as well.
    SetMap(node, __ UntagSmi(__ ArgumentsLength()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ArgumentsElements* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ NewArgumentsElements(Map(node->arguments_count_input()),
                                         node->type(),
                                         node->formal_parameter_count()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::RestLength* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ RestLength(node->formal_parameter_count()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LoadTaggedField* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ LoadTaggedField(Map(node->object_input()), node->offset()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadDoubleField* node,
                                const maglev::ProcessingState& state) {
    V<HeapNumber> field = __ LoadTaggedField<HeapNumber>(
        Map(node->object_input()), node->offset());
    SetMap(node, __ LoadField(field, AccessBuilder::ForHeapNumberValue()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadFixedArrayElement* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ LoadFixedArrayElement(
                     Map(node->elements_input()),
                     __ ChangeInt32ToIntPtr(Map(node->index_input()))));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadFixedDoubleArrayElement* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ LoadFixedDoubleArrayElement(
                     Map(node->elements_input()),
                     __ ChangeInt32ToIntPtr(Map(node->index_input()))));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadHoleyFixedDoubleArrayElement* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ LoadFixedDoubleArrayElement(
                     Map(node->elements_input()),
                     __ ChangeInt32ToIntPtr(Map(node->index_input()))));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(
      maglev::LoadHoleyFixedDoubleArrayElementCheckedNotHole* node,
      const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    V<Float64> result = __ LoadFixedDoubleArrayElement(
        Map(node->elements_input()),
        __ ChangeInt32ToIntPtr(Map(node->index_input())));
    __ DeoptimizeIf(__ Float64IsHole(result), frame_state,
                    DeoptimizeReason::kHole,
                    node->eager_deopt_info()->feedback_to_update());
    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::StoreTaggedFieldNoWriteBarrier* node,
                                const maglev::ProcessingState& state) {
    __ Store(Map(node->object_input()), Map(node->value_input()),
             StoreOp::Kind::TaggedBase(), MemoryRepresentation::AnyTagged(),
             WriteBarrierKind::kNoWriteBarrier, node->offset());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StoreTaggedFieldWithWriteBarrier* node,
                                const maglev::ProcessingState& state) {
    __ Store(Map(node->object_input()), Map(node->value_input()),
             StoreOp::Kind::TaggedBase(), MemoryRepresentation::AnyTagged(),
             WriteBarrierKind::kFullWriteBarrier, node->offset());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StoreDoubleField* node,
                                const maglev::ProcessingState& state) {
    V<HeapNumber> field = __ LoadTaggedField<HeapNumber>(
        Map(node->object_input()), node->offset());
    __ StoreField(field, AccessBuilder::ForHeapNumberValue(),
                  Map(node->value_input()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(
      maglev::StoreFixedArrayElementNoWriteBarrier* node,
      const maglev::ProcessingState& state) {
    __ StoreFixedArrayElement(Map(node->elements_input()),
                              __ ChangeInt32ToIntPtr(Map(node->index_input())),
                              Map(node->value_input()),
                              WriteBarrierKind::kNoWriteBarrier);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(
      maglev::StoreFixedArrayElementWithWriteBarrier* node,
      const maglev::ProcessingState& state) {
    __ StoreFixedArrayElement(Map(node->elements_input()),
                              __ ChangeInt32ToIntPtr(Map(node->index_input())),
                              Map(node->value_input()),
                              WriteBarrierKind::kFullWriteBarrier);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StoreFixedDoubleArrayElement* node,
                                const maglev::ProcessingState& state) {
    __ StoreFixedDoubleArrayElement(
        Map(node->elements_input()),
        __ ChangeInt32ToIntPtr(Map(node->index_input())),
        Map(node->value_input()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StoreMap* node,
                                const maglev::ProcessingState& state) {
    __ Store(Map(node->object_input()), __ HeapConstant(node->map().object()),
             StoreOp::Kind::TaggedBase(), MemoryRepresentation::TaggedPointer(),
             WriteBarrierKind::kMapWriteBarrier, HeapObject::kMapOffset);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StoreFloat64* node,
                                const maglev::ProcessingState& state) {
    __ Store(Map(node->object_input()), Map(node->value_input()),
             StoreOp::Kind::TaggedBase(), MemoryRepresentation::Float64(),
             WriteBarrierKind::kNoWriteBarrier, node->offset());
    return maglev::ProcessResult::kContinue;
  }

  // For-in specific operations.
  maglev::ProcessResult Process(maglev::LoadEnumCacheLength* node,
                                const maglev::ProcessingState& state) {
    V<Word32> bitfield3 =
        __ LoadField<Word32>(V<i::Map>::Cast(Map(node->map_input())),
                             AccessBuilder::ForMapBitField3());
    V<Word32> length = __ Word32ShiftRightLogical(
        __ Word32BitwiseAnd(bitfield3, Map::Bits3::EnumLengthBits::kMask),
        Map::Bits3::EnumLengthBits::kShift);
    SetMap(node, length);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckCacheIndicesNotCleared* node,
                                const maglev::ProcessingState& state) {
    // If the cache length is zero, we don't have any indices, so we know this
    // is ok even though the indices are the empty array.
    IF_NOT (__ Word32Equal(Map(node->length_input()), 0)) {
      // Otherwise, an empty array with non-zero required length is not valid.
      V<Word32> condition =
          RootEqual(node->indices_input(), RootIndex::kEmptyFixedArray);
      __ DeoptimizeIf(condition, BuildFrameState(node->eager_deopt_info()),
                      DeoptimizeReason::kWrongEnumIndices,
                      node->eager_deopt_info()->feedback_to_update());
    }
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadTaggedFieldByFieldIndex* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ LoadFieldByIndex(Map(node->object_input()),
                               __ UntagSmi(Map<Smi>(node->index_input()))));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LoadTypedArrayLength* node,
                                const maglev::ProcessingState& state) {
    // TODO(dmercadier): consider loading the raw length instead of the byte
    // length. This is not currently done because the raw length field might be
    // removed soon.
    V<WordPtr> length =
        __ LoadField<WordPtr>(Map<JSTypedArray>(node->receiver_input()),
                              AccessBuilder::ForJSTypedArrayByteLength());

    int element_size = ElementsKindSize(node->elements_kind());
    if (element_size > 1) {
      DCHECK(element_size == 2 || element_size == 4 || element_size == 8);
      length = __ WordPtrShiftRightLogical(
          length, base::bits::CountTrailingZeros(element_size));
    }
    SetMap(node, length);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckTypedArrayBounds* node,
                                const maglev::ProcessingState& state) {
    __ DeoptimizeIfNot(
        __ UintPtrLessThan(__ ChangeUint32ToUintPtr(Map(node->index_input())),
                           Map(node->length_input())),
        BuildFrameState(node->eager_deopt_info()),
        DeoptimizeReason::kOutOfBounds,
        node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LoadUnsignedIntTypedArrayElement* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, BuildTypedArrayLoad(Map<JSTypedArray>(node->object_input()),
                                     Map<Word32>(node->index_input()),
                                     node->elements_kind()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadSignedIntTypedArrayElement* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, BuildTypedArrayLoad(Map<JSTypedArray>(node->object_input()),
                                     Map<Word32>(node->index_input()),
                                     node->elements_kind()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadDoubleTypedArrayElement* node,
                                const maglev::ProcessingState& state) {
    DCHECK_EQ(node->elements_kind(),
              any_of(FLOAT32_ELEMENTS, FLOAT64_ELEMENTS));
    V<Float> value = V<Float>::Cast(BuildTypedArrayLoad(
        Map<JSTypedArray>(node->object_input()),
        Map<Word32>(node->index_input()), node->elements_kind()));
    if (node->elements_kind() == FLOAT32_ELEMENTS) {
      value = __ ChangeFloat32ToFloat64(V<Float32>::Cast(value));
    }
    SetMap(node, value);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::StoreIntTypedArrayElement* node,
                                const maglev::ProcessingState& state) {
    BuildTypedArrayStore(Map<JSTypedArray>(node->object_input()),
                         Map<Word32>(node->index_input()),
                         Map<Untagged>(node->value_input()),
                         node->elements_kind());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StoreDoubleTypedArrayElement* node,
                                const maglev::ProcessingState& state) {
    DCHECK_EQ(node->elements_kind(),
              any_of(FLOAT32_ELEMENTS, FLOAT64_ELEMENTS));
    V<Float> value = Map<Float>(node->value_input());
    if (node->elements_kind() == FLOAT32_ELEMENTS) {
      value = __ TruncateFloat64ToFloat32(Map(node->value_input()));
    }
    BuildTypedArrayStore(Map<JSTypedArray>(node->object_input()),
                         Map<Word32>(node->index_input()), value,
                         node->elements_kind());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckJSDataViewBounds* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    // Normal DataView (backed by AB / SAB) or non-length tracking backed by
    // GSAB.
    V<WordPtr> byte_length =
        __ LoadField<WordPtr>(Map<JSTypedArray>(node->receiver_input()),
                              AccessBuilder::ForJSDataViewByteLength());

    int element_size = ExternalArrayElementSize(node->element_type());
    if (element_size > 1) {
      // For element_size larger than 1, we need to make sure that {index} is
      // less than {byte_length}, but also that {index+element_size} is less
      // than {byte_length}. We do this by subtracting {element_size-1} from
      // {byte_length}: if the resulting length is greater than 0, then we can
      // just treat {element_size} as 1 and check if {index} is less than this
      // new {byte_length}.
      DCHECK(element_size == 2 || element_size == 4 || element_size == 8);
      byte_length = __ WordPtrSub(byte_length, element_size - 1);
      __ DeoptimizeIf(__ IntPtrLessThan(byte_length, 0), frame_state,
                      DeoptimizeReason::kOutOfBounds,
                      node->eager_deopt_info()->feedback_to_update());
    }
    __ DeoptimizeIfNot(
        __ Uint32LessThan(Map<Word32>(node->index_input()),
                          __ TruncateWordPtrToWord32(byte_length)),
        frame_state, DeoptimizeReason::kOutOfBounds,
        node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LoadSignedIntDataViewElement* node,
                                const maglev::ProcessingState& state) {
    V<JSDataView> data_view = Map<JSDataView>(node->object_input());
    V<WordPtr> storage = __ LoadField<WordPtr>(
        data_view, AccessBuilder::ForJSDataViewDataPointer());
    SetMap(node,
           __ LoadDataViewElement(
               data_view, storage,
               __ ChangeUint32ToUintPtr(Map<Word32>(node->index_input())),
               RootEqual(node->is_little_endian_input(), RootIndex::kTrueValue),
               node->type()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::LoadDoubleDataViewElement* node,
                                const maglev::ProcessingState& state) {
    V<JSDataView> data_view = Map<JSDataView>(node->object_input());
    V<WordPtr> storage = __ LoadField<WordPtr>(
        data_view, AccessBuilder::ForJSDataViewDataPointer());
    SetMap(node,
           __ LoadDataViewElement(
               data_view, storage,
               __ ChangeUint32ToUintPtr(Map<Word32>(node->index_input())),
               RootEqual(node->is_little_endian_input(), RootIndex::kTrueValue),
               ExternalArrayType::kExternalFloat64Array));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::StoreSignedIntDataViewElement* node,
                                const maglev::ProcessingState& state) {
    V<JSDataView> data_view = Map<JSDataView>(node->object_input());
    V<WordPtr> storage = __ LoadField<WordPtr>(
        data_view, AccessBuilder::ForJSDataViewDataPointer());
    __ StoreDataViewElement(
        data_view, storage,
        __ ChangeUint32ToUintPtr(Map<Word32>(node->index_input())),
        Map<Word32>(node->value_input()),
        RootEqual(node->is_little_endian_input(), RootIndex::kTrueValue),
        node->type());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::StoreDoubleDataViewElement* node,
                                const maglev::ProcessingState& state) {
    V<JSDataView> data_view = Map<JSDataView>(node->object_input());
    V<WordPtr> storage = __ LoadField<WordPtr>(
        data_view, AccessBuilder::ForJSDataViewDataPointer());
    __ StoreDataViewElement(
        data_view, storage,
        __ ChangeUint32ToUintPtr(Map<Word32>(node->index_input())),
        Map<Float64>(node->value_input()),
        RootEqual(node->is_little_endian_input(), RootIndex::kTrueValue),
        ExternalArrayType::kExternalFloat64Array);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckTypedArrayNotDetached* node,
                                const maglev::ProcessingState& state) {
    __ DeoptimizeIf(
        __ ArrayBufferIsDetached(Map<JSArrayBufferView>(node->object_input())),
        BuildFrameState(node->eager_deopt_info()),
        DeoptimizeReason::kArrayBufferWasDetached,
        node->eager_deopt_info()->feedback_to_update());

    return maglev::ProcessResult::kContinue;
  }

  void BuildJump(maglev::BasicBlock* target) {
    Block* destination = Map(target);
    if (target->is_loop() && target->predecessor_count() > 2) {
      // This loop has multiple forward edge in Maglev, so we'll create an extra
      // block in Turboshaft that will be the only predecessor.
      auto it = loop_single_edge_predecessors_.find(target);
      if (it != loop_single_edge_predecessors_.end()) {
        destination = it->second;
      } else {
        Block* loop_only_pred = __ NewBlock();
        loop_single_edge_predecessors_[target] = loop_only_pred;
        destination = loop_only_pred;
      }
    }
    __ Goto(destination);
  }
  maglev::ProcessResult Process(maglev::Jump* node,
                                const maglev::ProcessingState& state) {
    BuildJump(node->target());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckpointedJump* node,
                                const maglev::ProcessingState& state) {
    BuildJump(node->target());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::JumpLoop* node,
                                const maglev::ProcessingState& state) {
    __ Goto(Map(node->target()));
    FixLoopPhis(node->target());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Int32Compare* node,
                                const maglev::ProcessingState& state) {
    V<Word32> bool_res =
        ConvertCompare<Word32>(node->left_input(), node->right_input(),
                               node->operation(), Sign::kSigned);
    SetMap(node, ConvertWord32ToJSBool(bool_res));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64Compare* node,
                                const maglev::ProcessingState& state) {
    V<Word32> bool_res =
        ConvertCompare<Float64>(node->left_input(), node->right_input(),
                                node->operation(), Sign::kSigned);
    SetMap(node, ConvertWord32ToJSBool(bool_res));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TaggedEqual* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, ConvertWord32ToJSBool(
                     __ TaggedEqual(Map(node->lhs()), Map(node->rhs()))));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TestUndetectable* node,
                                const maglev::ProcessingState& state) {
    ObjectIsOp::InputAssumptions assumption;
    switch (node->check_type()) {
      case maglev::CheckType::kCheckHeapObject:
        assumption = ObjectIsOp::InputAssumptions::kNone;
        break;
      case maglev::CheckType::kOmitHeapObjectCheck:
        assumption = ObjectIsOp::InputAssumptions::kHeapObject;
        break;
    }
    SetMap(node, ConvertWord32ToJSBool(
                     __ ObjectIs(Map(node->value()),
                                 ObjectIsOp::Kind::kUndetectable, assumption)));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TestTypeOf* node,
                                const maglev::ProcessingState& state) {
    V<Object> input = Map(node->value());
    V<Boolean> result;
    switch (node->literal()) {
      case interpreter::TestTypeOfFlags::LiteralFlag::kNumber:
        result = ConvertWord32ToJSBool(
            __ ObjectIs(input, ObjectIsOp::Kind::kNumber,
                        ObjectIsOp::InputAssumptions::kNone));
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kString:
        result = ConvertWord32ToJSBool(
            __ ObjectIs(input, ObjectIsOp::Kind::kString,
                        ObjectIsOp::InputAssumptions::kNone));
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kSymbol:
        result = ConvertWord32ToJSBool(
            __ ObjectIs(input, ObjectIsOp::Kind::kSymbol,
                        ObjectIsOp::InputAssumptions::kNone));
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kBigInt:
        result = ConvertWord32ToJSBool(
            __ ObjectIs(input, ObjectIsOp::Kind::kBigInt,
                        ObjectIsOp::InputAssumptions::kNone));
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kFunction:
        result = ConvertWord32ToJSBool(
            __ ObjectIs(input, ObjectIsOp::Kind::kDetectableCallable,
                        ObjectIsOp::InputAssumptions::kNone));
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kBoolean:
        result = __ Select(__ RootEqual(input, RootIndex::kTrueValue, isolate_),
                           __ HeapConstant(local_factory_->true_value()),
                           __ HeapConstant(local_factory_->false_value()),
                           RegisterRepresentation::Tagged(), BranchHint::kNone,
                           SelectOp::Implementation::kBranch);
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kUndefined:
        result = __ Select(__ RootEqual(input, RootIndex::kNullValue, isolate_),
                           __ HeapConstant(local_factory_->false_value()),
                           ConvertWord32ToJSBool(__ ObjectIs(
                               input, ObjectIsOp::Kind::kUndetectable,
                               ObjectIsOp::InputAssumptions::kNone)),
                           RegisterRepresentation::Tagged(), BranchHint::kNone,
                           SelectOp::Implementation::kBranch);
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kObject:
        result = __ Select(__ ObjectIs(input, ObjectIsOp::Kind::kNonCallable,
                                       ObjectIsOp::InputAssumptions::kNone),
                           __ HeapConstant(local_factory_->true_value()),
                           ConvertWord32ToJSBool(__ RootEqual(
                               input, RootIndex::kNullValue, isolate_)),
                           RegisterRepresentation::Tagged(), BranchHint::kNone,
                           SelectOp::Implementation::kBranch);
        break;
      case interpreter::TestTypeOfFlags::LiteralFlag::kOther:
        UNREACHABLE();  // Should never be emitted.
    }

    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckDetectableCallable* node,
                                const maglev::ProcessingState& state) {
    V<Object> receiver = Map(node->receiver_input());

    ObjectIsOp::InputAssumptions assumptions;
    switch (node->check_type()) {
      case maglev::CheckType::kCheckHeapObject:
        assumptions = ObjectIsOp::InputAssumptions::kNone;
        break;
      case maglev::CheckType::kOmitHeapObjectCheck:
        assumptions = ObjectIsOp::InputAssumptions::kHeapObject;
        break;
    }

    __ DeoptimizeIfNot(
        __ ObjectIs(receiver, ObjectIsOp::Kind::kDetectableCallable,
                    assumptions),
        BuildFrameState(node->eager_deopt_info()),
        DeoptimizeReason::kNotDetectableReceiver,
        node->eager_deopt_info()->feedback_to_update());

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::BranchIfToBooleanTrue* node,
                                const maglev::ProcessingState& state) {
    TruncateJSPrimitiveToUntaggedOp::InputAssumptions assumption =
        node->check_type() == maglev::CheckType::kCheckHeapObject
            ? TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kObject
            : TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kHeapObject;
    V<Word32> condition = V<Word32>::Cast(__ TruncateJSPrimitiveToUntagged(
        Map(node->condition_input()),
        TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kBit, assumption));
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfInt32Compare* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition =
        ConvertCompare<Word32>(node->left_input(), node->right_input(),
                               node->operation(), Sign::kSigned);
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfUint32Compare* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition =
        ConvertCompare<Word32>(node->left_input(), node->right_input(),
                               node->operation(), Sign::kUnsigned);
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfFloat64Compare* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition =
        ConvertCompare<Float64>(node->left_input(), node->right_input(),
                                node->operation(), Sign::kSigned);
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfInt32ToBooleanTrue* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition = Map(node->condition_input());
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfFloat64ToBooleanTrue* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition = Float64ToBit(Map(node->condition_input()));
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfFloat64IsHole* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition = __ Float64IsHole(Map(node->condition_input()));
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfReferenceEqual* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition =
        __ TaggedEqual(Map(node->left_input()), Map(node->right_input()));
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfRootConstant* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition =
        RootEqual(node->condition_input(), node->root_index());
    __ Branch(condition, Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfUndefinedOrNull* node,
                                const maglev::ProcessingState& state) {
    __ GotoIf(RootEqual(node->condition_input(), RootIndex::kUndefinedValue),
              Map(node->if_true()));
    __ Branch(RootEqual(node->condition_input(), RootIndex::kNullValue),
              Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfUndetectable* node,
                                const maglev::ProcessingState& state) {
    ObjectIsOp::InputAssumptions assumption;
    switch (node->check_type()) {
      case maglev::CheckType::kCheckHeapObject:
        assumption = ObjectIsOp::InputAssumptions::kNone;
        break;
      case maglev::CheckType::kOmitHeapObjectCheck:
        assumption = ObjectIsOp::InputAssumptions::kHeapObject;
        break;
    }
    __ Branch(__ ObjectIs(Map(node->condition_input()),
                          ObjectIsOp::Kind::kUndetectable, assumption),
              Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfSmi* node,
                                const maglev::ProcessingState& state) {
    __ Branch(__ IsSmi(Map(node->condition_input())), Map(node->if_true()),
              Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::BranchIfJSReceiver* node,
                                const maglev::ProcessingState& state) {
    __ GotoIf(__ IsSmi(Map(node->condition_input())), Map(node->if_false()));
    __ Branch(__ JSAnyIsNotPrimitive(Map(node->condition_input())),
              Map(node->if_true()), Map(node->if_false()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Switch* node,
                                const maglev::ProcessingState& state) {
    compiler::turboshaft::SwitchOp::Case* cases =
        __ output_graph().graph_zone()
            -> AllocateArray<compiler::turboshaft::SwitchOp::Case>(
                             node->size());
    int case_value_base = node->value_base();
    for (int i = 0; i < node->size(); i++) {
      cases[i] = {i + case_value_base, Map(node->targets()[i].block_ptr()),
                  BranchHint::kNone};
    }
    Block* default_block;
    bool emit_default_block = false;
    if (node->has_fallthrough()) {
      default_block = Map(state.next_block());
    } else {
      default_block = __ NewBlock();
      emit_default_block = true;
    }
    __ Switch(Map(node->value()), base::VectorOf(cases, node->size()),
              default_block);
    if (emit_default_block) {
      __ Bind(default_block);
      __ Unreachable();
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckedSmiUntag* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ CheckedSmiUntag(Map(node->input()),
                              BuildFrameState(node->eager_deopt_info()),
                              node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::UnsafeSmiUntag* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ UntagSmi(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedSmiTagInt32* node,
                                const maglev::ProcessingState& state) {
    SetMap(
        node,
        __ ConvertUntaggedToJSPrimitiveOrDeopt(
            Map(node->input()), BuildFrameState(node->eager_deopt_info()),
            ConvertUntaggedToJSPrimitiveOrDeoptOp::JSPrimitiveKind::kSmi,
            RegisterRepresentation::Word32(),
            ConvertUntaggedToJSPrimitiveOrDeoptOp::InputInterpretation::kSigned,
            node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedSmiTagUint32* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ ConvertUntaggedToJSPrimitiveOrDeopt(
               Map(node->input()), BuildFrameState(node->eager_deopt_info()),
               ConvertUntaggedToJSPrimitiveOrDeoptOp::JSPrimitiveKind::kSmi,
               RegisterRepresentation::Word32(),
               ConvertUntaggedToJSPrimitiveOrDeoptOp::InputInterpretation::
                   kUnsigned,
               node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedSmiTagFloat64* node,
                                const maglev::ProcessingState& state) {
    V<Word32> as_int32 = __ ChangeFloat64ToInt32OrDeopt(
        Map(node->input()), BuildFrameState(node->eager_deopt_info()),
        CheckForMinusZeroMode::kCheckForMinusZero,
        node->eager_deopt_info()->feedback_to_update());
    SetMap(
        node,
        __ ConvertUntaggedToJSPrimitiveOrDeopt(
            as_int32, BuildFrameState(node->eager_deopt_info()),
            ConvertUntaggedToJSPrimitiveOrDeoptOp::JSPrimitiveKind::kSmi,
            RegisterRepresentation::Word32(),
            ConvertUntaggedToJSPrimitiveOrDeoptOp::InputInterpretation::kSigned,
            node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::UnsafeSmiTag* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ TagSmi(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }

#define PROCESS_BINOP_WITH_OVERFLOW(MaglevName, TurboshaftName,                \
                                    minus_zero_mode)                           \
  maglev::ProcessResult Process(maglev::Int32##MaglevName##WithOverflow* node, \
                                const maglev::ProcessingState& state) {        \
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());     \
    SetMap(node,                                                               \
           __ Word32##TurboshaftName##DeoptOnOverflow(                         \
               Map(node->left_input()), Map(node->right_input()), frame_state, \
               node->eager_deopt_info()->feedback_to_update(),                 \
               CheckForMinusZeroMode::k##minus_zero_mode));                    \
    return maglev::ProcessResult::kContinue;                                   \
  }
  PROCESS_BINOP_WITH_OVERFLOW(Add, SignedAdd, DontCheckForMinusZero)
  PROCESS_BINOP_WITH_OVERFLOW(Subtract, SignedSub, DontCheckForMinusZero)
  PROCESS_BINOP_WITH_OVERFLOW(Multiply, SignedMul, CheckForMinusZero)
  PROCESS_BINOP_WITH_OVERFLOW(Divide, SignedDiv, CheckForMinusZero)
  PROCESS_BINOP_WITH_OVERFLOW(Modulus, SignedMod, CheckForMinusZero)
#undef PROCESS_BINOP_WITH_OVERFLOW
  maglev::ProcessResult Process(maglev::Int32IncrementWithOverflow* node,
                                const maglev::ProcessingState& state) {
    // Turboshaft doesn't have a dedicated Increment operation; we use a regular
    // addition instead.
    SetMap(node, __ Word32SignedAddDeoptOnOverflow(
                     Map(node->value_input()), 1,
                     BuildFrameState(node->eager_deopt_info()),
                     node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Int32DecrementWithOverflow* node,
                                const maglev::ProcessingState& state) {
    // Turboshaft doesn't have a dedicated Decrement operation; we use a regular
    // addition instead.
    SetMap(node, __ Word32SignedSubDeoptOnOverflow(
                     Map(node->value_input()), 1,
                     BuildFrameState(node->eager_deopt_info()),
                     node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Int32NegateWithOverflow* node,
                                const maglev::ProcessingState& state) {
    // Turboshaft doesn't have a Int32NegateWithOverflow operation, but Turbofan
    // emits mutliplications by -1 for this, so using this as well here.
    SetMap(node, __ Word32SignedMulDeoptOnOverflow(
                     Map(node->value_input()), -1,
                     BuildFrameState(node->eager_deopt_info()),
                     node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }

#define PROCESS_FLOAT64_BINOP(MaglevName, TurboshaftName)               \
  maglev::ProcessResult Process(maglev::Float64##MaglevName* node,      \
                                const maglev::ProcessingState& state) { \
    SetMap(node, __ Float64##TurboshaftName(Map(node->left_input()),    \
                                            Map(node->right_input()))); \
    return maglev::ProcessResult::kContinue;                            \
  }
  PROCESS_FLOAT64_BINOP(Add, Add)
  PROCESS_FLOAT64_BINOP(Subtract, Sub)
  PROCESS_FLOAT64_BINOP(Multiply, Mul)
  PROCESS_FLOAT64_BINOP(Divide, Div)
  PROCESS_FLOAT64_BINOP(Modulus, Mod)
  PROCESS_FLOAT64_BINOP(Exponentiate, Power)
#undef PROCESS_FLOAT64_BINOP

#define PROCESS_INT32_BITWISE_BINOP(Name)                               \
  maglev::ProcessResult Process(maglev::Int32Bitwise##Name* node,       \
                                const maglev::ProcessingState& state) { \
    SetMap(node, __ Word32Bitwise##Name(Map(node->left_input()),        \
                                        Map(node->right_input())));     \
    return maglev::ProcessResult::kContinue;                            \
  }
  PROCESS_INT32_BITWISE_BINOP(And)
  PROCESS_INT32_BITWISE_BINOP(Or)
  PROCESS_INT32_BITWISE_BINOP(Xor)
#undef PROCESS_INT32_BITWISE_BINOP

#define PROCESS_INT32_SHIFT(MaglevName, TurboshaftName)                 \
  maglev::ProcessResult Process(maglev::Int32##MaglevName* node,        \
                                const maglev::ProcessingState& state) { \
    SetMap(node, __ Word32##TurboshaftName(Map(node->left_input()),     \
                                           Map(node->right_input())));  \
    return maglev::ProcessResult::kContinue;                            \
  }
  PROCESS_INT32_SHIFT(ShiftLeft, ShiftLeft)
  PROCESS_INT32_SHIFT(ShiftRight, ShiftRightArithmetic)
  PROCESS_INT32_SHIFT(ShiftRightLogical, ShiftRightLogical)
#undef PROCESS_INT32_SHIFT

  maglev::ProcessResult Process(maglev::Int32BitwiseNot* node,
                                const maglev::ProcessingState& state) {
    // Turboshaft doesn't have a bitwise Not operator; we instead use "^ -1".
    SetMap(node, __ Word32BitwiseXor(Map(node->value_input()),
                                     __ Word32Constant(-1)));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Int32AbsWithOverflow* node,
                                const maglev::ProcessingState& state) {
    V<Word32> input = Map(node->input());
    ScopedVariable<Word32, AssemblerT> result(this, input);

    IF (__ Int32LessThan(input, 0)) {
      V<Tuple<Word32, Word32>> result_with_ovf =
          __ Int32MulCheckOverflow(input, -1);
      __ DeoptimizeIf(__ Projection<1>(result_with_ovf),
                      BuildFrameState(node->eager_deopt_info()),
                      DeoptimizeReason::kOverflow,
                      node->eager_deopt_info()->feedback_to_update());
      result = __ Projection<0>(result_with_ovf);
    }

    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Float64Negate* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Float64Negate(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64Abs* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Float64Abs(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64Round* node,
                                const maglev::ProcessingState& state) {
    if (node->kind() == maglev::Float64Round::Kind::kFloor) {
      SetMap(node, __ Float64RoundDown(Map(node->input())));
    } else if (node->kind() == maglev::Float64Round::Kind::kCeil) {
      SetMap(node, __ Float64RoundUp(Map(node->input())));
    } else {
      DCHECK_EQ(node->kind(), maglev::Float64Round::Kind::kNearest);
      // Nearest rounds to +infinity on ties. We emulate this by rounding up and
      // adjusting if the difference exceeds 0.5 (like SimplifiedLowering does
      // for lower Float64Round).
      OpIndex input = Map(node->input());
      ScopedVariable<Float64, AssemblerT> result(this,
                                                 __ Float64RoundUp(input));
      IF_NOT (__ Float64LessThanOrEqual(__ Float64Sub(result, 0.5), input)) {
        result = __ Float64Sub(result, 1.0);
      }

      SetMap(node, result);
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Float64Ieee754Unary* node,
                                const maglev::ProcessingState& state) {
    FloatUnaryOp::Kind kind;
    switch (node->ieee_function()) {
#define CASE(MathName, ExpName, EnumName)                         \
  case maglev::Float64Ieee754Unary::Ieee754Function::k##EnumName: \
    kind = FloatUnaryOp::Kind::k##EnumName;                       \
    break;
      IEEE_754_UNARY_LIST(CASE)
#undef CASE
    }
    SetMap(node, __ Float64Unary(Map(node->input()), kind));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckedSmiIncrement* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    V<Smi> result;
    if constexpr (SmiValuesAre31Bits()) {
      result = __ BitcastWord32ToSmi(__ Word32SignedAddDeoptOnOverflow(
          __ BitcastSmiToWord32(Map(node->value_input())),
          Smi::FromInt(1).ptr(), frame_state,
          node->eager_deopt_info()->feedback_to_update()));
    } else {
      // Remember that 32-bit Smis are stored in the upper 32 bits of 64-bit
      // qwords. We thus perform a 64-bit addition rather than a 32-bit one,
      // despite Smis being only 32 bits.
      result = __ BitcastWordPtrToSmi(__ WordPtrSignedAddDeoptOnOverflow(
          __ BitcastSmiToWordPtr(Map(node->value_input())),
          Smi::FromInt(1).ptr(), frame_state,
          node->eager_deopt_info()->feedback_to_update()));
    }
    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedSmiDecrement* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    V<Smi> result;
    if constexpr (SmiValuesAre31Bits()) {
      result = __ BitcastWord32ToSmi(__ Word32SignedSubDeoptOnOverflow(
          __ BitcastSmiToWord32(Map(node->value_input())),
          Smi::FromInt(1).ptr(), frame_state,
          node->eager_deopt_info()->feedback_to_update()));
    } else {
      result = __ BitcastWordPtrToSmi(__ WordPtrSignedSubDeoptOnOverflow(
          __ BitcastSmiToWordPtr(Map(node->value_input())),
          Smi::FromInt(1).ptr(), frame_state,
          node->eager_deopt_info()->feedback_to_update()));
    }
    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }

// Note that Maglev collects feedback in the generic binops and unops, so that
// Turbofan has chance to get better feedback. However, once we reach Turbofan,
// we stop collecting feedback, since we've tried multiple times to keep
// collecting feedback in Turbofan, but it never seemed worth it. The latest
// occurence of this was ended by this CL: https://crrev.com/c/4110858.
#define PROCESS_GENERIC_BINOP(Name)                                            \
  maglev::ProcessResult Process(maglev::Generic##Name* node,                   \
                                const maglev::ProcessingState& state) {        \
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());      \
    SetMap(node,                                                               \
           __ Generic##Name(Map(node->left_input()), Map(node->right_input()), \
                            frame_state, native_context()));                   \
    return maglev::ProcessResult::kContinue;                                   \
  }
  GENERIC_BINOP_LIST(PROCESS_GENERIC_BINOP)
#undef PROCESS_GENERIC_BINOP

#define PROCESS_GENERIC_UNOP(Name)                                         \
  maglev::ProcessResult Process(maglev::Generic##Name* node,               \
                                const maglev::ProcessingState& state) {    \
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());  \
    SetMap(node, __ Generic##Name(Map(node->operand_input()), frame_state, \
                                  native_context()));                      \
    return maglev::ProcessResult::kContinue;                               \
  }
  GENERIC_UNOP_LIST(PROCESS_GENERIC_UNOP)
#undef PROCESS_GENERIC_UNOP

  maglev::ProcessResult Process(maglev::ToNumberOrNumeric* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    SetMap(node, __ ToNumberOrNumeric(Map(node->value_input()), frame_state,
                                      native_context(), node->mode()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LogicalNot* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition = __ TaggedEqual(
        Map(node->value()), __ HeapConstant(local_factory_->true_value()));
    SetMap(node, ConvertWord32ToJSBool(condition, /*flip*/ true));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ToBooleanLogicalNot* node,
                                const maglev::ProcessingState& state) {
    TruncateJSPrimitiveToUntaggedOp::InputAssumptions assumption =
        node->check_type() == maglev::CheckType::kCheckHeapObject
            ? TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kObject
            : TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kHeapObject;
    V<Word32> condition = V<Word32>::Cast(__ TruncateJSPrimitiveToUntagged(
        Map(node->value()), TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kBit,
        assumption));
    SetMap(node, ConvertWord32ToJSBool(condition, /*flip*/ true));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ToBoolean* node,
                                const maglev::ProcessingState& state) {
    TruncateJSPrimitiveToUntaggedOp::InputAssumptions input_assumptions;
    switch (node->check_type()) {
      case maglev::CheckType::kCheckHeapObject:
        input_assumptions =
            TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kObject;
        break;
      case maglev::CheckType::kOmitHeapObjectCheck:
        input_assumptions =
            TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kHeapObject;
        break;
    }
    SetMap(node, ConvertWord32ToJSBool(
                     V<Word32>::Cast(__ TruncateJSPrimitiveToUntagged(
                         Map(node->value()),
                         TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kBit,
                         input_assumptions))));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Int32ToBoolean* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, ConvertWord32ToJSBool(Map(node->value()), node->flip()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64ToBoolean* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition = Float64ToBit(Map(node->value()));
    SetMap(node, ConvertWord32ToJSBool(condition, node->flip()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Int32ToNumber* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ConvertInt32ToNumber(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Uint32ToNumber* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ConvertUint32ToNumber(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64ToTagged* node,
                                const maglev::ProcessingState& state) {
    // Float64ToTagged's conversion mode is used to control whether integer
    // floats should be converted to Smis or to HeapNumbers: kCanonicalizeSmi
    // means that they can be converted to Smis, and otherwise they should
    // remain HeapNumbers.
    ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind kind =
        node->conversion_mode() ==
                maglev::Float64ToTagged::ConversionMode::kCanonicalizeSmi
            ? ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kNumber
            : ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kHeapNumber;
    SetMap(node,
           __ ConvertUntaggedToJSPrimitive(
               Map(node->input()), kind, RegisterRepresentation::Float64(),
               ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kSigned,
               CheckForMinusZeroMode::kCheckForMinusZero));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::HoleyFloat64ToTagged* node,
                                const maglev::ProcessingState& state) {
    Label<Object> done(this);
    V<Float64> input = Map(node->input());
    if (node->conversion_mode() ==
        maglev::HoleyFloat64ToTagged::ConversionMode::kCanonicalizeSmi) {
      // ConvertUntaggedToJSPrimitive cannot at the same time canonicalize smis
      // and handle holes. We thus manually insert a smi check when the
      // conversion_mode is CanonicalizeSmi.
      IF (__ Float64IsSmi(input)) {
        GOTO(done,
             __ ConvertUntaggedToJSPrimitive(
                 __ TruncateFloat64ToInt32OverflowUndefined(input),
                 ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kSmi,
                 RegisterRepresentation::Word32(),
                 ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kSigned,
                 CheckForMinusZeroMode::kDontCheckForMinusZero));
      }
    }
    GOTO(done, __ ConvertUntaggedToJSPrimitive(
                   Map(node->input()),
                   ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::
                       kHeapNumberOrUndefined,
                   RegisterRepresentation::Float64(),
                   ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kSigned,
                   CheckForMinusZeroMode::kCheckForMinusZero));
    BIND(done, result);
    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64ToHeapNumberForField* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ ConvertUntaggedToJSPrimitive(
               Map(node->input()),
               ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kHeapNumber,
               RegisterRepresentation::Float64(),
               ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kSigned,
               CheckForMinusZeroMode::kCheckForMinusZero));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::HoleyFloat64IsHole* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, ConvertWord32ToJSBool(__ Float64IsHole(Map(node->input()))));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckedNumberOrOddballToFloat64* node,
                                const maglev::ProcessingState& state) {
    ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind kind;
    switch (node->conversion_type()) {
      case maglev::TaggedToFloat64ConversionType::kOnlyNumber:
        kind = ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::kNumber;
        break;
      case maglev::TaggedToFloat64ConversionType::kNumberOrOddball:
        kind = ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::
            kNumberOrOddball;
        break;
    }
    SetMap(
        node,
        __ ConvertJSPrimitiveToUntaggedOrDeopt(
            Map(node->input()), BuildFrameState(node->eager_deopt_info()), kind,
            ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kFloat64,
            CheckForMinusZeroMode::kCheckForMinusZero,
            node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::UncheckedNumberOrOddballToFloat64* node,
                                const maglev::ProcessingState& state) {
    // `node->conversion_type()` doesn't matter here, since for both HeapNumbers
    // and Oddballs, the Float64 value is at the same index (and this node never
    // deopts, regardless of its input).
    SetMap(node, __ ConvertJSPrimitiveToUntagged(
                     Map(node->input()),
                     ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kFloat64,
                     ConvertJSPrimitiveToUntaggedOp::InputAssumptions::
                         kNumberOrOddball));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TruncateUint32ToInt32* node,
                                const maglev::ProcessingState& state) {
    // This doesn't matter in Turboshaft: both Uint32 and Int32 are Word32.
    SetMap(node, Map(node->input()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedInt32ToUint32* node,
                                const maglev::ProcessingState& state) {
    __ DeoptimizeIf(__ Int32LessThan(Map(node->input()), 0),
                    BuildFrameState(node->eager_deopt_info()),
                    DeoptimizeReason::kNotUint32,
                    node->eager_deopt_info()->feedback_to_update());
    SetMap(node, Map(node->input()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedUint32ToInt32* node,
                                const maglev::ProcessingState& state) {
    __ DeoptimizeIf(__ Int32LessThan(Map(node->input()), 0),
                    BuildFrameState(node->eager_deopt_info()),
                    DeoptimizeReason::kNotInt32,
                    node->eager_deopt_info()->feedback_to_update());
    SetMap(node, Map(node->input()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::UnsafeInt32ToUint32* node,
                                const maglev::ProcessingState& state) {
    // This is a no-op in Maglev, and also in Turboshaft where both Uint32 and
    // Int32 are Word32.
    SetMap(node, Map(node->input()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedObjectToIndex* node,
                                const maglev::ProcessingState& state) {
    OpIndex result = __ ConvertJSPrimitiveToUntaggedOrDeopt(
        Map(node->object_input()), BuildFrameState(node->eager_deopt_info()),
        ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::kNumberOrString,
        ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kArrayIndex,
        CheckForMinusZeroMode::kCheckForMinusZero,
        node->eager_deopt_info()->feedback_to_update());
    SetMap(node, Is64() ? __ TruncateWord64ToWord32(result) : result);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ChangeInt32ToFloat64* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ChangeInt32ToFloat64(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ChangeUint32ToFloat64* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ChangeUint32ToFloat64(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedTruncateFloat64ToInt32* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ ChangeFloat64ToInt32OrDeopt(
               Map(node->input()), BuildFrameState(node->eager_deopt_info()),
               CheckForMinusZeroMode::kCheckForMinusZero,
               node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedTruncateFloat64ToUint32* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ ChangeFloat64ToUint32OrDeopt(
               Map(node->input()), BuildFrameState(node->eager_deopt_info()),
               CheckForMinusZeroMode::kCheckForMinusZero,
               node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(
      maglev::CheckedTruncateNumberOrOddballToInt32* node,
      const maglev::ProcessingState& state) {
    TruncateJSPrimitiveToUntaggedOrDeoptOp::InputRequirement input_requirement;
    switch (node->conversion_type()) {
      case maglev::TaggedToFloat64ConversionType::kOnlyNumber:
        input_requirement =
            TruncateJSPrimitiveToUntaggedOrDeoptOp::InputRequirement::kNumber;
        break;
      case maglev::TaggedToFloat64ConversionType::kNumberOrOddball:
        input_requirement = TruncateJSPrimitiveToUntaggedOrDeoptOp::
            InputRequirement::kNumberOrOddball;
        break;
    }
    SetMap(
        node,
        __ TruncateJSPrimitiveToUntaggedOrDeopt(
            Map(node->input()), BuildFrameState(node->eager_deopt_info()),
            TruncateJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kInt32,
            input_requirement, node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TruncateNumberOrOddballToInt32* node,
                                const maglev::ProcessingState& state) {
    // In Maglev, TruncateNumberOrOddballToInt32 does the same thing for both
    // NumberOrOddball and Number; except when debug_code is enabled: then,
    // Maglev inserts runtime checks ensuring that the input is indeed a Number
    // or NumberOrOddball. Turboshaft doesn't typically introduce such runtime
    // checks, so we instead just lower both Number and NumberOrOddball to the
    // NumberOrOddball variant.
    SetMap(node, __ TruncateJSPrimitiveToUntagged(
                     Map(node->input()),
                     TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kInt32,
                     TruncateJSPrimitiveToUntaggedOp::InputAssumptions::
                         kNumberOrOddball));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::TruncateFloat64ToInt32* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ JSTruncateFloat64ToWord32(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::HoleyFloat64ToMaybeNanFloat64* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Float64SilenceNaN(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedHoleyFloat64ToFloat64* node,
                                const maglev::ProcessingState& state) {
    V<Float64> input = Map(node->input());

    __ DeoptimizeIf(__ Float64IsHole(input),
                    BuildFrameState(node->eager_deopt_info()),
                    DeoptimizeReason::kHole,
                    node->eager_deopt_info()->feedback_to_update());

    SetMap(node, input);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ConvertHoleToUndefined* node,
                                const maglev::ProcessingState& state) {
    V<Word32> cond = RootEqual(node->object_input(), RootIndex::kTheHoleValue);
    SetMap(node,
           __ Select(cond, __ HeapConstant(local_factory_->undefined_value()),
                     Map<Object>(node->object_input()),
                     RegisterRepresentation::Tagged(), BranchHint::kNone,
                     SelectOp::Implementation::kBranch));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ConvertReceiver* node,
                                const maglev::ProcessingState& state) {
    Label<Object> done(this);
    Label<> non_js_receiver(this);
    V<Object> receiver = Map(node->receiver_input());

    GOTO_IF(__ IsSmi(receiver), non_js_receiver);

    GOTO_IF(__ JSAnyIsNotPrimitive(V<HeapObject>::Cast(receiver)), done,
            receiver);

    if (node->mode() != ConvertReceiverMode::kNotNullOrUndefined) {
      Label<> convert_global_proxy(this);
      GOTO_IF(__ RootEqual(receiver, RootIndex::kUndefinedValue, isolate_),
              convert_global_proxy);
      GOTO_IF_NOT(__ RootEqual(receiver, RootIndex::kNullValue, isolate_),
                  non_js_receiver);
      GOTO(convert_global_proxy);
      BIND(convert_global_proxy);
      GOTO(done,
           __ HeapConstant(
               node->native_context().global_proxy_object(broker_).object()));
    } else {
      GOTO(non_js_receiver);
    }

    BIND(non_js_receiver);
    GOTO(done, __ CallBuiltin_ToObject(
                   isolate_, __ HeapConstant(node->native_context().object()),
                   V<JSPrimitive>::Cast(receiver)));

    BIND(done, result);
    SetMap(node, result);

    return maglev::ProcessResult::kContinue;
  }

  static constexpr int kMinClampedUint8 = 0;
  static constexpr int kMaxClampedUint8 = 255;
  V<Word32> Int32ToUint8Clamped(V<Word32> value) {
    ScopedVariable<Word32, AssemblerT> result(this);
    IF (__ Int32LessThan(value, kMinClampedUint8)) {
      result = __ Word32Constant(kMinClampedUint8);
    } ELSE IF (__ Int32LessThan(value, kMaxClampedUint8)) {
      result = value;
    } ELSE {
      result = __ Word32Constant(kMaxClampedUint8);
    }
    return result;
  }
  V<Word32> Float64ToUint8Clamped(V<Float64> value) {
    ScopedVariable<Word32, AssemblerT> result(this);
    IF (__ Float64LessThan(value, kMinClampedUint8)) {
      result = __ Word32Constant(kMinClampedUint8);
    } ELSE IF (__ Float64LessThan(kMaxClampedUint8, value)) {
      result = __ Word32Constant(kMaxClampedUint8);
    } ELSE {
      // Note that this case handles values that are in range of Clamped Uint8
      // and NaN. The order of the IF/ELSE-IF/ELSE in this function is so that
      // we do indeed end up here for NaN.
      result = __ JSTruncateFloat64ToWord32(__ Float64RoundTiesEven(value));
    }
    return result;
  }

  maglev::ProcessResult Process(maglev::Int32ToUint8Clamped* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, Int32ToUint8Clamped(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Uint32ToUint8Clamped* node,
                                const maglev::ProcessingState& state) {
    ScopedVariable<Word32, AssemblerT> result(this);
    V<Word32> value = Map(node->input());
    IF (__ Uint32LessThan(value, kMaxClampedUint8)) {
      result = value;
    } ELSE {
      result = __ Word32Constant(kMaxClampedUint8);
    }
    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64ToUint8Clamped* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, Float64ToUint8Clamped(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CheckedNumberToUint8Clamped* node,
                                const maglev::ProcessingState& state) {
    ScopedVariable<Word32, AssemblerT> result(this);
    V<Object> value = Map(node->input());
    IF (__ IsSmi(value)) {
      result = Int32ToUint8Clamped(__ UntagSmi(V<Smi>::Cast(value)));
    } ELSE {
      V<i::Map> map = __ LoadMapField(value);
      __ DeoptimizeIfNot(
          __ TaggedEqual(map,
                         __ HeapConstant(local_factory_->heap_number_map())),
          BuildFrameState(node->eager_deopt_info()),
          DeoptimizeReason::kNotAHeapNumber,
          node->eager_deopt_info()->feedback_to_update());
      result = Float64ToUint8Clamped(
          __ LoadField<Float64>(value, AccessBuilder::ForHeapNumberValue()));
    }
    SetMap(node, result);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ToObject* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ConvertJSPrimitiveToObject(
                     Map(node->value_input()), Map(node->context()),
                     ConvertReceiverMode::kNotNullOrUndefined));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Return* node,
                                const maglev::ProcessingState& state) {
    __ Return(Map(node->value_input()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Deopt* node,
                                const maglev::ProcessingState& state) {
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ Deoptimize(frame_state, node->reason(),
                  node->eager_deopt_info()->feedback_to_update());
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::SetPendingMessage* node,
                                const maglev::ProcessingState& state) {
    V<WordPtr> message_address = __ ExternalConstant(
        ExternalReference::address_of_pending_message(isolate_));
    V<Object> old_message = __ LoadMessage(message_address);
    __ StoreMessage(message_address, Map(node->value()));
    SetMap(node, old_message);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Abort* node,
                                const maglev::ProcessingState&) {
    __ RuntimeAbort(node->reason());
    // TODO(dmercadier): remove this `Unreachable` once RuntimeAbort is marked
    // as a block terminator.
    __ Unreachable();
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Identity* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, Map(node->input(0)));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::GetSecondReturnedValue* node,
                                const maglev::ProcessingState& state) {
    DCHECK(second_return_value_.valid());
    SetMap(node, second_return_value_);

#ifdef DEBUG
    second_return_value_ = V<Object>::Invalid();
#endif

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::TryOnStackReplacement*,
                                const maglev::ProcessingState&) {
    // Turboshaft is the top tier compiler, so we never need to OSR from it.
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ReduceInterruptBudgetForReturn*,
                                const maglev::ProcessingState&) {
    // No need to update the interrupt budget once we reach Turboshaft.
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ReduceInterruptBudgetForLoop* node,
                                const maglev::ProcessingState&) {
    // ReduceInterruptBudgetForLoop nodes are not emitted by Maglev when it is
    // used as a frontend for Turboshaft.
    UNREACHABLE();
  }
  maglev::ProcessResult Process(maglev::HandleNoHeapWritesInterrupt* node,
                                const maglev::ProcessingState&) {
    __ JSLoopStackCheck(native_context(),
                        BuildFrameState(node->lazy_deopt_info()));
    return maglev::ProcessResult::kContinue;
  }

  template <typename NodeT>
  maglev::ProcessResult Process(NodeT* node,
                                const maglev::ProcessingState& state) {
    UNIMPLEMENTED();
  }

  AssemblerT& Asm() { return assembler_; }
  Zone* temp_zone() { return temp_zone_; }
  Zone* graph_zone() { return Asm().output_graph().graph_zone(); }

 private:
  V<FrameState> BuildFrameState(maglev::EagerDeoptInfo* eager_deopt_info) {
    // Eager deopts don't have a result location/size.
    const interpreter::Register result_location =
        interpreter::Register::invalid_value();
    const int result_size = 0;

    switch (eager_deopt_info->top_frame().type()) {
      case maglev::DeoptFrame::FrameType::kInterpretedFrame:
        return BuildFrameState(eager_deopt_info->top_frame().as_interpreted(),
                               result_location, result_size);
      case maglev::DeoptFrame::FrameType::kBuiltinContinuationFrame:
        return BuildFrameState(
            eager_deopt_info->top_frame().as_builtin_continuation());
      case maglev::DeoptFrame::FrameType::kInlinedArgumentsFrame:
      case maglev::DeoptFrame::FrameType::kConstructInvokeStubFrame:
        UNIMPLEMENTED();
    }
  }

  V<FrameState> BuildFrameState(maglev::LazyDeoptInfo* lazy_deopt_info) {
    switch (lazy_deopt_info->top_frame().type()) {
      case maglev::DeoptFrame::FrameType::kInterpretedFrame:
        return BuildFrameState(lazy_deopt_info->top_frame().as_interpreted(),
                               lazy_deopt_info->result_location(),
                               lazy_deopt_info->result_size());
      case maglev::DeoptFrame::FrameType::kConstructInvokeStubFrame:
        return BuildFrameState(
            lazy_deopt_info->top_frame().as_construct_stub());

      case maglev::DeoptFrame::FrameType::kBuiltinContinuationFrame:
        return BuildFrameState(
            lazy_deopt_info->top_frame().as_builtin_continuation());

      case maglev::DeoptFrame::FrameType::kInlinedArgumentsFrame:
        UNIMPLEMENTED();
    }
  }

  V<FrameState> BuildParentFrameState(maglev::DeoptFrame& frame) {
    // Only the topmost frame should have a valid result_location and
    // result_size. One reason for this is that, in Maglev, the PokeAt is not an
    // attribute of the DeoptFrame but rather of the LazyDeoptInfo (to which the
    // topmost frame is attached).
    const interpreter::Register result_location =
        interpreter::Register::invalid_value();
    const int result_size = 0;

    switch (frame.type()) {
      case maglev::DeoptFrame::FrameType::kInterpretedFrame:
        return BuildFrameState(frame.as_interpreted(), result_location,
                               result_size);
      case maglev::DeoptFrame::FrameType::kConstructInvokeStubFrame:
        return BuildFrameState(frame.as_construct_stub());
      case maglev::DeoptFrame::FrameType::kInlinedArgumentsFrame:
        return BuildFrameState(frame.as_inlined_arguments());
      case maglev::DeoptFrame::FrameType::kBuiltinContinuationFrame:
        return BuildFrameState(frame.as_builtin_continuation());
    }
  }

  V<FrameState> BuildFrameState(maglev::ConstructInvokeStubDeoptFrame& frame) {
    FrameStateData::Builder builder;
    if (frame.parent() != nullptr) {
      V<FrameState> parent_frame = BuildParentFrameState(*frame.parent());
      builder.AddParentFrameState(parent_frame);
    }

    // Closure
    // TODO(dmercadier): ConstructInvokeStub frames don't have a Closure input,
    // but the instruction selector assumes that they do and that it should be
    // skipped. We thus use SmiConstant(0) as a fake Closure input here, but it
    // would be nicer to fix the instruction selector to not require this input
    // at all for such frames.
    V<Any> fake_closure_input = __ SmiConstant(0);
    builder.AddInput(MachineType::AnyTagged(), fake_closure_input);

    // Parameters
    AddDeoptInput(builder, frame.receiver());

    // Context
    AddDeoptInput(builder, frame.context());

    const FrameStateInfo* frame_state_info = MakeFrameStateInfo(frame);
    return __ FrameState(
        builder.Inputs(), builder.inlined(),
        builder.AllocateFrameStateData(*frame_state_info, graph_zone()));
  }

  V<FrameState> BuildFrameState(maglev::InlinedArgumentsDeoptFrame& frame) {
    FrameStateData::Builder builder;
    if (frame.parent() != nullptr) {
      V<FrameState> parent_frame = BuildParentFrameState(*frame.parent());
      builder.AddParentFrameState(parent_frame);
    }

    // Closure
    AddDeoptInput(builder, frame.closure());

    // Parameters
    for (const maglev::ValueNode* arg : frame.arguments()) {
      AddDeoptInput(builder, arg);
    }

    // Context
    // TODO(dmercadier): InlinedExtraArguments frames don't have a Context
    // input, but the instruction selector assumes that they do and that it
    // should be skipped. We thus use SmiConstant(0) as a fake Context input
    // here, but it would be nicer to fix the instruction selector to not
    // require this input at all for such frames.
    V<Any> fake_context_input = __ SmiConstant(0);
    builder.AddInput(MachineType::AnyTagged(), fake_context_input);

    const FrameStateInfo* frame_state_info = MakeFrameStateInfo(frame);
    return __ FrameState(
        builder.Inputs(), builder.inlined(),
        builder.AllocateFrameStateData(*frame_state_info, graph_zone()));
  }

  V<FrameState> BuildFrameState(maglev::BuiltinContinuationDeoptFrame& frame) {
    FrameStateData::Builder builder;
    if (frame.parent() != nullptr) {
      V<FrameState> parent_frame = BuildParentFrameState(*frame.parent());
      builder.AddParentFrameState(parent_frame);
    }

    // Closure
    if (frame.is_javascript()) {
      builder.AddInput(MachineType::AnyTagged(),
                       __ HeapConstant(frame.javascript_target().object()));
    } else {
      builder.AddUnusedRegister();
    }

    // Parameters
    for (maglev::ValueNode* param : frame.parameters()) {
      AddDeoptInput(builder, param);
    }

    // Extra fixed JS frame parameters. These are at the end since JS builtins
    // push their parameters in reverse order.
    constexpr int kExtraFixedJSFrameParameters = 3;
    if (frame.is_javascript()) {
      static_assert(kExtraFixedJSFrameParameters == 3);
      // kJavaScriptCallTargetRegister
      builder.AddInput(MachineType::AnyTagged(),
                       __ HeapConstant(frame.javascript_target().object()));
      // kJavaScriptCallNewTargetRegister
      builder.AddInput(MachineType::AnyTagged(),
                       __ HeapConstant(local_factory_->undefined_value()));
      // kJavaScriptCallArgCountRegister
      builder.AddInput(
          MachineType::AnyTagged(),
          __ SmiConstant(Smi::FromInt(
              Builtins::GetStackParameterCount(frame.builtin_id()))));
    }

    // Context
    AddDeoptInput(builder, frame.context());

    const FrameStateInfo* frame_state_info = MakeFrameStateInfo(frame);
    return __ FrameState(
        builder.Inputs(), builder.inlined(),
        builder.AllocateFrameStateData(*frame_state_info, graph_zone()));
  }

  V<FrameState> BuildFrameState(maglev::InterpretedDeoptFrame& frame,
                                interpreter::Register result_location,
                                int result_size) {
    DCHECK_EQ(result_size != 0, result_location.is_valid());
    FrameStateData::Builder builder;

    if (frame.parent() != nullptr) {
      V<FrameState> parent_frame = BuildParentFrameState(*frame.parent());
      builder.AddParentFrameState(parent_frame);
    }

    // Closure
    AddDeoptInput(builder, frame.closure());

    // Parameters
    frame.frame_state()->ForEachParameter(
        frame.unit(), [&](maglev::ValueNode* value, interpreter::Register reg) {
          AddDeoptInput(builder, value, reg, result_location, result_size);
        });

    // Context
    AddDeoptInput(builder, frame.frame_state()->context(frame.unit()));

    // Locals
    // ForEachLocal in Maglev skips over dead registers, but we still need to
    // call AddUnusedRegister on the Turboshaft FrameStateData Builder.
    // {local_index} is used to keep track of such unused registers.
    // Among the variables not included in ForEachLocal is the Accumulator (but
    // this is fine since there is an entry in the state specifically for the
    // accumulator later).
    int local_index = 0;
    frame.frame_state()->ForEachLocal(
        frame.unit(), [&](maglev::ValueNode* value, interpreter::Register reg) {
          while (local_index < reg.index()) {
            builder.AddUnusedRegister();
            local_index++;
          }
          AddDeoptInput(builder, value, reg, result_location, result_size);
          local_index++;
        });
    for (; local_index < frame.unit().register_count(); local_index++) {
      builder.AddUnusedRegister();
    }

    // Accumulator
    if (frame.frame_state()->liveness()->AccumulatorIsLive()) {
      AddDeoptInput(builder, frame.frame_state()->accumulator(frame.unit()),
                    interpreter::Register::virtual_accumulator(),
                    result_location, result_size);
    } else {
      builder.AddUnusedRegister();
    }

    OutputFrameStateCombine combine =
        ComputeCombine(frame, result_location, result_size);

    const FrameStateInfo* frame_state_info = MakeFrameStateInfo(frame, combine);
    return __ FrameState(
        builder.Inputs(), builder.inlined(),
        builder.AllocateFrameStateData(*frame_state_info, graph_zone()));
  }

  void AddDeoptInput(FrameStateData::Builder& builder,
                     const maglev::ValueNode* node) {
    builder.AddInput(MachineTypeFor(node->value_representation()), Map(node));
  }

  void AddDeoptInput(FrameStateData::Builder& builder,
                     const maglev::ValueNode* node, interpreter::Register reg,
                     interpreter::Register result_location, int result_size) {
    if (result_location.is_valid() && maglev::LazyDeoptInfo::InReturnValues(
                                          reg, result_location, result_size)) {
      builder.AddUnusedRegister();
    } else {
      AddDeoptInput(builder, node);
    }
  }

  OutputFrameStateCombine ComputeCombine(maglev::InterpretedDeoptFrame& frame,
                                         interpreter::Register result_location,
                                         int result_size) {
    if (result_size == 0) {
      return OutputFrameStateCombine::Ignore();
    }
    return OutputFrameStateCombine::PokeAt(
        frame.ComputeReturnOffset(result_location, result_size));
  }

  const FrameStateInfo* MakeFrameStateInfo(
      maglev::InterpretedDeoptFrame& maglev_frame,
      OutputFrameStateCombine combine) {
    FrameStateType type = FrameStateType::kUnoptimizedFunction;
    uint16_t parameter_count = maglev_frame.unit().parameter_count();
    uint16_t max_arguments = maglev_frame.unit().max_arguments();
    int local_count = maglev_frame.unit().register_count();
    Handle<SharedFunctionInfo> shared_info =
        maglev_frame.unit().shared_function_info().object();
    FrameStateFunctionInfo* info = graph_zone()->New<FrameStateFunctionInfo>(
        type, parameter_count, max_arguments, local_count, shared_info);

    return graph_zone()->New<FrameStateInfo>(maglev_frame.bytecode_position(),
                                             combine, info);
  }

  const FrameStateInfo* MakeFrameStateInfo(
      maglev::InlinedArgumentsDeoptFrame& maglev_frame) {
    FrameStateType type = FrameStateType::kInlinedExtraArguments;
    uint16_t parameter_count =
        static_cast<uint16_t>(maglev_frame.arguments().size());
    uint16_t max_arguments = 0;
    int local_count = 0;
    Handle<SharedFunctionInfo> shared_info =
        maglev_frame.unit().shared_function_info().object();
    FrameStateFunctionInfo* info = graph_zone()->New<FrameStateFunctionInfo>(
        type, parameter_count, max_arguments, local_count, shared_info);

    return graph_zone()->New<FrameStateInfo>(maglev_frame.bytecode_position(),
                                             OutputFrameStateCombine::Ignore(),
                                             info);
  }

  const FrameStateInfo* MakeFrameStateInfo(
      maglev::ConstructInvokeStubDeoptFrame& maglev_frame) {
    FrameStateType type = FrameStateType::kConstructInvokeStub;
    Handle<SharedFunctionInfo> shared_info =
        maglev_frame.unit().shared_function_info().object();
    constexpr uint16_t kParameterCount = 1;  // Only 1 parameter: the receiver.
    constexpr uint16_t kMaxArguments = 0;
    constexpr int kLocalCount = 0;
    FrameStateFunctionInfo* info = graph_zone()->New<FrameStateFunctionInfo>(
        type, kParameterCount, kMaxArguments, kLocalCount, shared_info);

    return graph_zone()->New<FrameStateInfo>(
        BytecodeOffset::None(), OutputFrameStateCombine::Ignore(), info);
  }

  const FrameStateInfo* MakeFrameStateInfo(
      maglev::BuiltinContinuationDeoptFrame& maglev_frame) {
    FrameStateType type = maglev_frame.is_javascript()
                              ? FrameStateType::kJavaScriptBuiltinContinuation
                              : FrameStateType::kBuiltinContinuation;
    uint16_t parameter_count =
        static_cast<uint16_t>(maglev_frame.parameters().length());
    constexpr int kExtraFixedJSFrameParameters = 3;
    if (maglev_frame.is_javascript()) {
      parameter_count += kExtraFixedJSFrameParameters;
    }
    Handle<SharedFunctionInfo> shared_info =
        GetSharedFunctionInfo(maglev_frame).object();
    constexpr int kLocalCount = 0;
    constexpr uint16_t kMaxArguments = 0;
    FrameStateFunctionInfo* info = graph_zone()->New<FrameStateFunctionInfo>(
        type, parameter_count, kMaxArguments, kLocalCount, shared_info);

    return graph_zone()->New<FrameStateInfo>(
        Builtins::GetContinuationBytecodeOffset(maglev_frame.builtin_id()),
        OutputFrameStateCombine::Ignore(), info);
  }

  SharedFunctionInfoRef GetSharedFunctionInfo(
      const maglev::DeoptFrame& deopt_frame) {
    switch (deopt_frame.type()) {
      case maglev::DeoptFrame::FrameType::kInterpretedFrame:
        return deopt_frame.as_interpreted().unit().shared_function_info();
      case maglev::DeoptFrame::FrameType::kInlinedArgumentsFrame:
        return deopt_frame.as_inlined_arguments().unit().shared_function_info();
      case maglev::DeoptFrame::FrameType::kConstructInvokeStubFrame:
        return deopt_frame.as_construct_stub().unit().shared_function_info();
      case maglev::DeoptFrame::FrameType::kBuiltinContinuationFrame:
        return GetSharedFunctionInfo(*deopt_frame.parent());
    }
  }

  enum class Sign { kSigned, kUnsigned };
  template <typename rep>
  V<Word32> ConvertCompare(maglev::Input left_input, maglev::Input right_input,
                           ::Operation operation, Sign sign) {
    DCHECK_IMPLIES(
        (std::is_same_v<rep, Float64> || std::is_same_v<rep, Float32>),
        sign == Sign::kSigned);
    ComparisonOp::Kind kind;
    bool swap_inputs = false;
    switch (operation) {
      case ::Operation::kEqual:
      case ::Operation::kStrictEqual:
        kind = ComparisonOp::Kind::kEqual;
        break;
      case ::Operation::kLessThan:
        kind = sign == Sign::kSigned ? ComparisonOp::Kind::kSignedLessThan
                                     : ComparisonOp::Kind::kUnsignedLessThan;
        break;
      case ::Operation::kLessThanOrEqual:
        kind = sign == Sign::kSigned
                   ? ComparisonOp::Kind::kSignedLessThanOrEqual
                   : ComparisonOp::Kind::kUnsignedLessThanOrEqual;
        break;
      case ::Operation::kGreaterThan:
        kind = sign == Sign::kSigned ? ComparisonOp::Kind::kSignedLessThan
                                     : ComparisonOp::Kind::kUnsignedLessThan;
        swap_inputs = true;
        break;
      case ::Operation::kGreaterThanOrEqual:
        kind = sign == Sign::kSigned
                   ? ComparisonOp::Kind::kSignedLessThanOrEqual
                   : ComparisonOp::Kind::kUnsignedLessThanOrEqual;
        swap_inputs = true;
        break;
      default:
        UNREACHABLE();
    }
    V<rep> left = Map(left_input);
    V<rep> right = Map(right_input);
    if (swap_inputs) std::swap(left, right);
    return __ Comparison(left, right, kind, V<rep>::rep);
  }

  V<Word32> ConvertInt32Compare(maglev::Input left_input,
                                maglev::Input right_input,
                                maglev::AssertCondition condition,
                                bool* negate_result) {
    ComparisonOp::Kind kind;
    bool swap_inputs = false;
    switch (condition) {
      case maglev::AssertCondition::kEqual:
        kind = ComparisonOp::Kind::kEqual;
        break;
      case maglev::AssertCondition::kNotEqual:
        kind = ComparisonOp::Kind::kEqual;
        *negate_result = true;
        break;
      case maglev::AssertCondition::kLessThan:
        kind = ComparisonOp::Kind::kSignedLessThan;
        break;
      case maglev::AssertCondition::kLessThanEqual:
        kind = ComparisonOp::Kind::kSignedLessThanOrEqual;
        break;
      case maglev::AssertCondition::kGreaterThan:
        kind = ComparisonOp::Kind::kSignedLessThan;
        swap_inputs = true;
        break;
      case maglev::AssertCondition::kGreaterThanEqual:
        kind = ComparisonOp::Kind::kSignedLessThanOrEqual;
        swap_inputs = true;
        break;
      case maglev::AssertCondition::kUnsignedLessThan:
        kind = ComparisonOp::Kind::kUnsignedLessThan;
        break;
      case maglev::AssertCondition::kUnsignedLessThanEqual:
        kind = ComparisonOp::Kind::kUnsignedLessThanOrEqual;
        break;
      case maglev::AssertCondition::kUnsignedGreaterThan:
        kind = ComparisonOp::Kind::kUnsignedLessThan;
        swap_inputs = true;
        break;
      case maglev::AssertCondition::kUnsignedGreaterThanEqual:
        kind = ComparisonOp::Kind::kUnsignedLessThanOrEqual;
        swap_inputs = true;
        break;
    }
    V<Word32> left = Map(left_input);
    V<Word32> right = Map(right_input);
    if (swap_inputs) std::swap(left, right);
    return __ Comparison(left, right, kind, WordRepresentation::Word32());
  }

  V<Word32> RootEqual(maglev::Input input, RootIndex root) {
    return __ RootEqual(Map(input), root, isolate_);
  }

  void DeoptIfInt32IsNotSmi(maglev::Input maglev_input,
                            maglev::NodeBase* node) {
    // TODO(dmercadier): is there no higher level way of doing this?
    V<Word32> input = Map<Word32>(maglev_input);
    V<Tuple<Word32, Word32>> add = __ Int32AddCheckOverflow(input, input);
    V<Word32> check = __ template Projection<1>(add);
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ DeoptimizeIf(check, frame_state, DeoptimizeReason::kNotASmi,
                    node->eager_deopt_info()->feedback_to_update());
  }

  std::pair<V<WordPtr>, V<Object>> GetTypedArrayDataAndBasePointers(
      V<JSTypedArray> typed_array) {
    V<WordPtr> data_pointer = __ LoadField<WordPtr>(
        typed_array, AccessBuilder::ForJSTypedArrayExternalPointer());
    V<Object> base_pointer = __ LoadField<Object>(
        typed_array, AccessBuilder::ForJSTypedArrayBasePointer());
    return {data_pointer, base_pointer};
  }
  V<Untagged> BuildTypedArrayLoad(V<JSTypedArray> typed_array, V<Word32> index,
                                  ElementsKind kind) {
    auto [data_pointer, base_pointer] =
        GetTypedArrayDataAndBasePointers(typed_array);
    return __ LoadTypedElement(typed_array, base_pointer, data_pointer,
                               __ ChangeUint32ToUintPtr(index),
                               GetArrayTypeFromElementsKind(kind));
  }
  void BuildTypedArrayStore(V<JSTypedArray> typed_array, V<Word32> index,
                            V<Untagged> value, ElementsKind kind) {
    auto [data_pointer, base_pointer] =
        GetTypedArrayDataAndBasePointers(typed_array);
    __ StoreTypedElement(typed_array, base_pointer, data_pointer,
                         __ ChangeUint32ToUintPtr(index), value,
                         GetArrayTypeFromElementsKind(kind));
  }

  void FixLoopPhis(maglev::BasicBlock* loop) {
    DCHECK(loop->is_loop());
    if (!loop->has_phi()) return;
    for (maglev::Phi* maglev_phi : *loop->phis()) {
      OpIndex phi_index = Map(maglev_phi);
      PendingLoopPhiOp& pending_phi =
          __ output_graph().Get(phi_index).Cast<PendingLoopPhiOp>();
      __ output_graph().Replace<PhiOp>(
          phi_index,
          base::VectorOf(
              {pending_phi.first(), Map(maglev_phi -> backedge_input())}),
          pending_phi.rep);
    }
  }

  RegisterRepresentation RegisterRepresentationFor(
      maglev::ValueRepresentation value_rep) {
    switch (value_rep) {
      case maglev::ValueRepresentation::kTagged:
        return RegisterRepresentation::Tagged();
      case maglev::ValueRepresentation::kInt32:
      case maglev::ValueRepresentation::kUint32:
        return RegisterRepresentation::Word32();
      case maglev::ValueRepresentation::kFloat64:
      case maglev::ValueRepresentation::kHoleyFloat64:
        return RegisterRepresentation::Float64();
      case maglev::ValueRepresentation::kIntPtr:
        return RegisterRepresentation::WordPtr();
    }
  }

  // TODO(dmercadier): Using a Branch would open more optimization opportunities
  // for BranchElimination compared to using a Select. However, in most cases,
  // Maglev should avoid materializing JS booleans, so there is a good chance
  // that it we actually need to do it, it's because we have to, and
  // BranchElimination probably cannot help. Thus, using a Select rather than a
  // Branch leads to smaller graphs, which is generally beneficial. Still, once
  // the graph builder is finished, we should evaluate whether Select or Branch
  // is the best choice here.
  V<Boolean> ConvertWord32ToJSBool(V<Word32> b, bool flip = false) {
    V<Boolean> true_idx = __ HeapConstant(local_factory_->true_value());
    V<Boolean> false_idx = __ HeapConstant(local_factory_->false_value());
    if (flip) std::swap(true_idx, false_idx);
    return __ Select(b, true_idx, false_idx, RegisterRepresentation::Tagged(),
                     BranchHint::kNone, SelectOp::Implementation::kBranch);
  }

  // Converts a Float64 to a Word32 boolean, correctly producing 0 for NaN, by
  // relying on the fact that "0.0 < abs(x)" is only false for NaN and 0.
  V<Word32> Float64ToBit(V<Float64> input) {
    return __ Float64LessThan(0.0, __ Float64Abs(input));
  }

  LazyDeoptOnThrow ShouldLazyDeoptOnThrow(maglev::NodeBase* node) {
    if (!node->properties().can_throw()) return LazyDeoptOnThrow::kNo;
    const maglev::ExceptionHandlerInfo* info = node->exception_handler_info();
    if (info->ShouldLazyDeopt()) return LazyDeoptOnThrow::kYes;
    return LazyDeoptOnThrow::kNo;
  }

  class ThrowingScope {
    // In Maglev, exception handlers have no predecessors, and their Phis are a
    // bit special: they all correspond to interpreter registers, and get
    // eventually initialized with the value that their predecessors have for
    // the corresponding interpreter registers.

    // In Turboshaft, exception handlers have predecessors and contain regular
    // phis. Creating a ThrowingScope takes care of recording in Variables
    // the current value of interpreter registers (right before emitting a node
    // that can throw), and sets the current_catch_block of the Assembler.
    // Throwing operations that are emitted while the scope is active will
    // automatically be wired to the catch handler. Then, when calling
    // Process(Phi) on exception phis (= when processing the catch handler),
    // these Phis will be mapped to the Variable corresponding to their owning
    // intepreter register.

   public:
    ThrowingScope(GraphBuilder* builder, maglev::NodeBase* throwing_node)
        : builder_(*builder) {
      if (!throwing_node->properties().can_throw()) return;
      const maglev::ExceptionHandlerInfo* info =
          throwing_node->exception_handler_info();
      if (!info->HasExceptionHandler() || info->ShouldLazyDeopt()) return;

      maglev::BasicBlock* block = info->catch_block.block_ptr();
      auto* liveness = block->state()->frame_state().liveness();

      maglev::LazyDeoptInfo* deopt_info = throwing_node->lazy_deopt_info();
      const maglev::InterpretedDeoptFrame* lazy_frame;
      switch (deopt_info->top_frame().type()) {
        case maglev::DeoptFrame::FrameType::kInterpretedFrame:
          lazy_frame = &deopt_info->top_frame().as_interpreted();
          break;
        case maglev::DeoptFrame::FrameType::kInlinedArgumentsFrame:
          UNREACHABLE();
        case maglev::DeoptFrame::FrameType::kConstructInvokeStubFrame:
        case maglev::DeoptFrame::FrameType::kBuiltinContinuationFrame:
          lazy_frame = &deopt_info->top_frame().parent()->as_interpreted();
          break;
      }

      lazy_frame->frame_state()->ForEachValue(
          lazy_frame->unit(), [this, liveness](maglev::ValueNode* value,
                                               interpreter::Register reg) {
            if (!reg.is_parameter() && !liveness->RegisterIsLive(reg.index())) {
              // Skip, since not live at the handler offset.
              return;
            }
            if (reg == interpreter::Register::virtual_accumulator()) {
              // If the accumulator is live, the it corresponds to the exception
              // object rather than whatever value it contained before the
              // throwing operation. We don't record anything here, and when
              // creating the exception phis, we'll use `catch_block_begin_` for
              // the accumulator.
              return;
            }
            auto it = builder_.regs_to_vars_.find(reg.index());
            Variable var;
            if (it == builder_.regs_to_vars_.end()) {
              var = __ NewVariable(RegisterRepresentation::Tagged());
              builder_.regs_to_vars_.insert({reg.index(), var});
            } else {
              var = it->second;
            }
            __ SetVariable(var, builder_.Map(value));
          });

      DCHECK_EQ(__ current_catch_block(), nullptr);
      __ set_current_catch_block(builder_.Map(block));
    }

    ~ThrowingScope() {
      // Resetting the catch handler. It is always set on a case-by-case basis
      // before emitting a throwing node, so there is no need to "reset the
      // previous catch handler" or something like that, since there is no
      // previous handler (there is a DCHECK in the ThrowingScope constructor
      // checking that the current_catch_block is indeed nullptr when the scope
      // is created).
      __ set_current_catch_block(nullptr);
    }

   private:
    GraphBuilder::AssemblerT& Asm() { return builder_.Asm(); }
    GraphBuilder& builder_;
  };

  template <typename T>
  V<T> Map(const maglev::Input input) {
    return V<T>::Cast(Map(input.node()));
  }
  OpIndex Map(const maglev::Input input) { return Map(input.node()); }
  OpIndex Map(const maglev::NodeBase* node) {
    DCHECK(node_mapping_[node].valid());
    return node_mapping_[node];
  }
  Block* Map(const maglev::BasicBlock* block) { return block_mapping_[block]; }

  OpIndex SetMap(maglev::NodeBase* node, OpIndex idx) {
    DCHECK(idx.valid());
    DCHECK_EQ(__ output_graph().Get(idx).outputs_rep().size(), 1);
    node_mapping_[node] = idx;
    return idx;
  }

  V<NativeContext> native_context() {
    DCHECK(native_context_.valid());
    return native_context_;
  }

  PipelineData* data_;
  Zone* temp_zone_;
  Isolate* isolate_ = data_->isolate();
  LocalIsolate* local_isolate_ = isolate_->AsLocalIsolate();
  JSHeapBroker* broker_ = data_->broker();
  LocalFactory* local_factory_ = local_isolate_->factory();
  AssemblerT assembler_;
  maglev::MaglevCompilationUnit* maglev_compilation_unit_;
  ZoneUnorderedMap<const maglev::NodeBase*, OpIndex> node_mapping_;
  ZoneUnorderedMap<const maglev::BasicBlock*, Block*> block_mapping_;
  ZoneUnorderedMap<int, Variable> regs_to_vars_;

  // In Turboshaft, exception blocks start with a CatchBlockBegin. In Maglev,
  // there is no such operation, and the exception is instead populated into the
  // accumulator by the throwing code, and is then loaded in Maglev through an
  // exception phi. When emitting a Turboshaft exception block, we thus store
  // the CatchBlockBegin in {catch_block_begin_}, which we then use when trying
  // to map the exception phi corresponding to the accumulator.
  V<Object> catch_block_begin_ = V<Object>::Invalid();

  // Maglev loops can have multiple forward edges, while Turboshaft should only
  // have a single one. When a Maglev loop has multiple forward edge, we create
  // an additional Turboshaft block before (which we record in
  // {loop_single_edge_predecessors_}), and jumps to the loop will instead go to
  // this additional block, which will become the only forward predecessor of
  // the loop.
  ZoneUnorderedMap<maglev::BasicBlock*, Block*> loop_single_edge_predecessors_;
  // When we create an additional loop predecessor for loops that have multiple
  // forward predecessors, we store the newly created phis in
  // {loop_phis_first_input_}, so that we can then use them as the first input
  // of the original loop phis. {loop_phis_first_input_index_} is used as an
  // index in {loop_phis_first_input_} in VisitPhi so that we know where to find
  // the first input for the current loop phi.
  base::SmallVector<OpIndex, 16> loop_phis_first_input_;
  int loop_phis_first_input_index_ = -1;

  // Magle doesn't have projections. Instead, after nodes that return multiple
  // values (currently, only maglev::ForInPrepare and maglev::CallBuiltin for
  // some builtins), Maglev inserts a GetSecondReturnedValue node, which
  // basically just binds kReturnRegister1 to a ValueNode. In the
  // Maglev->Turboshaft translation, when we emit a builtin call with multiple
  // return values, we set {second_return_value_} to the 2nd projection, and
  // then use it when translating GetSecondReturnedValue.
  V<Object> second_return_value_ = V<Object>::Invalid();

  V<NativeContext> native_context_ = V<NativeContext>::Invalid();
  V<Object> new_target_param_ = V<Object>::Invalid();
  base::SmallVector<int, 16> predecessor_permutation_;
};

// A NodeProcessor wrapper around GraphBuilder that takes care of skipping nodes
// when we are in Unreachable code.
class NodeProcessorBase : public GraphBuilder {
 public:
  using GraphBuilder::GraphBuilder;

  template <typename NodeT>
  maglev::ProcessResult Process(NodeT* node,
                                const maglev::ProcessingState& state) {
    if (GraphBuilder::Asm().generating_unreachable_operations()) {
      return maglev::ProcessResult::kRemove;
    } else {
      return GraphBuilder::Process(node, state);
    }
  }
};

namespace {
void PrintBytecode(PipelineData& data,
                   maglev::MaglevCompilationInfo* compilation_info) {
  DCHECK(data.info()->trace_turbo_graph());
  maglev::MaglevCompilationUnit* top_level_unit =
      compilation_info->toplevel_compilation_unit();
  CodeTracer* code_tracer = data.GetCodeTracer();
  CodeTracer::StreamScope tracing_scope(code_tracer);
  tracing_scope.stream()
      << "\n----- Bytecode before MaglevGraphBuilding -----\n"
      << std::endl;
  tracing_scope.stream() << "Function: "
                         << Brief(*compilation_info->toplevel_function())
                         << std::endl;
  BytecodeArray::Disassemble(top_level_unit->bytecode().object(),
                             tracing_scope.stream());
  Print(*top_level_unit->feedback().object(), tracing_scope.stream());
}

void PrintMaglevGraph(PipelineData& data,
                      maglev::MaglevCompilationInfo* compilation_info,
                      maglev::Graph* maglev_graph) {
  CodeTracer* code_tracer = data.GetCodeTracer();
  CodeTracer::StreamScope tracing_scope(code_tracer);
  tracing_scope.stream()
      << "\n----- Maglev graph after MaglevGraphBuilding -----" << std::endl;
  maglev::PrintGraph(tracing_scope.stream(), compilation_info, maglev_graph);
}
}  // namespace

void MaglevGraphBuildingPhase::Run(PipelineData* data, Zone* temp_zone) {
  JSHeapBroker* broker = data->broker();
  UnparkedScopeIfNeeded unparked_scope(broker);

  auto compilation_info = maglev::MaglevCompilationInfo::New(
      data->isolate(), broker, data->info()->closure(),
      data->info()->osr_offset());

  if (V8_UNLIKELY(data->info()->trace_turbo_graph())) {
    PrintBytecode(*data, compilation_info.get());
  }

  LocalIsolate* local_isolate = broker->local_isolate()
                                    ? broker->local_isolate()
                                    : broker->isolate()->AsLocalIsolate();
  maglev::Graph* maglev_graph =
      maglev::Graph::New(temp_zone, data->info()->is_osr());
  if (V8_UNLIKELY(data->info()->trace_turbo_graph())) {
    compilation_info->set_graph_labeller(new maglev::MaglevGraphLabeller());
  }
  maglev::MaglevGraphBuilder maglev_graph_builder(
      local_isolate, compilation_info->toplevel_compilation_unit(),
      maglev_graph);
  maglev_graph_builder.Build();

  // TODO(dmercadier, nicohartmann): remove the MaglevPhiRepresentationSelector
  // once we have representation selection / simplified lowering in Turboshaft.
  maglev::GraphProcessor<maglev::MaglevPhiRepresentationSelector>
      representation_selector(&maglev_graph_builder);
  representation_selector.ProcessGraph(maglev_graph);

  if (V8_UNLIKELY(data->info()->trace_turbo_graph())) {
    PrintMaglevGraph(*data, compilation_info.get(), maglev_graph);
  }

  // TODO(nicohartmann): Should we have source positions here?
  data->InitializeGraphComponent(nullptr);

  maglev::GraphProcessor<NodeProcessorBase, true> builder(
      data, data->graph(), temp_zone,
      compilation_info->toplevel_compilation_unit());
  builder.ProcessGraph(maglev_graph);
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
