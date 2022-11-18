// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-compiler.h"

#include <iomanip>
#include <ostream>
#include <type_traits>

#include "src/base/iterator.h"
#include "src/base/logging.h"
#include "src/base/threaded-list.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/deoptimizer/translation-array.h"
#include "src/execution/frames.h"
#include "src/ic/handler-configuration.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-generator.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph-verifier.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc.h"
#include "src/maglev/maglev-vreg-allocator.h"
#include "src/objects/code-inl.h"
#include "src/objects/js-function.h"
#include "src/utils/identity-map.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

class UseMarkingProcessor {
 public:
  explicit UseMarkingProcessor(MaglevCompilationInfo* compilation_info)
      : compilation_info_(compilation_info) {}

  void PreProcessGraph(Graph* graph) { next_node_id_ = kFirstValidNodeId; }
  void PostProcessGraph(Graph* graph) { DCHECK(loop_used_nodes_.empty()); }
  void PreProcessBasicBlock(BasicBlock* block) {
    if (!block->has_state()) return;
    if (block->state()->is_loop()) {
      loop_used_nodes_.push_back(LoopUsedNodes{next_node_id_, {}});
#ifdef DEBUG
      loop_used_nodes_.back().header = block;
#endif
    }
  }

  template <typename NodeT>
  void Process(NodeT* node, const ProcessingState& state) {
    node->set_id(next_node_id_++);
    MarkInputUses(node, state);
  }

  template <typename NodeT>
  void MarkInputUses(NodeT* node, const ProcessingState& state) {
    LoopUsedNodes* loop_used_nodes = GetCurrentLoopUsedNodes();
    if constexpr (NodeT::kProperties.can_eager_deopt()) {
      MarkCheckpointNodes(node, node->eager_deopt_info(), loop_used_nodes,
                          state);
    }
    for (Input& input : *node) {
      MarkUse(input.node(), node->id(), &input, loop_used_nodes);
    }
    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      MarkCheckpointNodes(node, node->lazy_deopt_info(), loop_used_nodes,
                          state);
    }
  }

  void MarkInputUses(Phi* node, const ProcessingState& state) {
    // Don't mark Phi uses when visiting the node, because of loop phis.
    // Instead, they'll be visited while processing Jump/JumpLoop.
  }

  // Specialize the two unconditional jumps to extend their Phis' inputs' live
  // ranges.

  void MarkInputUses(JumpLoop* node, const ProcessingState& state) {
    int i = state.block()->predecessor_id();
    BasicBlock* target = node->target();
    uint32_t use = node->id();

    if (target->has_phi()) {
      // Phis are potential users of nodes outside this loop, but only on
      // initial loop entry, not on actual looping, so we don't need to record
      // their other inputs for lifetime extension.
      for (Phi* phi : *target->phis()) {
        ValueNode* input = phi->input(i).node();
        input->mark_use(use, &phi->input(i));
      }
    }

    DCHECK(!loop_used_nodes_.empty());
    LoopUsedNodes loop_used_nodes = std::move(loop_used_nodes_.back());
    loop_used_nodes_.pop_back();

    DCHECK_EQ(loop_used_nodes.header, target);
    if (!loop_used_nodes.used_nodes.empty()) {
      // Uses of nodes in this loop may need to propagate to an outer loop, so
      // that they're lifetime is extended there too.
      // TODO(leszeks): We only need to extend the lifetime in one outermost
      // loop, allow nodes to be "moved" between lifetime extensions.
      LoopUsedNodes* outer_loop_used_nodes = GetCurrentLoopUsedNodes();
      base::Vector<Input> used_node_inputs =
          compilation_info_->zone()->NewVector<Input>(
              loop_used_nodes.used_nodes.size());
      int i = 0;
      for (ValueNode* used_node : loop_used_nodes.used_nodes) {
        Input* input = new (&used_node_inputs[i++]) Input(used_node);
        MarkUse(used_node, use, input, outer_loop_used_nodes);
      }
      node->set_used_nodes(used_node_inputs);
    }
  }
  void MarkInputUses(Jump* node, const ProcessingState& state) {
    int i = state.block()->predecessor_id();
    BasicBlock* target = node->target();
    if (!target->has_phi()) return;
    uint32_t use = node->id();
    LoopUsedNodes* loop_used_nodes = GetCurrentLoopUsedNodes();
    for (Phi* phi : *target->phis()) {
      ValueNode* input = phi->input(i).node();
      MarkUse(input, use, &phi->input(i), loop_used_nodes);
    }
  }

 private:
  struct LoopUsedNodes {
    uint32_t loop_header_id;
    std::unordered_set<ValueNode*> used_nodes;
#ifdef DEBUG
    BasicBlock* header = nullptr;
#endif
  };

  LoopUsedNodes* GetCurrentLoopUsedNodes() {
    if (loop_used_nodes_.empty()) return nullptr;
    return &loop_used_nodes_.back();
  }

  void MarkUse(ValueNode* node, uint32_t use_id, InputLocation* input,
               LoopUsedNodes* loop_used_nodes) {
    node->mark_use(use_id, input);

    // If we are in a loop, loop_used_nodes is non-null. In this case, check if
    // the incoming node is from outside the loop, and make sure to extend its
    // lifetime to the loop end if yes.
    if (loop_used_nodes) {
      // If the node's id is smaller than the smallest id inside the loop, then
      // it must have been created before the loop. This means that it's alive
      // on loop entry, and therefore has to be alive across the loop back edge
      // too.
      if (node->id() < loop_used_nodes->loop_header_id) {
        loop_used_nodes->used_nodes.insert(node);
      }
    }
  }

  void MarkCheckpointNodes(NodeBase* node, const EagerDeoptInfo* deopt_info,
                           LoopUsedNodes* loop_used_nodes,
                           const ProcessingState& state) {
    int use_id = node->id();
    detail::DeepForEachInput(
        deopt_info,
        [&](ValueNode* node, interpreter::Register reg, InputLocation* input) {
          MarkUse(node, use_id, input, loop_used_nodes);
        });
  }
  void MarkCheckpointNodes(NodeBase* node, const LazyDeoptInfo* deopt_info,
                           LoopUsedNodes* loop_used_nodes,
                           const ProcessingState& state) {
    int use_id = node->id();
    detail::DeepForEachInput(
        deopt_info,
        [&](ValueNode* node, interpreter::Register reg, InputLocation* input) {
          MarkUse(node, use_id, input, loop_used_nodes);
        });
  }

  MaglevCompilationInfo* compilation_info_;
  uint32_t next_node_id_;
  std::vector<LoopUsedNodes> loop_used_nodes_;
};

