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
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/execution/frames.h"
#include "src/ic/handler-configuration.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-generator.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph-verifier.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc.h"
#include "src/maglev/maglev-vreg-allocator.h"
#include "src/objects/code-inl.h"
#include "src/objects/js-function.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

class NumberingProcessor {
 public:
  void PreProcessGraph(MaglevCompilationInfo*, Graph* graph) { node_id_ = 1; }
  void PostProcessGraph(MaglevCompilationInfo*, Graph* graph) {}
  void PreProcessBasicBlock(MaglevCompilationInfo*, BasicBlock* block) {}

  void Process(NodeBase* node, const ProcessingState& state) {
    node->set_id(node_id_++);
  }

 private:
  uint32_t node_id_;
};

class UseMarkingProcessor {
 public:
  void PreProcessGraph(MaglevCompilationInfo*, Graph* graph) {}
  void PostProcessGraph(MaglevCompilationInfo*, Graph* graph) {}
  void PreProcessBasicBlock(MaglevCompilationInfo*, BasicBlock* block) {}

  template <typename NodeT>
  void Process(NodeT* node, const ProcessingState& state) {
    if constexpr (NodeT::kProperties.can_eager_deopt()) {
      MarkCheckpointNodes(node, node->eager_deopt_info(), state);
    }
    for (Input& input : *node) {
      input.node()->mark_use(node->id(), &input);
    }
    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      MarkCheckpointNodes(node, node->lazy_deopt_info(), state);
    }
  }

  void Process(Phi* node, const ProcessingState& state) {
    // Don't mark Phi uses when visiting the node, because of loop phis.
    // Instead, they'll be visited while processing Jump/JumpLoop.
  }

  // Specialize the two unconditional jumps to extend their Phis' inputs' live
  // ranges.

  void Process(JumpLoop* node, const ProcessingState& state) {
    int i = state.block()->predecessor_id();
    BasicBlock* target = node->target();
    if (!target->has_phi()) return;
    uint32_t use = node->id();
    for (Phi* phi : *target->phis()) {
      ValueNode* input = phi->input(i).node();
      input->mark_use(use, &phi->input(i));
    }
  }
  void Process(Jump* node, const ProcessingState& state) {
    int i = state.block()->predecessor_id();
    BasicBlock* target = node->target();
    if (!target->has_phi()) return;
    uint32_t use = node->id();
    for (Phi* phi : *target->phis()) {
      ValueNode* input = phi->input(i).node();
      input->mark_use(use, &phi->input(i));
    }
  }

 private:
  void MarkCheckpointNodes(NodeBase* node, const MaglevCompilationUnit& unit,
                           const CheckpointedInterpreterState* checkpoint_state,
                           InputLocation* input_locations,
                           const ProcessingState& state, int& index) {
    if (checkpoint_state->parent) {
      MarkCheckpointNodes(node, *unit.caller(), checkpoint_state->parent,
                          input_locations, state, index);
    }

    const CompactInterpreterFrameState* register_frame =
        checkpoint_state->register_frame;
    int use_id = node->id();

    register_frame->ForEachValue(
        unit, [&](ValueNode* node, interpreter::Register reg) {
          node->mark_use(use_id, &input_locations[index++]);
        });
  }
  void MarkCheckpointNodes(NodeBase* node, const EagerDeoptInfo* deopt_info,
                           const ProcessingState& state) {
    int index = 0;
    MarkCheckpointNodes(node, deopt_info->unit, &deopt_info->state,
                        deopt_info->input_locations, state, index);
  }
  void MarkCheckpointNodes(NodeBase* node, const LazyDeoptInfo* deopt_info,
                           const ProcessingState& state) {
    const CompactInterpreterFrameState* register_frame =
        deopt_info->state.register_frame;
    int use_id = node->id();
    int index = 0;

    register_frame->ForEachValue(
        deopt_info->unit, [&](ValueNode* node, interpreter::Register reg) {
          // Skip over the result location.
          if (reg == deopt_info->result_location) return;
          node->mark_use(use_id, &deopt_info->input_locations[index++]);
        });
  }
};

// static
void MaglevCompiler::Compile(LocalIsolate* local_isolate,
                             MaglevCompilationInfo* compilation_info) {
  compiler::UnparkedScopeIfNeeded unparked_scope(compilation_info->broker());

  // Build graph.
  if (FLAG_print_maglev_code || FLAG_code_comments || FLAG_print_maglev_graph ||
      FLAG_trace_maglev_regalloc) {
    compilation_info->set_graph_labeller(new MaglevGraphLabeller());
  }

  // TODO(v8:7700): Support exceptions in maglev. We currently bail if exception
  // handler table is non-empty.
  if (compilation_info->toplevel_compilation_unit()
          ->bytecode()
          .handler_table_size() > 0) {
    return;
  }

  Graph* graph = Graph::New(compilation_info->zone());

  MaglevGraphBuilder graph_builder(
      local_isolate, compilation_info->toplevel_compilation_unit(), graph);

  graph_builder.Build();

  // TODO(v8:7700): Clean up after all bytecodes are supported.
  if (graph_builder.found_unsupported_bytecode()) {
    return;
  }

  if (FLAG_print_maglev_graph) {
    MaglevCompilationUnit* top_level_unit =
        compilation_info->toplevel_compilation_unit();
    std::cout << "Compiling " << Brief(*top_level_unit->function().object())
              << " with Maglev\n";
    top_level_unit->bytecode().object()->Disassemble(std::cout);
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
    GraphMultiProcessor<NumberingProcessor, UseMarkingProcessor,
                        MaglevVregAllocator>
        processor(compilation_info);
    processor.ProcessGraph(graph_builder.graph());
  }

  if (FLAG_print_maglev_graph) {
    std::cout << "After node processor" << std::endl;
    PrintGraph(std::cout, compilation_info, graph_builder.graph());
  }

  StraightForwardRegisterAllocator allocator(compilation_info,
                                             graph_builder.graph());

  if (FLAG_print_maglev_graph) {
    std::cout << "After register allocation" << std::endl;
    PrintGraph(std::cout, compilation_info, graph_builder.graph());
  }

  // Stash the compiled graph on the compilation info.
  compilation_info->set_graph(graph_builder.graph());
}

// static
MaybeHandle<CodeT> MaglevCompiler::GenerateCode(
    MaglevCompilationInfo* compilation_info) {
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
  if (!MaglevCodeGenerator::Generate(compilation_info, graph).ToHandle(&code)) {
    compilation_info->toplevel_compilation_unit()
        ->shared_function_info()
        .object()
        ->set_maglev_compilation_failed(true);
    return {};
  }

  compiler::JSHeapBroker* const broker = compilation_info->broker();
  const bool deps_committed_successfully = broker->dependencies()->Commit(code);
  CHECK(deps_committed_successfully);

  if (FLAG_print_maglev_code) {
    code->Print();
  }

  Isolate* const isolate = compilation_info->isolate();
  isolate->native_context()->AddOptimizedCode(ToCodeT(*code));
  return ToCodeT(code, isolate);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
