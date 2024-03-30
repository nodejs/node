// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/maglev-graph-building-phase.h"

#include "src/compiler/turboshaft/assembler.h"
#include "src/handles/global-handles-inl.h"
#include "src/handles/handles.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-builder.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

class GraphBuilder {
 public:
  using Assembler = TSAssembler<>;

  GraphBuilder(Graph& graph, Zone* temp_zone)
      : temp_zone_(temp_zone),
        assembler_(graph, graph, temp_zone),
        node_mapping_(temp_zone),
        block_mapping_(temp_zone) {}

  void PreProcessGraph(maglev::Graph* graph) {
    for (maglev::BasicBlock* block : *graph) {
      block_mapping_[block] =
          block->is_loop() ? __ NewLoopHeader() : __ NewBlock();
    }
    // Constants are not in a block in Maglev but are in Turboshaft. We bind a
    // block now, so that Constants can then be emitted.
    __ Bind(__ NewBlock());
  }

  void PostProcessGraph(maglev::Graph* graph) {}

  void PreProcessBasicBlock(maglev::BasicBlock* block) {
    if (__ current_block() != nullptr) {
      // The first block for Constants doesn't end with a Jump, so we add one
      // now.
      __ Goto(Map(block));
    }
    __ Bind(Map(block));
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

  maglev::ProcessResult Process(maglev::InitialValue* node,
                                const maglev::ProcessingState& state) {
#ifdef DEBUG
    char* debug_name = strdup(node->source().ToString().c_str());
#else
    char* debug_name = nullptr;
#endif
    SetMap(node, __ Parameter(node->source().ToParameterIndex(),
                              RegisterRepresentation::Tagged(), debug_name));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::FunctionEntryStackCheck* node,
                                const maglev::ProcessingState& state) {
    __ StackCheck(StackCheckOp::CheckOrigin::kFromJS,
                  StackCheckOp::CheckKind::kFunctionHeaderCheck);
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Jump* node,
                                const maglev::ProcessingState& state) {
    __ Goto(Map(node->target()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::CheckedSmiUntag* node,
                                const maglev::ProcessingState& state) {
    SetMap(node,
           __ CheckedSmiUntag(Map(node->input().node()),
                              BuildFrameState(node->eager_deopt_info()),
                              node->eager_deopt_info()->feedback_to_update()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Int32AddWithOverflow* node,
                                const maglev::ProcessingState& state) {
    OpIndex add_and_overflow = __ Int32AddCheckOverflow(
        Map(node->left_input().node()), Map(node->right_input().node()));
    __ DeoptimizeIf(
        __ Projection(add_and_overflow, 1, RegisterRepresentation::Word32()),
        BuildFrameState(node->eager_deopt_info()),
        node->eager_deopt_info()->reason(),
        node->eager_deopt_info()->feedback_to_update());
    SetMap(node, __ Projection(add_and_overflow, 0,
                               RegisterRepresentation::Word32()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Int32ToNumber* node,
                                const maglev::ProcessingState& state) {
    SetMap(node, __ ConvertInt32ToNumber(Map(node->input().node())));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::Return* node,
                                const maglev::ProcessingState& state) {
    __ Return(Map(node->value_input().node()));
    return maglev::ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::ReduceInterruptBudgetForReturn*,
                                const maglev::ProcessingState&) {
    // No need to update the interrupt budget once we reach Turboshaft.
    return maglev::ProcessResult::kContinue;
  }

  template <typename NodeT>
  maglev::ProcessResult Process(NodeT* node,
                                const maglev::ProcessingState& state) {
    UNIMPLEMENTED();
  }

  Assembler& Asm() { return assembler_; }
  Zone* temp_zone() { return temp_zone_; }
  Zone* graph_zone() { return Asm().output_graph().graph_zone(); }

 private:
  OpIndex BuildFrameState(maglev::EagerDeoptInfo* eager_deopt_info) {
    DCHECK_EQ(eager_deopt_info->top_frame().type(),
              maglev::DeoptFrame::FrameType::kInterpretedFrame);
    maglev::InterpretedDeoptFrame& frame =
        eager_deopt_info->top_frame().as_interpreted();
    FrameStateData::Builder builder;
    if (eager_deopt_info->top_frame().parent() != nullptr) {
      // TODO(dmercadier): do something about inlining.
      UNIMPLEMENTED();
    }

    // Closure
    builder.AddInput(MachineType::AnyTagged(), Map(frame.closure()));

    // Parameters
    frame.frame_state()->ForEachParameter(
        frame.unit(), [&](maglev::ValueNode* value, interpreter::Register reg) {
          builder.AddInput(MachineType::AnyTagged(), Map(value));
        });

    // Context
    builder.AddInput(MachineType::AnyTagged(),
                     Map(frame.frame_state()->context(frame.unit())));

    // The accumulator should be included both in the locals and the "stack"
    // input.
    if (frame.frame_state()->liveness()->AccumulatorIsLive()) {
      builder.AddInput(MachineType::AnyTagged(),
                       Map(frame.frame_state()->accumulator(frame.unit())));
    } else {
      // TODO(dmercadier): should we add an unused register or nothing here?
      builder.AddUnusedRegister();
    }

    // Locals
    // note that ForEachLocal skips the accumulator.
    frame.frame_state()->ForEachLocal(
        frame.unit(), [&](maglev::ValueNode* value, interpreter::Register reg) {
          builder.AddInput(MachineType::AnyTagged(), Map(value));
        });

    // Accumulator
    if (frame.frame_state()->liveness()->AccumulatorIsLive()) {
      builder.AddInput(MachineType::AnyTagged(),
                       Map(frame.frame_state()->accumulator(frame.unit())));
    } else {
      builder.AddUnusedRegister();
    }

    const FrameStateInfo* frame_state_info = MakeFrameStateInfo(frame);
    return __ FrameState(
        builder.Inputs(), builder.inlined(),
        builder.AllocateFrameStateData(*frame_state_info, graph_zone()));
  }

  const FrameStateInfo* MakeFrameStateInfo(
      maglev::InterpretedDeoptFrame& maglev_frame) {
    FrameStateType type = FrameStateType::kUnoptimizedFunction;
    int parameter_count = maglev_frame.unit().parameter_count();
    int local_count =
        maglev_frame.frame_state()->liveness()->live_value_count();
    Handle<SharedFunctionInfo> shared_info =
        PipelineData::Get().info()->shared_info();
    FrameStateFunctionInfo* info = graph_zone()->New<FrameStateFunctionInfo>(
        type, parameter_count, local_count, shared_info);

    return graph_zone()->New<FrameStateInfo>(maglev_frame.bytecode_position(),
                                             OutputFrameStateCombine::Ignore(),
                                             info);
  }

  OpIndex Map(const maglev::NodeBase* node) { return node_mapping_[node]; }
  Block* Map(const maglev::BasicBlock* block) { return block_mapping_[block]; }

  OpIndex SetMap(maglev::NodeBase* node, OpIndex idx) {
    node_mapping_[node] = idx;
    return idx;
  }

  Zone* temp_zone_;
  LocalIsolate* isolate_ = PipelineData::Get().isolate()->AsLocalIsolate();
  JSHeapBroker* broker_ = PipelineData::Get().broker();
  Assembler assembler_;
  ZoneUnorderedMap<const maglev::NodeBase*, OpIndex> node_mapping_;
  ZoneUnorderedMap<const maglev::BasicBlock*, Block*> block_mapping_;
};

void MaglevGraphBuildingPhase::Run(Zone* temp_zone) {
  PipelineData& data = PipelineData::Get();
  UnparkedScopeIfNeeded unparked_scope(data.broker());

  auto compilation_info = maglev::MaglevCompilationInfo::New(
      data.isolate(), data.broker(), data.info()->closure(),
      data.info()->osr_offset());

  maglev::Graph* maglev_graph =
      maglev::Graph::New(temp_zone, data.info()->is_osr());
  maglev::MaglevGraphBuilder maglev_graph_builder(
      data.isolate()->AsLocalIsolate(),
      compilation_info->toplevel_compilation_unit(), maglev_graph);
  maglev_graph_builder.Build();

  maglev::GraphProcessor<GraphBuilder, true> builder(data.graph(), temp_zone);
  builder.ProcessGraph(maglev_graph);
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