class TranslationArrayProcessor {
 public:
  explicit TranslationArrayProcessor(LocalIsolate* local_isolate,
                                     MaglevCompilationInfo* compilation_info)
      : local_isolate_(local_isolate), compilation_info_(compilation_info) {}

  void PreProcessGraph(Graph* graph) {
    translation_array_builder_.reset(
        new TranslationArrayBuilder(compilation_info_->zone()));
    deopt_literals_.reset(new IdentityMap<int, base::DefaultAllocationPolicy>(
        local_isolate_->heap()->heap()));

    tagged_slots_ = graph->tagged_stack_slots();
  }

  void PostProcessGraph(Graph* graph) {
    compilation_info_->set_translation_array_builder(
        std::move(translation_array_builder_), std::move(deopt_literals_));
  }
  void PreProcessBasicBlock(BasicBlock* block) {}

  void Process(NodeBase* node, const ProcessingState& state) {
    if (node->properties().can_eager_deopt()) {
      EmitEagerDeopt(node->eager_deopt_info());
    }
    if (node->properties().can_lazy_deopt()) {
      EmitLazyDeopt(node->lazy_deopt_info());
    }
  }

 private:
  void EmitDeoptFrame(const MaglevCompilationUnit& unit,
                      const CheckpointedInterpreterState& state,
                      const InputLocation* input_locations) {
    if (state.parent) {
      // Deopt input locations are in the order of deopt frame emission, so
      // update the pointer after emitting the parent frame.
      EmitDeoptFrame(*unit.caller(), *state.parent, input_locations);
    }

    // Returns are used for updating an accumulator or register after a lazy
    // deopt.
    const int return_offset = 0;
    const int return_count = 0;
    translation_array_builder().BeginInterpretedFrame(
        state.bytecode_position,
        GetDeoptLiteral(*unit.shared_function_info().object()),
        unit.register_count(), return_offset, return_count);

    EmitDeoptFrameValues(unit, state.register_frame, input_locations,
                         interpreter::Register::invalid_value(), return_count);
  }

