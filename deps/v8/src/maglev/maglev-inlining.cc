// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-inlining.h"

#include <algorithm>
#include <utility>

#include "src/base/logging.h"
#include "src/execution/local-isolate.h"
#include "src/maglev/maglev-graph-optimizer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-post-hoc-optimizations-processors.h"
#include "src/maglev/maglev-reducer-inl.h"

namespace v8::internal::maglev {

#define TRACE(x)                                                               \
  do {                                                                         \
    if (V8_UNLIKELY(v8_flags.trace_maglev_inlining && is_tracing_enabled())) { \
      StdoutStream() << x << std::endl;                                        \
    }                                                                          \
  } while (false)

bool MaglevCallSiteInfoCompare::operator()(const MaglevCallSiteInfo* info1,
                                           const MaglevCallSiteInfo* info2) {
  return info1->score < info2->score;
}

bool MaglevInliner::IsSmallWithHeapNumberInputsOutputs(
    MaglevCallSiteInfo* call_site) const {
  bool has_heapnumber_input_output = false;

  if (call_site->generic_call_node->get_uses_repr_hints().contains_any(
          UseRepresentationSet{UseRepresentation::kFloat64,
                               UseRepresentation::kHoleyFloat64,
                               UseRepresentation::kTruncatedInt32})) {
    // TruncatedInt32 uses do not necessarily mean that the input is a
    // HeapNumber, but when emitted operation that truncate their inputs to
    // Int32, Maglev doesn't distinguish between Smis and HeapNumbers.
    // TODO(dmercadier): distinguish between Smis and HeapNumbers in the graph
    // builder for operations that truncate their inputs, and then in turn, make
    // sure that we're not setting {has_heapnumber_input_output} to true for a
    // function that has so far only returned Smis.
    has_heapnumber_input_output = true;
  }

  if (!has_heapnumber_input_output) {
    for (ValueNode* input : call_site->caller_details.arguments) {
      if (input->UnwrapIdentities()->is_float64_or_holey_float64()) {
        has_heapnumber_input_output = true;
        break;
      }
    }
  }

  if (!has_heapnumber_input_output) {
    TRACE("> Does not have heapnum in/out. Uses: "
          << call_site->generic_call_node->get_uses_repr_hints());
    return false;
  }

  return call_site->bytecode_length <=
         max_inlined_bytecode_size_small_with_heapnum_in_out();
}

bool MaglevInliner::Run() {
  if (graph_->inlineable_calls().empty()) return true;

  while (!graph_->inlineable_calls().empty()) {
    MaglevCallSiteInfo* call_site = ChooseNextCallSite();

    TRACE("Considering for inlining "
          << graph_->graph_labeller()->NodeId(call_site->generic_call_node)
          << " : " << call_site->generic_call_node->shared_function_info()
          << " score:" << call_site->score);

    bool is_small_with_heapnum_input_outputs =
        IsSmallWithHeapNumberInputsOutputs(call_site);

    if (graph_->total_inlined_bytecode_size() >
        max_inlined_bytecode_size_cumulative()) {
      // We ran out of budget. Checking if this is a small-ish function that we
      // can still inline.
      if (graph_->total_inlined_bytecode_size_small() >
          max_inlined_bytecode_size_small_total()) {
        graph_->compilation_info()->set_could_not_inline_all_candidates();
        TRACE("> Budget exhausted, stopping");
        break;
      }

      if (!is_small_with_heapnum_input_outputs) {
        graph_->compilation_info()->set_could_not_inline_all_candidates();
        // Not that we don't break just rather just continue: next candidates
        // might be inlineable.
        TRACE("> !has_heapnumber_input_output or not enough budget, skipping");
        continue;
      }
    }

    InliningResult result =
        BuildInlineFunction(call_site, is_small_with_heapnum_input_outputs);
    if (result == InliningResult::kAbort) return false;
    if (result == InliningResult::kFail) continue;
    DCHECK_EQ(result, InliningResult::kDone);

    // If --trace-maglev-inlining-verbose, we print the graph after each
    // inlining step/call.
    if (V8_UNLIKELY(ShouldPrintMaglevGraph())) {
      std::cout << "\nAfter inlining "
                << call_site->generic_call_node->shared_function_info()
                << std::endl;
      PrintGraph(std::cout, graph_);
    }

    // Optimize current graph.
    {
      GraphProcessor<MaglevGraphOptimizer> optimizer(graph_);
      optimizer.ProcessGraph(graph_);

      if (V8_UNLIKELY(ShouldPrintMaglevGraph())) {
        std::cout << "\nAfter optimization "
                  << call_site->generic_call_node->shared_function_info()
                  << std::endl;
        PrintGraph(std::cout, graph_);
      }
    }
  }

  GraphProcessor<ClearReturnedValueUsesFromDeoptFrames>
      clear_returned_value_uses(zone());
  clear_returned_value_uses.ProcessGraph(graph_);

  // Otherwise we print just once at the end.
  if (V8_UNLIKELY(ShouldPrintMaglevGraph())) {
    std::cout << "\nAfter inlining" << std::endl;
    PrintGraph(std::cout, graph_);
  }

  return true;
}

int MaglevInliner::max_inlined_bytecode_size_cumulative() const {
  if (graph_->compilation_info()->is_turbolev()) {
    return v8_flags.max_inlined_bytecode_size_cumulative;
  } else {
    return v8_flags.max_maglev_inlined_bytecode_size_cumulative;
  }
}
int MaglevInliner::max_inlined_bytecode_size_small_total() const {
  if (graph_->compilation_info()->is_turbolev()) {
    return v8_flags.max_inlined_bytecode_size_small_total;
  } else {
    return v8_flags.max_maglev_inlined_bytecode_size_small_total;
  }
}
int MaglevInliner::max_inlined_bytecode_size_small_with_heapnum_in_out() const {
  if (graph_->compilation_info()->is_turbolev()) {
    return v8_flags.max_inlined_bytecode_size_small_with_heapnum_in_out;
  } else {
    return v8_flags.max_maglev_inlined_bytecode_size_small_with_heapnum_in_out;
  }
}

MaglevCallSiteInfo* MaglevInliner::ChooseNextCallSite() {
  auto call_site = graph_->inlineable_calls().top();
  graph_->inlineable_calls().pop();
  return call_site;
}

MaglevInliner::InliningResult MaglevInliner::BuildInlineFunction(
    MaglevCallSiteInfo* call_site, bool is_small) {
  CallKnownJSFunction* call_node = call_site->generic_call_node;
  BasicBlock* call_block = call_node->owner();
  MaglevCallerDetails* caller_details = &call_site->caller_details;
  DeoptFrame* caller_deopt_frame = caller_details->deopt_frame;
  const MaglevCompilationUnit* caller_unit =
      &caller_deopt_frame->GetCompilationUnit();
  compiler::SharedFunctionInfoRef shared = call_node->shared_function_info();

  if (!call_block || call_block->is_dead()) {
    // The block containing the call is unreachable, and it was previously
    // removed. Do not try to inline the call.
    return InliningResult::kFail;
  }

  if (V8_UNLIKELY(v8_flags.trace_maglev_inlining && is_tracing_enabled())) {
    std::cout << "  non-eager inlining " << shared << std::endl;
  }

  // Check if the catch block might become unreachable, ie, the call is the
  // only instance of a throwable node in this block to the same catch block.
  ExceptionHandlerInfo* call_exception_handler_info =
      call_node->exception_handler_info();
  bool catch_block_might_be_unreachable = false;
  if (call_exception_handler_info->HasExceptionHandler() &&
      !call_exception_handler_info->ShouldLazyDeopt()) {
    BasicBlock* catch_block = call_exception_handler_info->catch_block();
    catch_block_might_be_unreachable = true;
    for (ExceptionHandlerInfo* info : call_block->exception_handlers()) {
      if (info != call_exception_handler_info && info->HasExceptionHandler() &&
          !info->ShouldLazyDeopt() && info->catch_block() == catch_block) {
        catch_block_might_be_unreachable = false;
        break;
      }
    }
  }

  // Remove exception handler info from call block.
  ExceptionHandlerInfo::List rem_handlers_in_call_block;
  call_block->exception_handlers().TruncateAt(&rem_handlers_in_call_block,
                                              call_exception_handler_info);
  rem_handlers_in_call_block.DropHead();

  // Truncate the basic block.
  ZoneVector<Node*> rem_nodes_in_call_block =
      call_block->Split(call_node, zone());

  // Create a new compilation unit.
  MaglevCompilationUnit* inner_unit = MaglevCompilationUnit::NewInner(
      zone(), caller_unit, shared, call_site->feedback_cell);

  if (is_small) {
    graph_->add_inlined_bytecode_size_small(call_site->bytecode_length);
  } else {
    graph_->add_inlined_bytecode_size(call_site->bytecode_length);
  }

  // This can be invalidated by a previous inlining and it was not propagated to
  // this node.
  // TODO(victorgomes): Check if we should maintain this. We could also clear
  // unstable maps here.
  call_site->caller_details.known_node_aspects
      ->MarkAnyMapForAnyNodeIsUnstable();

  // Create a new graph builder for the inlined function.
  LocalIsolate* local_isolate = broker()->local_isolate_or_isolate();
  MaglevGraphBuilder inner_graph_builder(local_isolate, inner_unit, graph_,
                                         &call_site->caller_details);

  // Make sure the arguments identities and conversions are unwrapped, so they
  // don't leak to the graph builder, nor to the interpreter frame state.
  for (ValueNode*& node : call_site->caller_details.arguments) {
    node = node->Unwrap();
  }

  // Update caller deopt frame with inlined arguments.
  caller_details->deopt_frame =
      inner_graph_builder.AddInlinedArgumentsToDeoptFrame(
          caller_deopt_frame, inner_unit, call_node->closure().node(),
          call_site->caller_details.arguments);

  // We truncate the graph to build the function in-place, preserving the
  // invariant that all jumps move forward (except JumpLoop).
  std::vector<BasicBlock*> saved_bb = TruncateGraphAt(call_block);
  ControlNode* control_node = call_block->reset_control_node();

  // Set the inner graph builder to build in the truncated call block.
  inner_graph_builder.set_current_block(call_block);

  // TODO(victorgomes): Fix use count. BuildInlineFunction uses the nodes from
  // caller_details.arguments as input instead of the call_node inputs. But we
  // remove the uses from the call node_inputs when overwriting the returned
  // value.
  ReduceResult result = inner_graph_builder.BuildInlineFunction(
      caller_deopt_frame->GetSourcePosition(),
      call_node->context().node()->Unwrap(),
      call_node->closure().node()->Unwrap(),
      call_node->new_target().node()->Unwrap());

  if (result.IsDoneWithAbort()) {
    if (inner_graph_builder.should_abort_compilation()) {
      return InliningResult::kAbort;
    }

    // Since the rest of the block is dead, these nodes don't belong
    // to any basic block anymore.
    for (auto node : rem_nodes_in_call_block) {
      if (node == nullptr) continue;
      node->set_owner(nullptr);
    }
    // Restore the rest of the graph.
    for (auto bb : saved_bb) {
      graph_->Add(bb);
    }
    RemovePredecessorFollowing(control_node, call_block);
    // TODO(victorgomes): We probably don't need to iterate all the graph to
    // remove unreachable blocks, but only the successors of control_node in
    // saved_bbs.
    RemoveUnreachableBlocks();
    return InliningResult::kDone;
  }

  DCHECK(result.IsDoneWithValue());
  ValueNode* returned_value = result.value()->Unwrap();

  // Resume execution using the final block of the inner builder.
  // Add remaining nodes to the final block and use the control flow of the
  // old call block.
  BasicBlock* final_block = inner_graph_builder.FinishInlinedBlockForCaller(
      control_node, rem_nodes_in_call_block);
  DCHECK_NOT_NULL(final_block);
  final_block->exception_handlers().Append(
      std::move(rem_handlers_in_call_block));

  // Update the predecessor of the successors of the {final_block}, that were
  // previously pointing to {call_block}.
  final_block->ForEachSuccessor(
      [call_block, final_block](BasicBlock* successor) {
        UpdatePredecessorsOf(successor, call_block, final_block);
      });

  // Restore the rest of the graph.
  for (auto bb : saved_bb) {
    graph_->Add(bb);
  }

  if (auto alloc = returned_value->TryCast<InlinedAllocation>()) {
    // TODO(victorgomes): Support eliding VOs.
    alloc->ForceEscaping();
#ifdef DEBUG
    alloc->set_is_returned_value_from_inline_call();
#endif  // DEBUG
  }
  call_node->OverwriteWithReturnValue(returned_value);

  // Remove unreachable catch block if no throwable nodes were added during
  // inlining.
  // TODO(victorgomes): Improve this: track if we didnt indeed add a throwable
  // node.
  if (catch_block_might_be_unreachable) {
    RemoveUnreachableBlocks();
  }

  return InliningResult::kDone;
}

std::vector<BasicBlock*> MaglevInliner::TruncateGraphAt(BasicBlock* block) {
  // TODO(victorgomes): Consider using a linked list of basic blocks in Maglev
  // instead of a vector.
  auto it = std::find(graph_->blocks().begin(), graph_->blocks().end(), block);
  CHECK_NE(it, graph_->blocks().end());
  size_t index = std::distance(graph_->blocks().begin(), it);
  std::vector<BasicBlock*> saved_bb(graph_->blocks().begin() + index + 1,
                                    graph_->blocks().end());
  graph_->blocks().resize(index);
  return saved_bb;
}

// static
void MaglevInliner::UpdatePredecessorsOf(BasicBlock* block,
                                         BasicBlock* prev_pred,
                                         BasicBlock* new_pred) {
  if (!block->has_state()) {
    DCHECK_EQ(block->predecessor(), prev_pred);
    block->set_predecessor(new_pred);
    return;
  }
  for (int i = 0; i < block->predecessor_count(); i++) {
    if (block->predecessor_at(i) == prev_pred) {
      block->state()->set_predecessor_at(i, new_pred);
      break;
    }
  }
}

void MaglevInliner::RemovePredecessorFollowing(ControlNode* control,
                                               BasicBlock* call_block) {
  BasicBlock::ForEachSuccessorFollowing(control, [&](BasicBlock* succ) {
    if (!succ->has_state()) {
      succ->set_predecessor(nullptr);
      return;
    }
    if (succ->is_loop() && succ->backedge_predecessor() == call_block) {
      succ->state()->TurnLoopIntoRegularBlock();
      return;
    }
    for (int i = succ->predecessor_count() - 1; i >= 0; i--) {
      if (succ->predecessor_at(i) == call_block) {
        succ->state()->RemovePredecessorAt(i);
      }
    }
  });
}

ProcessResult ReturnedValueRepresentationSelector::Process(
    ReturnedValue* node, const ProcessingState& state) {
  if (!node->is_used()) {
    return ProcessResult::kRemove;
  }
  // TODO(victorgomes): Use KNA to create better conversion nodes?
  ValueNode* input = node->input(0).node()->UnwrapIdentities();
  switch (input->value_representation()) {
    case ValueRepresentation::kInt32:
      node->OverwriteWith<Int32ToNumber>();
      break;
    case ValueRepresentation::kUint32:
      node->OverwriteWith<Uint32ToNumber>();
      break;
    case ValueRepresentation::kFloat64:
      node->OverwriteWith<Float64ToTagged>();
      node->Cast<Float64ToTagged>()->SetMode(
          Float64ToTagged::ConversionMode::kForceHeapNumber);
      break;
    case ValueRepresentation::kHoleyFloat64:
      node->OverwriteWith<HoleyFloat64ToTagged>();
      node->Cast<HoleyFloat64ToTagged>()->SetMode(
          HoleyFloat64ToTagged::ConversionMode::kForceHeapNumber);
      break;
    case ValueRepresentation::kIntPtr:
      node->OverwriteWith<IntPtrToNumber>();
      break;
    case ValueRepresentation::kTagged:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  return ProcessResult::kContinue;
}

}  // namespace v8::internal::maglev
