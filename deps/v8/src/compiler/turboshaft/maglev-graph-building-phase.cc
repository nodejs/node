// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/maglev-graph-building-phase.h"
#include <type_traits>

#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/common/globals.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/globals.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/maglev-early-lowering-reducer-inl.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
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

class GraphBuilder {
 public:
  using AssemblerT =
      TSAssembler<MaglevEarlyLoweringReducer, MachineOptimizationReducer,
                  VariableReducer, RequiredOptimizationReducer,
                  ValueNumberingReducer>;

  GraphBuilder(Graph& graph, Zone* temp_zone,
               maglev::MaglevCompilationUnit* maglev_compilation_unit)
      : temp_zone_(temp_zone),
        assembler_(graph, graph, temp_zone),
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
  }

  void PostProcessGraph(maglev::Graph* graph) {}

  void PreProcessBasicBlock(maglev::BasicBlock* block) {
    if (__ current_block() != nullptr) {
      // The first block for Constants doesn't end with a Jump, so we add one
      // now.
      __ Goto(Map(block));
    }

#ifdef DEBUG
    loop_phis_first_input_.clear();
    loop_phis_first_input_index_ = -1;
#endif

    if (block->is_loop() &&
        MapContains(loop_single_edge_predecessors_, block)) {
      EmitLoopSinglePredecessorBlock(block);
    }

    __ Bind(Map(block));

    if (block->is_loop()) {
      // The "permutation" stuff that comes afterwards in this function doesn't
      // apply to loops, since loops always have 2 predecessors in Turboshaft,
      // and in both Turboshaft and Maglev, the backedge is always the last
      // predecessors, so we never need to reorder phi inputs.
      return;
    }
    // Because of edge splitting in Maglev (which happens on Bind rather than on
    // Goto), predecessors in the Maglev graph are not always ordered by their
    // position in the graph (ie, block 4 could be the second predecessor and
    // block 5 the first one). However, since we're processing the graph "in
    // order" (because that's how the maglev GraphProcessor works), predecessors
    // in the Turboshaft graph will be ordered by their position in the graph.
    // We thus compute in {predecessor_permutation_} the Turboshaft predecessors
    // position of each Maglev predecessor, and we'll use this later when
    // emitting Phis to reorder their inputs.
    predecessor_permutation_.clear();
    if (block->has_phi()) {
      for (int i = 0; i < block->predecessor_count(); ++i) {
        Block* pred = Map(block->predecessor_at(i));
        int pred_index = __ current_block() -> GetPredecessorIndex(pred);
        DCHECK_IMPLIES(pred_index == -1,
                       block->is_loop() && i == block->predecessor_count() - 1);
        predecessor_permutation_.push_back(pred_index);
      }
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
    SetMap(
        node,
        __ HeapConstant(
            MakeRef(broker_, node->DoReify(isolate_)).AsHeapObject().object()));
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
      SetMap(node, __ GetVariable(regs_to_vars_[node->owner().index()]));
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
      for (int i = 0; i < input_count; ++i) {
        inputs.push_back(Map(node->input(predecessor_permutation_[i])));
      }
      SetMap(node, __ Phi(base::VectorOf(inputs), rep));
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CallKnownJSFunction* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Object> callee = Map(node->closure());
    base::SmallVector<OpIndex, 16> arguments;
    arguments.push_back(Map(node->receiver()));
    for (int i = 0; i < node->num_args(); i++) {
      arguments.push_back(Map(node->arg(i)));
    }

    arguments.push_back(Map(node->new_target()));
    arguments.push_back(__ Word32Constant(JSParameterCount(node->num_args())));

    // Load the context from {callee}.
    OpIndex context =
        __ LoadField(callee, AccessBuilder::ForJSFunctionContext());
    arguments.push_back(context);

    const CallDescriptor* descriptor = Linkage::GetJSCallDescriptor(
        graph_zone(), false, 1 + node->num_args(),
        CallDescriptor::kNeedsFrameState | CallDescriptor::kCanUseRoots);
    SetMap(node, __ Call(V<CallTarget>::Cast(callee), frame_state,
                         base::VectorOf(arguments),
                         TSCallDescriptor::Create(descriptor, CanThrow::kYes,
                                                  graph_zone())));

    return maglev::ProcessResult::kContinue;
  }
  V<Any> GenerateBuiltinCall(maglev::NodeBase* node, Builtin builtin,
                             V<FrameState> frame_state,
                             base::Vector<const OpIndex> arguments) {
    ThrowingScope throwing_scope(this, node);

    Callable callable =
        Builtins::CallableFor(isolate_->GetMainThreadIsolateUnsafe(), builtin);
    const CallInterfaceDescriptor& descriptor = callable.descriptor();
    CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
        graph_zone(), descriptor, descriptor.GetStackParameterCount(),
        CallDescriptor::kNeedsFrameState);
    V<Code> stub_code = __ HeapConstant(callable.code());

    return __ Call(stub_code, frame_state, base::VectorOf(arguments),
                   TSCallDescriptor::Create(call_descriptor, CanThrow::kYes,
                                            graph_zone()));
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

    SetMap(node, GenerateBuiltinCall(node, node->builtin(), frame_state,
                                     base::VectorOf(arguments)));

    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::CallRuntime* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    auto c_entry_stub = __ CEntryStubConstant(
        isolate_->GetMainThreadIsolateUnsafe(), node->ReturnCount());

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

    OptionalOpIndex frame_state = OpIndex::Invalid();
    if (call_descriptor->NeedsFrameState()) {
      frame_state = BuildFrameState(node->lazy_deopt_info());
    }

    SetMap(node, __ Call(c_entry_stub, frame_state, base::VectorOf(arguments),
                         TSCallDescriptor::Create(
                             call_descriptor, CanThrow::kYes, graph_zone())));

    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ThrowReferenceErrorIfHole* node,
                                const maglev::ProcessingState& state) {
    IF (UNLIKELY(RootEqual(node->value(), RootIndex::kTheHoleValue))) {
      OpIndex frame_state = BuildFrameState(node->lazy_deopt_info());
      __ CallRuntime_ThrowAccessedUninitializedVariable(
          isolate_->GetMainThreadIsolateUnsafe(), frame_state, native_context(),
          __ HeapConstant(node->name().object()));
      // ThrowAccessedUninitializedVariable should not return.
      __ Unreachable();
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CreateFunctionContext* node,
                                const maglev::ProcessingState& state) {
    OpIndex frame_state = BuildFrameState(node->lazy_deopt_info());
    V<Context> context = Map(node->context());
    V<ScopeInfo> scope_info = __ HeapConstant(node->scope_info().object());
    if (node->scope_type() == FUNCTION_SCOPE) {
      SetMap(node, __ CallBuiltin_FastNewFunctionContextFunction(
                       isolate_->GetMainThreadIsolateUnsafe(), frame_state,
                       context, scope_info, node->slot_count()));
    } else {
      DCHECK_EQ(node->scope_type(), EVAL_SCOPE);
      SetMap(node, __ CallBuiltin_FastNewFunctionContextEval(
                       isolate_->GetMainThreadIsolateUnsafe(), frame_state,
                       context, scope_info, node->slot_count()));
    }
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Construct* node,
                                const maglev::ProcessingState& state) {
    ThrowingScope throwing_scope(this, node);

    V<FrameState> frame_state = BuildFrameState(node->lazy_deopt_info());
    Callable callable = Builtins::CallableFor(
        isolate_->GetMainThreadIsolateUnsafe(), Builtin::kConstruct);
    const CallInterfaceDescriptor& descriptor = callable.descriptor();
    CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
        graph_zone(), descriptor, node->num_args(),
        CallDescriptor::kNeedsFrameState);
    V<Code> stub_code = __ HeapConstant(callable.code());
    base::SmallVector<OpIndex, 16> arguments;

    arguments.push_back(Map(node->function()));
    arguments.push_back(Map(node->new_target()));
    arguments.push_back(__ Word32Constant(node->num_args()));

    for (auto arg : node->args()) {
      arguments.push_back(Map(arg));
    }

#ifdef DEBUG
    CallInterfaceDescriptor inter_descr =
        Builtins::CallInterfaceDescriptorFor(Builtin::kConstruct);
    DCHECK(inter_descr.HasContextParameter());
#endif

    arguments.push_back(Map(node->context()));

    SetMap(node, __ Call(stub_code, frame_state, base::VectorOf(arguments),
                         TSCallDescriptor::Create(
                             call_descriptor, CanThrow::kYes, graph_zone())));

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

  maglev::ProcessResult Process(maglev::CheckMaps* node,
                                const maglev::ProcessingState& state) {
    Label<> done(this);
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    if (node->check_type() == maglev::CheckType::kCheckHeapObject) {
      OpIndex is_smi = __ IsSmi(Map(node->receiver_input()));
      if (AnyMapIsHeapNumber(node->maps())) {
        // Smis count as matching the HeapNumber map, so we're done.
        GOTO_IF(is_smi, done);
      } else {
        __ DeoptimizeIf(is_smi, frame_state, DeoptimizeReason::kWrongMap,
                        node->eager_deopt_info()->feedback_to_update());
      }
    }
    __ CheckMaps(Map(node->receiver_input()), frame_state, node->maps(),
                 CheckMapsFlag::kNone,
                 node->eager_deopt_info()->feedback_to_update());

    if (done.has_incoming_jump()) {
      GOTO(done);
      BIND(done);
    }
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
    // TODO(dmercadier): is there no higher level way of doing this?
    V<Word32> input = Map<Word32>(node->input());
    V<Tuple<Word32, Word32>> add = __ Int32AddCheckOverflow(input, input);
    V<Word32> check = __ template Projection<1>(add);
    V<FrameState> frame_state = BuildFrameState(node->eager_deopt_info());
    __ DeoptimizeIf(check, frame_state, DeoptimizeReason::kNotASmi,
                    node->eager_deopt_info()->feedback_to_update());
    SetMap(node, input);
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
      if (alloc->HasEscaped()) {
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
    if (!node->HasEscaped()) return maglev::ProcessResult::kRemove;
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

  maglev::ProcessResult Process(maglev::ArgumentsLength* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ArgumentsLength());
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ArgumentsElements* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ NewArgumentsElements(Map(node->arguments_count_input()),
                                         node->type(),
                                         node->formal_parameter_count()));
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
    V<Word32> condition = __ Float64Equal(Map(node->condition_input()), 0.0);
    // Swapping if_true and if_false because we the real condition is "!= 0"
    // rather than "== 0" (but Turboshaft doesn't have Float64NotEqual).
    __ Branch(condition, Map(node->if_false()), Map(node->if_true()));
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

  maglev::ProcessResult Process(maglev::Float64Negate* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Float64Negate(Map(node->input())));
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
        Map(node->value()), __ HeapConstant(factory_->true_value()));
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

  maglev::ProcessResult Process(maglev::Int32ToBoolean* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, ConvertWord32ToJSBool(Map(node->value()), node->flip()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::Float64ToBoolean* node,
                                const maglev::ProcessingState& state) {
    V<Word32> condition = __ Float64Equal(Map(node->value()), 0.0);
    // {condition} is 0 if the input is truthy, and false otherwise (because we
    // compared "== 0" rather than "!= 0"), so we need to negate `flip` in the
    // call to ConvertWord32ToJSBool.
    SetMap(node, ConvertWord32ToJSBool(condition, !node->flip()));
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

  maglev::ProcessResult Process(maglev::CheckedNumberOrOddballToFloat64* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ ConvertJSPrimitiveToUntaggedOrDeopt(
               Map(node->input()), BuildFrameState(node->eager_deopt_info()),
               ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::
                   kNumberOrOddball,
               ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kFloat64,
               CheckForMinusZeroMode::kCheckForMinusZero,
               node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::UncheckedNumberOrOddballToFloat64* node,
                                const maglev::ProcessingState& state) {
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
  maglev::ProcessResult Process(maglev::CheckedTruncateFloat64ToInt32* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ ChangeFloat64ToInt32OrDeopt(
               Map(node->input()), BuildFrameState(node->eager_deopt_info()),
               CheckForMinusZeroMode::kCheckForMinusZero,
               node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::HoleyFloat64ToMaybeNanFloat64* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ Float64SilenceNaN(Map(node->input())));
    return maglev::ProcessResult::kContinue;
  }
  maglev::ProcessResult Process(maglev::ConvertHoleToUndefined* node,
                                const maglev::ProcessingState& state) {
    V<Word32> cond = RootEqual(node->object_input(), RootIndex::kTheHoleValue);
    SetMap(node, __ Select(cond, __ HeapConstant(factory_->undefined_value()),
                           Map(node->object_input()),
                           RegisterRepresentation::Tagged(), BranchHint::kNone,
                           SelectOp::Implementation::kBranch));
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
    __ StoreMessage(
        __ ExternalConstant(
            ExternalReference::address_of_pending_message(isolate_)),
        Map(node->value()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Identity* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, Map(node->input(0)));
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
    // No need to update the interrupt budget once we reach Turboshaft. However,
    // we still need to emit a StackCheck to handle interrupt requests.
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
    // TODO(dmercadier): handle other FrameTypes.
    DCHECK_EQ(eager_deopt_info->top_frame().type(),
              maglev::DeoptFrame::FrameType::kInterpretedFrame);
    maglev::InterpretedDeoptFrame& frame =
        eager_deopt_info->top_frame().as_interpreted();
    return BuildFrameState(frame, OutputFrameStateCombine::Ignore(), true);
  }

  V<FrameState> BuildFrameState(maglev::LazyDeoptInfo* lazy_deopt_info) {
    DCHECK_EQ(lazy_deopt_info->top_frame().type(),
              maglev::DeoptFrame::FrameType::kInterpretedFrame);
    maglev::InterpretedDeoptFrame& frame =
        lazy_deopt_info->top_frame().as_interpreted();
    if (lazy_deopt_info->result_size() == 1) {
      DCHECK_EQ(lazy_deopt_info->result_location(),
                interpreter::Register::virtual_accumulator());
      return BuildFrameState(frame, OutputFrameStateCombine::PokeAt(0), true);
    } else if (lazy_deopt_info->result_size() == 0) {
      return BuildFrameState(frame, OutputFrameStateCombine::Ignore(), true);
    } else {
      // TODO(dmercadier): handle cases where the lazy deopt has multiple return
      // values.
      UNIMPLEMENTED();
    }
  }

  V<FrameState> BuildFrameState(maglev::InterpretedDeoptFrame& frame,
                                OutputFrameStateCombine combine,
                                bool is_topmost) {
    FrameStateData::Builder builder;

    if (frame.parent() != nullptr) {
      DCHECK_EQ(frame.parent()->type(),
                maglev::DeoptFrame::FrameType::kInterpretedFrame);
      OpIndex parent_frame =
          BuildFrameState(frame.parent()->as_interpreted(), combine, false);
      builder.AddParentFrameState(parent_frame);
    }

    // Closure
    AddDeoptInput(builder, frame.closure());

    // Parameters
    frame.frame_state()->ForEachParameter(
        frame.unit(), [&](maglev::ValueNode* value, interpreter::Register reg) {
          AddDeoptInput(builder, value);
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
          AddDeoptInput(builder, value);
          local_index++;
        });
    for (; local_index < frame.unit().register_count(); local_index++) {
      builder.AddUnusedRegister();
    }

    // Accumulator
    // When `combine` is PokeAt(0), the value of the accumulator is produced by
    // the function that lazy deopts, and thus it shouldn't be in the
    // FrameState (since the FrameState is defined before the function is
    // called), so in that case we set the Accumulator to UnusedRegister, even
    // if it is alive.
    // TODO(dmercadier): the "combine != PokeAt(0)" is fine for now since we
    // don't have PokeAt with other values than 0 for now, but once we do, we
    // should use instead something similar to InReturnValues in Maglev.
    if (is_topmost && frame.frame_state()->liveness()->AccumulatorIsLive() &&
        combine != OutputFrameStateCombine::PokeAt(0)) {
      AddDeoptInput(builder, frame.frame_state()->accumulator(frame.unit()));
    } else {
      builder.AddUnusedRegister();
    }

    const FrameStateInfo* frame_state_info = MakeFrameStateInfo(frame, combine);
    return __ FrameState(
        builder.Inputs(), builder.inlined(),
        builder.AllocateFrameStateData(*frame_state_info, graph_zone()));
  }

  void AddDeoptInput(FrameStateData::Builder& builder,
                     maglev::ValueNode* node) {
    builder.AddInput(MachineTypeFor(node->value_representation()), Map(node));
  }

  const FrameStateInfo* MakeFrameStateInfo(
      maglev::InterpretedDeoptFrame& maglev_frame,
      OutputFrameStateCombine combine) {
    FrameStateType type = FrameStateType::kUnoptimizedFunction;
    int parameter_count = maglev_frame.unit().parameter_count();
    int local_count = maglev_frame.unit().register_count();
    Handle<SharedFunctionInfo> shared_info =
        maglev_frame.unit().shared_function_info().object();
    FrameStateFunctionInfo* info = graph_zone()->New<FrameStateFunctionInfo>(
        type, parameter_count, local_count, shared_info);

    return graph_zone()->New<FrameStateInfo>(maglev_frame.bytecode_position(),
                                             combine, info);
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
    return __ TaggedEqual(
        Map(input),
        __ HeapConstant(Handle<HeapObject>::cast(isolate_->root_handle(root))));
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
  V<Object> ConvertWord32ToJSBool(V<Word32> b, bool flip = false) {
    V<Boolean> true_idx = __ HeapConstant(factory_->true_value());
    V<Boolean> false_idx = __ HeapConstant(factory_->false_value());
    if (flip) std::swap(true_idx, false_idx);
    return __ Select(b, true_idx, false_idx, RegisterRepresentation::Tagged(),
                     BranchHint::kNone, SelectOp::Implementation::kBranch);
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
      DCHECK(throwing_node->properties().can_throw());
      const maglev::ExceptionHandlerInfo* info =
          throwing_node->exception_handler_info();
      if (!info->HasExceptionHandler()) return;

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
    node_mapping_[node] = idx;
    return idx;
  }

  V<Context> native_context() {
    if (!native_context_.valid()) {
      native_context_ =
          __ HeapConstant(broker_->target_native_context().object());
    }
    return native_context_;
  }

  Zone* temp_zone_;
  LocalIsolate* isolate_ = PipelineData::Get().isolate()->AsLocalIsolate();
  JSHeapBroker* broker_ = PipelineData::Get().broker();
  LocalFactory* factory_ = isolate_->factory();
  AssemblerT assembler_;
  maglev::MaglevCompilationUnit* maglev_compilation_unit_;
  ZoneUnorderedMap<const maglev::NodeBase*, OpIndex> node_mapping_;
  ZoneUnorderedMap<const maglev::BasicBlock*, Block*> block_mapping_;
  ZoneUnorderedMap<int, Variable> regs_to_vars_;

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

  V<Context> native_context_ = OpIndex::Invalid();
  V<Object> new_target_param_ = OpIndex::Invalid();
  base::SmallVector<int, 16> predecessor_permutation_;
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

void MaglevGraphBuildingPhase::Run(Zone* temp_zone) {
  PipelineData& data = PipelineData::Get();
  JSHeapBroker* broker = data.broker();
  UnparkedScopeIfNeeded unparked_scope(broker);

  auto compilation_info = maglev::MaglevCompilationInfo::New(
      data.isolate(), broker, data.info()->closure(),
      data.info()->osr_offset());

  if (V8_UNLIKELY(data.info()->trace_turbo_graph())) {
    PrintBytecode(data, compilation_info.get());
  }

  LocalIsolate* local_isolate = broker->local_isolate()
                                    ? broker->local_isolate()
                                    : broker->isolate()->AsLocalIsolate();
  maglev::Graph* maglev_graph =
      maglev::Graph::New(temp_zone, data.info()->is_osr());
  if (V8_UNLIKELY(data.info()->trace_turbo_graph())) {
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

  if (V8_UNLIKELY(data.info()->trace_turbo_graph())) {
    PrintMaglevGraph(data, compilation_info.get(), maglev_graph);
  }

  maglev::GraphProcessor<GraphBuilder, true> builder(
      data.graph(), temp_zone, compilation_info->toplevel_compilation_unit());
  builder.ProcessGraph(maglev_graph);
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