  void EmitEagerDeopt(EagerDeoptInfo* deopt_info) {
    int frame_count = 1 + deopt_info->unit.inlining_depth();
    int jsframe_count = frame_count;
    int update_feedback_count = 0;
    deopt_info->translation_index =
        translation_array_builder().BeginTranslation(frame_count, jsframe_count,
                                                     update_feedback_count);

    EmitDeoptFrame(deopt_info->unit, deopt_info->state,
                   deopt_info->input_locations);
  }

  void EmitLazyDeopt(LazyDeoptInfo* deopt_info) {
    int frame_count = 1 + deopt_info->unit.inlining_depth();
    int jsframe_count = frame_count;
    int update_feedback_count = 0;
    deopt_info->translation_index =
        translation_array_builder().BeginTranslation(frame_count, jsframe_count,
                                                     update_feedback_count);

    const MaglevCompilationUnit& unit = deopt_info->unit;
    const InputLocation* input_locations = deopt_info->input_locations;

    if (deopt_info->state.parent) {
      // Deopt input locations are in the order of deopt frame emission, so
      // update the pointer after emitting the parent frame.
      EmitDeoptFrame(*unit.caller(), *deopt_info->state.parent,
                     input_locations);
    }

    // Return offsets are counted from the end of the translation frame, which
    // is the array [parameters..., locals..., accumulator]. Since it's the end,
    // we don't need to worry about earlier frames.
    int return_offset;
    if (deopt_info->result_location ==
        interpreter::Register::virtual_accumulator()) {
      return_offset = 0;
    } else if (deopt_info->result_location.is_parameter()) {
      // This is slightly tricky to reason about because of zero indexing and
      // fence post errors. As an example, consider a frame with 2 locals and
      // 2 parameters, where we want argument index 1 -- looking at the array
      // in reverse order we have:
      //   [acc, r1, r0, a1, a0]
      //                  ^
      // and this calculation gives, correctly:
      //   2 + 2 - 1 = 3
      return_offset = unit.register_count() + unit.parameter_count() -
                      deopt_info->result_location.ToParameterIndex();
    } else {
      return_offset =
          unit.register_count() - deopt_info->result_location.index();
    }
    translation_array_builder().BeginInterpretedFrame(
        deopt_info->state.bytecode_position,
        GetDeoptLiteral(*unit.shared_function_info().object()),
        unit.register_count(), return_offset, deopt_info->result_size);

    EmitDeoptFrameValues(unit, deopt_info->state.register_frame,
                         input_locations, deopt_info->result_location,
                         deopt_info->result_size);
  }

  void EmitDeoptStoreRegister(const compiler::AllocatedOperand& operand,
                              ValueRepresentation repr) {
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder().StoreRegister(operand.GetRegister());
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder().StoreInt32Register(operand.GetRegister());
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder().StoreDoubleRegister(
            operand.GetDoubleRegister());
        break;
    }
  }

  void EmitDeoptStoreStackSlot(const compiler::AllocatedOperand& operand,
                               ValueRepresentation repr) {
    int stack_slot = DeoptStackSlotFromStackSlot(operand);
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder().StoreStackSlot(stack_slot);
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder().StoreInt32StackSlot(stack_slot);
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder().StoreDoubleStackSlot(stack_slot);
        break;
    }
  }

  void EmitDeoptFrameSingleValue(ValueNode* value,
                                 const InputLocation& input_location) {
    if (input_location.operand().IsConstant()) {
      translation_array_builder().StoreLiteral(
          GetDeoptLiteral(*value->Reify(local_isolate_)));
    } else {
      const compiler::AllocatedOperand& operand =
          compiler::AllocatedOperand::cast(input_location.operand());
      ValueRepresentation repr = value->properties().value_representation();
      if (operand.IsAnyRegister()) {
        EmitDeoptStoreRegister(operand, repr);
      } else {
        EmitDeoptStoreStackSlot(operand, repr);
      }
    }
  }

  constexpr int DeoptStackSlotIndexFromFPOffset(int offset) {
    return 1 - offset / kSystemPointerSize;
  }

  inline int GetFramePointerOffsetForStackSlot(
      const compiler::AllocatedOperand& operand) {
    int index = operand.index();
    if (operand.representation() != MachineRepresentation::kTagged) {
      index += tagged_slots_;
    }
    return GetFramePointerOffsetForStackSlot(index);
  }

  inline constexpr int GetFramePointerOffsetForStackSlot(int index) {
    return StandardFrameConstants::kExpressionsOffset -
           index * kSystemPointerSize;
  }

  int DeoptStackSlotFromStackSlot(const compiler::AllocatedOperand& operand) {
    return DeoptStackSlotIndexFromFPOffset(
        GetFramePointerOffsetForStackSlot(operand));
  }

  bool InReturnValues(interpreter::Register reg,
                      interpreter::Register result_location, int result_size) {
    if (result_size == 0 || !result_location.is_valid()) {
      return false;
    }
    return base::IsInRange(reg.index(), result_location.index(),
                           result_location.index() + result_size - 1);
  }

  void EmitDeoptFrameValues(
      const MaglevCompilationUnit& compilation_unit,
      const CompactInterpreterFrameState* checkpoint_state,
      const InputLocation*& input_location,
      interpreter::Register result_location, int result_size) {
    // Closure
    if (compilation_unit.inlining_depth() == 0) {
      int closure_index = DeoptStackSlotIndexFromFPOffset(
          StandardFrameConstants::kFunctionOffset);
      translation_array_builder().StoreStackSlot(closure_index);
    } else {
      translation_array_builder().StoreLiteral(
          GetDeoptLiteral(*compilation_unit.function().object()));
    }

    // TODO(leszeks): The input locations array happens to be in the same order
    // as parameters+context+locals+accumulator are accessed here. We should
    // make this clearer and guard against this invariant failing.

    // Parameters
    {
      int i = 0;
      checkpoint_state->ForEachParameter(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_EQ(reg.ToParameterIndex(), i);
            if (InReturnValues(reg, result_location, result_size)) {
              translation_array_builder().StoreOptimizedOut();
            } else {
              EmitDeoptFrameSingleValue(value, *input_location);
              input_location++;
            }
            i++;
          });
    }

    // Context
    ValueNode* value = checkpoint_state->context(compilation_unit);
    EmitDeoptFrameSingleValue(value, *input_location);
    input_location++;

    // Locals
    {
      int i = 0;
      checkpoint_state->ForEachLocal(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_LE(i, reg.index());
            if (InReturnValues(reg, result_location, result_size)) return;
            while (i < reg.index()) {
              translation_array_builder().StoreOptimizedOut();
              i++;
            }
            DCHECK_EQ(i, reg.index());
            EmitDeoptFrameSingleValue(value, *input_location);
            input_location++;
            i++;
          });
      while (i < compilation_unit.register_count()) {
        translation_array_builder().StoreOptimizedOut();
        i++;
      }
    }

    // Accumulator
    {
      if (checkpoint_state->liveness()->AccumulatorIsLive() &&
          !InReturnValues(interpreter::Register::virtual_accumulator(),
                          result_location, result_size)) {
        ValueNode* value = checkpoint_state->accumulator(compilation_unit);
        EmitDeoptFrameSingleValue(value, *input_location);
        input_location++;
      } else {
        translation_array_builder().StoreOptimizedOut();
      }
    }
  }

  int GetDeoptLiteral(Object obj) {
    IdentityMapFindResult<int> res = deopt_literals_->FindOrInsert(obj);
    if (!res.already_exists) {
      DCHECK_EQ(0, *res.entry);
      *res.entry = deopt_literals_->size() - 1;
    }
    return *res.entry;
  }

  TranslationArrayBuilder& translation_array_builder() {
    return *translation_array_builder_;
  }

  LocalIsolate* local_isolate_;
  MaglevCompilationInfo* compilation_info_;
  std::unique_ptr<TranslationArrayBuilder> translation_array_builder_;
  std::unique_ptr<IdentityMap<int, base::DefaultAllocationPolicy>>
      deopt_literals_;
  int tagged_slots_;
};

// static
void MaglevCompiler::Compile(LocalIsolate* local_isolate,
                             MaglevCompilationInfo* compilation_info) {
  compiler::UnparkedScopeIfNeeded unparked_scope(compilation_info->broker());

  // Build graph.
  if (v8_flags.print_maglev_code || v8_flags.code_comments ||
      v8_flags.print_maglev_graph || v8_flags.trace_maglev_graph_building ||
      v8_flags.trace_maglev_regalloc) {
    compilation_info->set_graph_labeller(new MaglevGraphLabeller());
  }

  if (v8_flags.print_maglev_code || v8_flags.print_maglev_graph ||
      v8_flags.trace_maglev_graph_building || v8_flags.trace_maglev_regalloc) {
    MaglevCompilationUnit* top_level_unit =
        compilation_info->toplevel_compilation_unit();
    std::cout << "Compiling " << Brief(*top_level_unit->function().object())
              << " with Maglev\n";
    BytecodeArray::Disassemble(top_level_unit->bytecode().object(), std::cout);
    top_level_unit->feedback().object()->Print(std::cout);
  }

  Graph* graph = Graph::New(compilation_info->zone());

  MaglevGraphBuilder graph_builder(
      local_isolate, compilation_info->toplevel_compilation_unit(), graph);

  graph_builder.Build();

  if (v8_flags.print_maglev_graph) {
    std::cout << "\nAfter graph buiding" << std::endl;
    PrintGraph(std::cout, compilation_info, graph_builder.graph());
  }

#ifdef DEBUG
  {
    GraphProcessor<MaglevGraphVerifier> verifier(compilation_info);
    verifier.ProcessGraph(graph_builder.graph());
  }
#endif

  {
    GraphMultiProcessor<UseMarkingProcessor, MaglevVregAllocator> processor(
        UseMarkingProcessor{compilation_info});
    processor.ProcessGraph(graph_builder.graph());
  }

  if (v8_flags.print_maglev_graph) {
    std::cout << "After node processor" << std::endl;
    PrintGraph(std::cout, compilation_info, graph_builder.graph());
  }

  StraightForwardRegisterAllocator allocator(compilation_info,
                                             graph_builder.graph());

  if (v8_flags.print_maglev_graph) {
    std::cout << "After register allocation" << std::endl;
    PrintGraph(std::cout, compilation_info, graph_builder.graph());
  }

  GraphProcessor<TranslationArrayProcessor> build_translation_array(
      local_isolate, compilation_info);
  build_translation_array.ProcessGraph(graph_builder.graph());

  // Stash the compiled graph on the compilation info.
  compilation_info->set_graph(graph_builder.graph());
}

// static
MaybeHandle<CodeT> MaglevCompiler::GenerateCode(
    Isolate* isolate, MaglevCompilationInfo* compilation_info) {
  Graph* const graph = compilation_info->graph();
  if (graph == nullptr) {
    // Compilation failed.
    compilation_info->toplevel_compilation_unit()
        ->shared_function_info()
        .object()
        ->set_maglev_compilation_failed(true);
    return {};
  }

  Handle<Code> code;
  if (!MaglevCodeGenerator::Generate(isolate, compilation_info, graph)
           .ToHandle(&code)) {
    compilation_info->toplevel_compilation_unit()
        ->shared_function_info()
        .object()
        ->set_maglev_compilation_failed(true);
    return {};
  }

  if (!compilation_info->broker()->dependencies()->Commit(code)) {
    // Don't `set_maglev_compilation_failed` s.t. we may reattempt compilation.
    // TODO(v8:7700): Make this more robust, i.e.: don't recompile endlessly,
    // and possibly attempt to recompile as early as possible.
    return {};
  }

  if (v8_flags.print_maglev_code) {
    code->Print();
  }

  isolate->native_context()->AddOptimizedCode(ToCodeT(*code));
  return ToCodeT(code, isolate);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
