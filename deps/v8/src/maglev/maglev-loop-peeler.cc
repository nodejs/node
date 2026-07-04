// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-loop-peeler.h"

#include "src/base/logging.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-tracer.h"

namespace v8::internal::maglev {

#define TRACE_PEEL_SKIP(...) TRACE_PEEL(TraceColor::kDarkRed << __VA_ARGS__)
#define TRACE_PEEL_PEELED(...) TRACE_PEEL(TraceColor::kDarkGreen << __VA_ARGS__)

namespace {

ValueNode* Remap(const ValueMap& vmap, ValueNode* v) {
  if (v == nullptr) return nullptr;
  auto it = vmap.find(v);
  return it != vmap.end() ? it->second : v;
}

void RegisterIfLabelled(Graph* graph, NodeBase* node) {
  if (graph->has_graph_labeller()) {
    graph->graph_labeller()->RegisterNode(node);
  }
}

void RegisterClonedNode(Graph* graph, NodeBase* clone, const NodeBase* src) {
  if (graph->has_graph_labeller()) {
    graph->graph_labeller()->RegisterNode(
        clone, &graph->graph_labeller()->GetNodeProvenance(src));
  }
}

int LoopHeaderBytecodeOffset(BasicBlock* block) {
  if (!block->has_state()) return -1;
  return block->state()->merge_offset();
}

// Bumps `node`'s deopt-use counter the same way the graph builder does when
// it stores a ValueNode into a deopt frame.
ValueNode* RemapAndAddDeoptUse(const ValueMap& vmap, ValueNode* v,
                               const VirtualObjectList& virtual_objects) {
  ValueNode* mapped = Remap(vmap, v);
  if (mapped == nullptr) return nullptr;
  if (mapped->template Is<VirtualObject>()) return mapped;
  mapped->AddDeoptUse(virtual_objects);
  return mapped;
}

// Recursively clone a DeoptFrame, remapping ValueNode references through
// `vmap`. Frame-state values not in `vmap` are kept as-is (they're loop-
// invariant relative to the body we're peeling). Bumps the use-count of
// every ValueNode referenced by the cloned frame so subsequent dead-node
// sweeping doesn't drop them.
DeoptFrame* CloneDeoptFrame(DeoptFrame* src, const ValueMap& vmap, Zone* zone) {
  if (src == nullptr) return nullptr;
  DeoptFrame* parent_clone = CloneDeoptFrame(src->parent(), vmap, zone);
  VirtualObjectList remapped_vos =
      parent_clone ? parent_clone->GetVirtualObjects() : VirtualObjectList();
  switch (src->type()) {
    case DeoptFrame::FrameType::kInterpretedFrame: {
      auto& f = src->as_interpreted();
      const MaglevCompilationUnit& info = f.unit();
      VirtualObject* lvo = f.last_virtual_object();
      if (lvo != nullptr) {
        lvo = Remap(vmap, lvo)->Cast<VirtualObject>();
      }
      VirtualObjectList frame_vos(lvo);
      auto* new_fs = zone->New<CompactInterpreterFrameState>(
          info, f.frame_state()->liveness());
      new_fs->ForEachValue(
          info, [&](ValueNode*& dst, interpreter::Register reg) {
            ValueNode* src_val = f.frame_state()->GetValueOf(reg, info);
            dst = RemapAndAddDeoptUse(vmap, src_val, frame_vos);
          });
      ValueNode* closure = RemapAndAddDeoptUse(vmap, f.closure(), frame_vos);
      return zone->New<InterpretedDeoptFrame>(
          info, new_fs, closure, lvo, f.bytecode_position(),
          f.source_position(), parent_clone);
    }
    case DeoptFrame::FrameType::kInlinedArgumentsFrame: {
      auto& f = src->as_inlined_arguments();
      ValueNode* closure = RemapAndAddDeoptUse(vmap, f.closure(), remapped_vos);
      ValueNode** args = zone->AllocateArray<ValueNode*>(f.arguments().size());
      for (size_t i = 0; i < f.arguments().size(); ++i) {
        args[i] = RemapAndAddDeoptUse(vmap, f.arguments()[i], remapped_vos);
      }
      return zone->New<InlinedArgumentsDeoptFrame>(
          f.unit(), f.bytecode_position(), closure,
          base::Vector<ValueNode*>(args, f.arguments().size()), parent_clone);
    }
    case DeoptFrame::FrameType::kConstructInvokeStubFrame: {
      auto& f = src->as_construct_stub();
      return zone->New<ConstructInvokeStubDeoptFrame>(
          f.unit(), f.source_position(),
          RemapAndAddDeoptUse(vmap, f.receiver(), remapped_vos),
          RemapAndAddDeoptUse(vmap, f.context(), remapped_vos), parent_clone);
    }
    case DeoptFrame::FrameType::kBuiltinContinuationFrame: {
      auto& f = src->as_builtin_continuation();
      ValueNode** params =
          zone->AllocateArray<ValueNode*>(f.parameters().size());
      for (size_t i = 0; i < f.parameters().size(); ++i) {
        params[i] = RemapAndAddDeoptUse(vmap, f.parameters()[i], remapped_vos);
      }
      compiler::OptionalJSFunctionRef js_target;
      if (f.is_javascript()) js_target = f.javascript_target();
      return zone->New<BuiltinContinuationDeoptFrame>(
          f.builtin_id(),
          base::Vector<ValueNode*>(params, f.parameters().size()),
          RemapAndAddDeoptUse(vmap, f.context(), remapped_vos), js_target,
          parent_clone);
    }
  }
  UNREACHABLE();
}

void RemapClonedDeoptInfo(NodeBase* src, NodeBase* clone, const ValueMap& vmap,
                          Zone* zone) {
  if (clone->properties().has_eager_deopt_info()) {
    DeoptFrame* cloned_top =
        CloneDeoptFrame(&src->eager_deopt_info()->top_frame(), vmap, zone);
    clone->eager_deopt_info()->set_top_frame(cloned_top);
  }
  if (clone->properties().can_lazy_deopt()) {
    DeoptFrame* cloned_top =
        CloneDeoptFrame(&src->lazy_deopt_info()->top_frame(), vmap, zone);
    clone->lazy_deopt_info()->set_top_frame(cloned_top);
  }
}

void RemapClonedExceptionHandlerInfo(NodeBase* src, NodeBase* clone,
                                     BasicBlock* dst_block) {
  if (!clone->properties().can_throw()) return;
  ExceptionHandlerInfo* src_info = src->exception_handler_info();
  DCHECK(!src_info->HasExceptionHandler() || src_info->ShouldLazyDeopt());
  if (src_info->ShouldLazyDeopt()) {
    new (clone->exception_handler_info())
        ExceptionHandlerInfo(ExceptionHandlerInfo::kLazyDeopt);
  } else {
    new (clone->exception_handler_info()) ExceptionHandlerInfo();
  }
  if (clone->Is<CallKnownJSFunction>()) {
    dst_block->AddExceptionHandler(clone->exception_handler_info());
  }
}

// Clone a single non-phi, non-control Node by raw byte copy; rewires its
// inputs via vmap; for nodes carrying eager/lazy deopt info, also clones the
// deopt frame chain in-place.
Node* CloneNodeWithRemap(Node* src, const ValueMap& vmap, Zone* zone) {
  Node* clone = nullptr;
  switch (src->opcode()) {
#define CLONE_CASE(Name)                                         \
  case Opcode::k##Name:                                          \
    if constexpr (Opcode::k##Name != Opcode::kPhi) {             \
      clone = NodeBase::CloneRaw<Name>(src->Cast<Name>(), zone); \
    }                                                            \
    break;
    NODE_LIST(CLONE_CASE)
#undef CLONE_CASE
    default:
      break;
  }
  CHECK_NOT_NULL(clone);

  // Wire inputs: read each input from `src`, remap, set on clone. A src input
  // can be null (a dead node whose inputs were cleared without shrinking
  // input_count); CloneRaw already nulled the clone's input slots and set_input
  // DCHECKs non-null, so leave those slots null.
  DCHECK_EQ(clone->input_count(), src->input_count());
  for (int i = 0; i < clone->input_count(); ++i) {
    ValueNode* in = src->input(i).node();
    if (in == nullptr) continue;
    clone->set_input(i, Remap(vmap, in));
  }

  // Patch eager/lazy deopt info: clone the deopt frame chain.
  RemapClonedDeoptInfo(src, clone, vmap, zone);
  return clone;
}

// Returns true if the node is safe to omit from the peeled iteration.
// These nodes (loop-interrupt + OSR triggers) are only meaningful at the
// loop's back-edge and produce no value used elsewhere.
bool IsSkippableInPeel(Node* node) {
  return node->Is<HandleNoHeapWritesInterrupt>() ||
         node->Is<TryOnStackReplacement>() ||
         node->Is<ReduceInterruptBudgetForLoop>();
}

// TODO(victorgomes): This might be too conservative. To investigate.
// Returns true if cloning `node` would corrupt graph-level bookkeeping that
// the peeler cannot replicate (escape/elide maps for allocations, context
// allocation tracking, etc.). Conservative; opcodes get added here as their
// side tables are understood.
bool NodeHasUntrackableSideState(Node* node) {
  switch (node->opcode()) {
    // TODO(victorgomes): Allocation folding is disabled on turbolev, check if
    // we need to bail for these nodes.
    case Opcode::kInlinedAllocation:
    case Opcode::kAllocationBlock:
    // TODO(victorgomes): support AllocateElementsArray — peeling a loop that
    // contains it would let the array be allocated only once.
    case Opcode::kAllocateElementsArray:
    case Opcode::kCreateFunctionContext:
      return true;
    default:
      return false;
  }
}

// Substitutes Phi references inside a deopt-info's frame chain. For each
// ValueNode reference that is a key in `replacements` (the keys are always
// Phis), replaces it with the mapped value, updating use counts. Used by the
// peeler to redirect downstream deopt-frame references from loop header
// phis to the peel-exit-merge (PEM) phis. Goes through every input the
// visitor exposes — closure, frame_state values, inlined arguments,
// stub-frame receiver/context, builtin-continuation parameters/context —
// so the rewire covers all four DeoptFrame::FrameType variants.
template <typename DeoptInfoT>
void RewireDeoptFramePhiRefs(DeoptInfoT* info, const PhiMap& replacements) {
  info->ForEachInput([&](ValueNode*& slot) {
    Phi* phi = slot->TryCast<Phi>();
    if (phi == nullptr) return;
    auto it = replacements.find(phi);
    if (it == replacements.end()) return;
    slot = it->second;
    phi->remove_use();
    slot->add_use();
  });
}

// Rewrites a node's input references that match the replacements map.
void RewireNodeInputs(NodeBase* node, const PhiMap& replacements) {
  for (int i = 0; i < node->input_count(); ++i) {
    ValueNode* in = node->input(i).node();
    if (in == nullptr) continue;
    if (Phi* in_phi = in->TryCast<Phi>()) {
      auto it = replacements.find(in_phi);
      if (it != replacements.end()) {
        node->change_input(i, it->second);
      }
    }
  }
}

// Rewrites every successor target of `control` from `from` to `to` in place.
// Returns the number of edges retargeted.
void RetargetControlSuccessors(ControlNode* control, BasicBlock* from,
                               BasicBlock* to) {
  if (auto* uc = control->TryCast<UnconditionalControlNode>()) {
    if (uc->target() == from) {
      uc->set_target(to);
    }
  } else if (auto* br = control->TryCast<BranchControlNode>()) {
    if (br->if_true() == from) {
      br->set_if_true(to);
    }
    if (br->if_false() == from) {
      br->set_if_false(to);
    }
  } else if (auto* sw = control->TryCast<Switch>()) {
    for (int i = 0; i < sw->size(); ++i) {
      if (sw->targets()[i].block_ptr() == from) {
        sw->targets()[i].set_block_ptr(to);
      }
    }
    if (sw->has_fallthrough() && sw->fallthrough() == from) {
      sw->set_fallthrough(to);
    }
  }
}

}  // namespace

// Matches each recorded peelable loop (identified during graph building by its
// owning compilation unit and loop-header bytecode offset) to its loop-header
// BasicBlock in the final, post-inlining graph, caching it in info.header.
void MaglevLoopPeeler::ResolveLoopHeader() {
  for (BasicBlock* block : graph_->blocks()) {
    if (!block->is_loop()) continue;
    int header_offset = LoopHeaderBytecodeOffset(block);
    const MaglevCompilationUnit& unit = block->state()->unit();
    auto it = std::find_if(
        graph_->peelable_loops().begin(), graph_->peelable_loops().end(),
        [&unit, header_offset](const MaglevLoopPeelInfo& info) {
          return info.unit == &unit && info.loop_header_offset == header_offset;
        });
    if (it != graph_->peelable_loops().end()) it->header = block;
  }
}

// Walks the graph in RPO from `header` to the back-edge to compute the
// natural-loop body. A block belongs to the loop iff it lies on the RPO
// segment between the header and the (unique) back-edge predecessor and is
// reducible-entry (all of its predecessors are already in the body). On the
// way, the function collects:
//   - body blocks in RPO with the back-edge cut,
//   - the unique back-edge predecessor (block whose JumpLoop targets header),
//   - one ExitEdge per body-block successor that leaves body_set, walking
//     past any existing kEdgeSplit pass-through to find the real downstream
//     block and recording the absorbed edge-split as `original_es`.
// TODO(victorgomes): finding the body by walking backward from the back-edge
// (à la Turboshaft's LoopFinder) is more precise than this forward RPO walk
// (e.g. it wouldn't count a block reached only via a `break` as in-loop);
// switch to it in a follow-up.
// Bails on:
//   - more than one forward predecessor (Maglev loops have a single back-edge,
//     so predecessor_count > 2 means extra forward edges into the header),
//   - a nested loop in the body (a body block that is itself a loop header);
//     this is what restricts peeling to innermost loops,
//   - a body block with attached exception handlers.
std::optional<MaglevLoopPeeler::LoopInfo> MaglevLoopPeeler::BuildLoopInfo(
    BasicBlock* header) {
  int header_offset = LoopHeaderBytecodeOffset(header);
  // ResolveLoopHeader only tags loop blocks, and Run() rejects dead headers.
  DCHECK(header->is_loop());
  DCHECK(!header->state()->is_resumable_loop());
  DCHECK(!header->state()->is_unmerged_loop());
  if (header->predecessor_count() != 2) {
    // TODO(victorgomes): a loop header with more than one forward predecessor
    // is peelable in principle; bailing here is conservative.
    TRACE_PEEL_SKIP("@" << header_offset
                        << ": skip (header has >1 forward predecessor)");
    return std::nullopt;
  }

  LoopInfo loop(zone());
  loop.preheader = header->predecessor_at(0);
  BasicBlock* backedge = header->backedge_predecessor();
  // With predecessor_count == 2 and a fully-merged loop, both slots are set.
  DCHECK_NOT_NULL(backedge);
  DCHECK_NOT_NULL(loop.preheader);
  // backedge_predecessor() is the last predecessor, the back-edge by
  // construction, so its control is a JumpLoop.
  DCHECK(backedge->control_node()->Is<JumpLoop>());

  // Walk RPO from header to backedge inclusive. For a reducible innermost
  // loop, body == RPO[header..backedge]. Build body and body_set in one
  // pass; bail on nested loops and irreducible entry blocks. After the
  // walk, body.front() == header and body.back() == backedge.
  auto it = std::find(graph_->blocks().begin(), graph_->blocks().end(), header);
  DCHECK_NE(it, graph_->blocks().end());
  DCHECK_EQ(*it, header);
  loop.body.push_back(header);
  loop.body_set.insert(header);
  if (header == backedge) {
    TRACE_PEEL_SKIP("@" << header_offset << ": skip (infinite loop)");
    return std::nullopt;
  }
  ++it;  // Skip header.
  do {
    DCHECK_NE(it, graph_->blocks().end());
    BasicBlock* b = *it;
    if (b->is_loop()) {
      TRACE_PEEL_SKIP("@" << header_offset << ": skip (not innermost)");
      return std::nullopt;
    }
    // Reducible-entry: for a reducible loop, every predecessor of a
    // non-header body block is itself a body block at an earlier RPO
    // position — i.e., already in body_set. In Maglev an irreducible entry
    // only comes from resumable generators, which we already skip.
    DCHECK(b->ForAllPredecessors(
        [&](BasicBlock* p) { return loop.body_set.contains(p); }));
    loop.body.push_back(b);
    loop.body_set.insert(b);
  } while (*it++ != backedge);

  // Classify out-of-body successor edges and collect exit edges.
  for (BasicBlock* b : loop.body) {
    // TODO(victorgomes): support a catch block inside the loop body; its
    // exception merge state needs cloning support in CloneBodySubgraph.
    if (b->has_state() && b->state()->is_exception_handler()) {
      TRACE_PEEL_SKIP("@" << header_offset
                          << ": skip (catch block in loop body)");
      return std::nullopt;
    }

    // For each successor outside the body, record an ExitEdge.
    // If the exit edge is already an edge split, then the original iteration
    // has to funnel through the PEM after peeling. So instead of later
    // inserting a second edge-split, walk past the existing one to the real
    // downstream target and absorb it as `original_es` (its Jump gets
    // retargeted to the PEM in WireExitControl).
    b->ForEachSuccessor([&](BasicBlock* succ) {
      if (loop.body_set.contains(succ)) return;
      ExitEdge edge;
      edge.src = b;
      if (succ->is_edge_split_block()) {
        edge.original_es = succ;
        edge.target = succ->control_node()->Cast<Jump>()->target();
      } else {
        edge.original_es = nullptr;
        edge.target = succ;
      }
      if (edge.target->has_state() && edge.target->is_merge_block()) {
        // Record the exit's slot in the merge target's predecessor array:
        // RewireLoopConnections anchors the PEM at the lowest such slot and
        // removes the others; BuildPeelExitMerge folds it into pem_pred_id.
        BasicBlock* immediate_pred =
            edge.original_es != nullptr ? edge.original_es : b;
        int predecessor_id = edge.target->get_predecessor_index(immediate_pred);
        DCHECK_GE(predecessor_id, 0);
        edge.predecessor_id = predecessor_id;
      } else {
        edge.predecessor_id = -1;
      }
      loop.exits.push_back(edge);
    });
  }

  // A loop with no exit edges never leaves via a forward edge: either a truly
  // infinite loop (e.g. `while (true)` with no break/return), or a loop whose
  // only way out is terminal control (return/throw/deopt, which isn't a
  // successor edge leaving the body).
  if (loop.exits.empty()) {
    TRACE_PEEL_SKIP("@" << header_offset << ": skip (no exit edges)");
    return std::nullopt;
  }

  // TODO(victorgomes): we only support exits whose target has no phi nodes,
  // and all no-phi exits must share the same downstream target. A
  // phi-bearing target or multiple distinct no-phi targets would each
  // require their own PEM with its own header-phi → pem-phi map, which
  // makes the downstream rewire walk ambiguous.
  // loop.exits is non-empty (checked above), so seed from the first target and
  // require every exit to share it.
  for (const ExitEdge& e : loop.exits) {
    if (e.target->has_phi()) {
      TRACE_PEEL_SKIP("@" << header_offset << ": skip (exit target has phis)");
      return std::nullopt;
    }
    if (loop.exits.front().target != e.target) {
      TRACE_PEEL_SKIP("@" << header_offset
                          << ": skip (multiple distinct no-phi exit targets)");
      return std::nullopt;
    }
  }

  return loop;
}

bool MaglevLoopPeeler::IsCloneable(const LoopInfo& loop) const {
  int header_offset = LoopHeaderBytecodeOffset(loop.header());
  DCHECK(!loop.preheader->control_node()->Is<BranchControlNode>() &&
         !loop.preheader->control_node()->Is<Switch>());
  DCHECK(loop.preheader->control_node()->Is<Jump>() ||
         loop.preheader->control_node()->Is<CheckpointedJump>());


  // Collect every value defined in the loop body so the walk further below can
  // reject any body value that is used outside the body (peeling would break
  // its dominance on the body-skipped path).
  // TODO(victorgomes): Is this whole collecting body value nodes needed? Add a
  // test to cover these cases.
  ZoneAbslFlatHashSet<ValueNode*> body_values(zone());
  for (BasicBlock* block : loop.body) {
    if (!block->is_loop() && block->has_phi()) {
      for (Phi* phi : *block->phis()) {
        body_values.insert(phi);
      }
    }
    for (Node* node : block->nodes()) {
      if (node == nullptr) continue;
      if (IsSkippableInPeel(node)) continue;
      // TODO(victorgomes): support throwable nodes with a live catch block;
      // cloning one means cloning/sharing the catch block and its exception
      // merge state. Nodes with no live catch edge (no handler / lazy-deopt)
      // are cloned; see RemapClonedExceptionHandlerInfo.
      if (node->properties().can_throw()) {
        ExceptionHandlerInfo* info = node->exception_handler_info();
        if (info->HasExceptionHandler() && !info->ShouldLazyDeopt()) {
          TRACE_PEEL_SKIP("@" << header_offset
                              << ": skip (throwable node with live catch block "
                              << OpcodeToString(node->opcode()) << ")");
          return false;
        }
      }
      if (NodeHasUntrackableSideState(node)) {
        TRACE_PEEL_SKIP("@" << header_offset << ": skip (uncloneable node "
                            << OpcodeToString(node->opcode())
                            << "untrackable side state)");
        return false;
      }
      if (auto* v = node->TryCast<ValueNode>()) body_values.insert(v);
    }
    ControlNode* control = block->control_node();
    if (block == loop.backedge()) {
      DCHECK(control->Is<JumpLoop>());
    } else {
      if (control->Is<Switch>()) {
        // TODO(victorgomes): Support Switch control nodes in loop peeling.
        TRACE_PEEL_SKIP("@" << header_offset
                            << ": skip (unsupported switch control node)");
        return false;
      }
      if (control->Is<Throw>() &&
          control->exception_handler_info()->HasExceptionHandler() &&
          !control->exception_handler_info()->ShouldLazyDeopt()) {
        // TODO(victorgomes): Support live catch blocks.
        TRACE_PEEL_SKIP("@"
                        << header_offset
                        << ": skip (unsupported throw with live catch block)");
        return false;
      }
      // This is not the backedge, and the loop is innermost.
      DCHECK(!control->Is<JumpLoop>());
    }
  }

  // TODO(victorgomes): bail if any non-header-phi body value has an use
  // outside the loop body. After peeling, body values are defined only
  // on the body-ran path (through the original header); body-skipped
  // paths go through PEM and bypass the body. A downstream use of a
  // non-phi body value would no longer be dominated by its definition
  // on the body-skipped path. Header phis are exempt because the
  // peeler rewires their downstream uses to the matching PEM phi.
  // Follow-up work: we could use Identities to relax this invariant and
  // avoid iterating the entire graph.
  auto is_body_value = [&](ValueNode* in) -> bool {
    if (in == nullptr) return false;
    return body_values.contains(in);
  };
  for (BasicBlock* b : graph_->blocks()) {
    DCHECK_NOT_NULL(b);
    if (loop.body_set.contains(b)) continue;
    if (b->is_dead()) continue;
    bool uses_body_value = false;
    b->ForEachNodeAndControl([&](NodeBase* n) {
      for (int i = 0; i < n->input_count(); ++i) {
        if (is_body_value(n->input(i).node())) uses_body_value = true;
      }
      // Deopt frames also reference body values; a downstream frame-state
      // reference to a body value breaks dominance on the body-skipped path
      // just like a regular input would.
      if (n->properties().has_eager_deopt_info()) {
        n->eager_deopt_info()->ForEachInput([&](ValueNode*& slot) {
          if (is_body_value(slot)) uses_body_value = true;
        });
      }
      if (n->properties().can_lazy_deopt()) {
        n->lazy_deopt_info()->ForEachInput([&](ValueNode*& slot) {
          if (is_body_value(slot)) uses_body_value = true;
        });
      }
    });
    if (uses_body_value) {
      TRACE_PEEL_SKIP("@" << header_offset
                          << ": skip (body value used downstream)");
      return false;
    }
  }

  return true;
}

// Peels the first iteration of a loop. For every control transfer leaving
// the loop body (the "exit edge"), the peeler joins the *cloned* (peeled)
// and the *original* iteration's exit-side at a dedicated Peel Exit Merge
// (PEM) block before reaching the original target.
//
// ─────────────────── Before peeling ───────────────────
//
//                ┌─────────────┐
//                │  Preheader  │
//                └──────┬──────┘
//                       │
//                       ▼
//                ┌─────────────┐   ◄────┐
//                │    Header   │        │
//                └──┬───────┬──┘        │
//             body  │       │ exit      │
//                   ▼       ▼           │  backedge
//                ┌──────┐  ┌─────────┐  │
//                │ Body │  │ LoopExit│  │
//                └──┬───┘  └─────────┘  │
//                   │                   │
//                   ▼                   │
//                ┌──────────┐           │
//                │ Backedge ├───────────┘
//                └──────────┘
//
// ─────────────────── After peeling ────────────────────
//
// (The diagram below shows the single-exit case for clarity. For a
//  multi-exit loop, the same per-exit structure repeats independently
//  for each exit edge. See note below.)
//
//          ┌─────────────┐
//          │  Preheader  │
//          └──────┬──────┘
//                 │
//                 ▼
//          ┌─────────────────┐
//          │  Cloned Header  │  (the peeled iteration)
//          └──┬───────────┬──┘
//       body  │           │ exit
//             ▼           ▼
//        ┌────────┐  ┌─────────┐
//        │ Cloned │  │  Edge   │
//        │  Body  │  │ Split A │
//        └────┬───┘  └────┬────┘
//             │           |
//             ▼           |
//        ┌──────────┐     └─────────┐
//        │  Cloned  │               |
//        │ Backedge │               |
//        └────┬─────┘               |
//             │                     |
//             ▼                     |
//  ┌────►┌─────────────┐            |
//  |     │    Header   │   (the original loop;
//  |     └──┬───────┬──┘    runs at most N - 1 more times)
//  |  body  │       │ exit          |
//  |        ▼       ▼               |
//  |      ┌──────┐  ┌─────────┐     |
//  |      │ Body │  │  Edge   │     |
//  |      └──┬───┘  │ Split B │     |
//  | back    │      └────┬────┘     |
//  | edge    ▼           |          |
//  |      ┌──────────┐   |          |
//  └──────┤ Backedge │   |          |
//         └──────────┘   |          |
//                        |          |
//                        ▼          ▼
//                ┌──────────────────────────┐
//                │            PEM           │ ◄─── 2-pred merge.
//                │  pred 0 ← Edge Split A   │      Phis merge
//                │  pred 1 ← Edge Split B   │      values from
//                └────────────┬─────────────┘      both paths.
//                             │
//                             ▼
//                        ┌──────────┐
//                        │ LoopExit │
//                        └──────────┘
//
// Multi-exit note: exits that share the same downstream target SHARE
// a PEM. For K exits all going to the same merge target T, the peeler
// builds ONE PEM with 2K predecessors and 2K-input phis.
void MaglevLoopPeeler::PeelLoop(const LoopInfo& loop) {
  PeelContext ctx(loop, zone());

  CloneBodySubgraph(ctx);
  BuildPeelExitMerge(ctx);
  WireExitControl(ctx);
  CloneLoopBodyControl(ctx);
  RewireDownstreamPhiRefs(ctx);
  RetargetOriginalLoopExits(ctx);
  RewireLoopConnections(ctx);
  SplicePeeledBlocks(ctx);

  TRACE_PEEL_PEELED("@" << LoopHeaderBytecodeOffset(loop.header())
                        << ": peeled (body=" << loop.body.size()
                        << " block(s), exits=" << loop.exits.size());
}

// Allocate one cloned BB per body block, then clone internal merge-block phis
// and non-control nodes. Populates ctx.block_map and ctx.value_map (with header
// phis pre-seeded to their pre-header inputs).
void MaglevLoopPeeler::CloneBodySubgraph(PeelContext& ctx) {
  // Header phis resolve to their pre-header input.
  if (ctx.loop.header()->has_phi()) {
    for (Phi* phi : *ctx.loop.header()->phis()) {
      ctx.value_map[phi] = phi->input(0).node();
    }
  }

  // Allocate one cloned block per body block, in RPO order.
  for (BasicBlock* block : ctx.loop.body) {
    if (block == ctx.loop.header()) {
      BasicBlock** preds = zone()->AllocateArray<BasicBlock*>(1);
      preds[0] = ctx.loop.preheader;
      MergePointInterpreterFrameState* state =
          MergePointInterpreterFrameState::NewForPeel(
              block->state()->unit(), *block->state(), preds, 1);
      BasicBlock* cloned_block = zone()->New<BasicBlock>(state, zone());
      ctx.block_map[block] = cloned_block;
    } else if (block->is_merge_block()) {
      int predecessor_count = block->predecessor_count();
      BasicBlock** preds =
          zone()->AllocateArray<BasicBlock*>(predecessor_count);
      for (int i = 0; i < predecessor_count; ++i) {
        preds[i] = ctx.block_map.at(block->predecessor_at(i));
      }
      MergePointInterpreterFrameState* state =
          MergePointInterpreterFrameState::NewForPeel(block->state()->unit(),
                                                      *block->state(), preds,
                                                      predecessor_count);
      ctx.block_map[block] = zone()->New<BasicBlock>(state, zone());
    } else {
      // kOther or kEdgeSplit: single predecessor.
      BasicBlock* cloned_block =
          zone()->New<BasicBlock>(/* state */ nullptr, zone());
      BasicBlock* pred = block->predecessor();
      DCHECK_NOT_NULL(pred);
      cloned_block->set_predecessor(ctx.block_map.at(pred));
      ctx.block_map[block] = cloned_block;
    }
  }

  // Clone nodes per block.
  for (BasicBlock* block : ctx.loop.body) {
    BasicBlock* dst_block = ctx.block_map[block];
    // Clone phis at internal merge blocks.
    if (block != ctx.loop.header() && block->has_phi()) {
      for (Phi* phi : *block->phis()) {
        DCHECK(!phi->is_exception_phi());
        int input_count = phi->input_count();
        Phi* clone_phi = NodeBase::New<Phi>(zone(), input_count,
                                            dst_block->state(), phi->owner());
        for (int i = 0; i < input_count; ++i) {
          clone_phi->set_input(i, Remap(ctx.value_map, phi->input(i).node()));
        }
        dst_block->state()->phis()->Add(clone_phi);
        ctx.value_map[phi] = clone_phi;
        RegisterClonedNode(graph_, clone_phi, phi);
      }
    }
    dst_block->nodes().reserve(block->nodes().size());
    for (Node*& node : block->nodes()) {
      if (node == nullptr) continue;
      // This loop is being peeled now. A %AssertPeeled marker asserts exactly
      // that, so drop it from the original body (so it never reaches the
      // Turbolev backend) and don't clone it into the peeled iteration. A
      // %AssertNotPeeled marker asserts the opposite, so peeling violates it.
      if (auto* assert_peeled = node->TryCast<AssertPeeled>()) {
        if (!assert_peeled->expect_peeled()) {
          FATAL("%%AssertNotPeeled: loop was peeled");
        }
        node = nullptr;
        continue;
      }
      if (IsSkippableInPeel(node)) continue;
      // Don't clone Identity nodes; fold each one away by mapping it to its
      // underlying value. That value may itself be defined earlier in the body
      // and thus already cloned, so Remap it to the clone (Remap leaves
      // loop-invariant values unchanged).
      if (node->Is<Identity>()) {
        ValueNode* identity = node->Cast<ValueNode>();
        ctx.value_map[identity] =
            Remap(ctx.value_map, identity->UnwrapIdentities());
        continue;
      }
      Node* cloned = CloneNodeWithRemap(node, ctx.value_map, zone());
      cloned->set_owner(dst_block);
      RemapClonedExceptionHandlerInfo(node, cloned, dst_block);
      dst_block->nodes().push_back(cloned);
      if (auto* vn = cloned->TryCast<ValueNode>()) {
        ctx.value_map[node->Cast<ValueNode>()] = vn;
      }
      RegisterClonedNode(graph_, cloned, node);
    }
    // Control nodes are cloned later, in CloneLoopBodyControl, because a
    // cloned control node may target blocks/merges not created yet.
  }
}

// For each exit edge, allocate edge-split blocks (Branch sources only) and a
// single shared PEM. The PEM gets one phi per loop-header phi, with the same
// owner: for every exit it merges the loop-entry value (the header phi's
// pre-header input, on the peeled path) with the header phi itself (on the
// original path). Mirroring the header phis this way lets the downstream rewire
// redirect each header-phi use to its PEM phi. All exits target the same no-phi
// block (BuildLoopInfo guarantees this). Populates ctx.exit_edge_splits,
// ctx.pem and ctx.exit_target.
//
//                     PEM (Peel Exit Merge) block
//             ┌────────────────────────────────────────┐
//             │  Predecessors:                         │
//             │  [0]: Cloned ES 0 (or Cloned Src 0)    │ ◄── peeled path
//             │  [1]: Original ES 0 (or Original Src 0)│ ◄── original path
//             │  [2]: Cloned ES 1 (or Cloned Src 1)    │ ◄── peeled path
//             │  [3]: Original ES 1 (or Original Src 1)│ ◄── original path
//             │  ...                                   │
//             ├────────────────────────────────────────┤
//             │  Phis (pem_phi):                       │
//             │  inputs matching above predecessors:   │
//             │  [2*idx]:   loop-entry value (input 0) │
//             │  [2*idx+1]: loop phi value             │
//             └────────────────────────────────────────┘
void MaglevLoopPeeler::BuildPeelExitMerge(PeelContext& ctx) {
  // Allocate cloned_es / original_es per exit and classify the original side.
  for (size_t i = 0; i < ctx.loop.exits.size(); ++i) {
    const ExitEdge& exit_edge = ctx.loop.exits[i];
    BasicBlock* cloned_src = ctx.block_map[exit_edge.src];
    const bool src_is_branch =
        exit_edge.src->control_node()->Is<BranchControlNode>();
    ExitEdgeSplits& splits = ctx.exit_edge_splits[i];
    if (!src_is_branch) {
      // Jump source: no edge split; the original Jump retargets to the PEM.
      splits.kind = OriginalSplitKind::kNone;
      continue;
    }
    splits.cloned_es = zone()->New<BasicBlock>(/* state */ nullptr, zone());
    splits.cloned_es->set_predecessor(cloned_src);
    if (exit_edge.original_es != nullptr) {
      // Absorb the pre-existing kEdgeSplit on the original side.
      splits.kind = OriginalSplitKind::kExisting;
      splits.original_es = exit_edge.original_es;
    } else {
      // The Branch's exit-side targets a single-predecessor block (split-edge
      // form inserts no kEdgeSplit there). Allocate one, because the post-peel
      // PEM is a merge that the original Branch now feeds. (A non-merge target
      // has a single predecessor inherently; a merge target must have exactly
      // one, else split-edge form would have inserted a kEdgeSplit.)
      DCHECK(!exit_edge.target->has_state() ||
             exit_edge.target->predecessor_count() == 1);
      splits.kind = OriginalSplitKind::kNew;
      splits.original_es = zone()->New<BasicBlock>(/* state */ nullptr, zone());
      splits.original_es->set_predecessor(exit_edge.src);
    }
  }

  // Initialize the single exit target. BuildLoopInfo guarantees
  // at least one exit, every exit's target is no-phi, and all share the
  // same target.
  DCHECK(!ctx.loop.exits.empty());
  ctx.exit_target = ctx.loop.exits[0].target;
#ifdef DEBUG
  for (size_t i = 0; i < ctx.loop.exits.size(); ++i) {
    DCHECK(!ctx.loop.exits[i].target->has_phi());
    DCHECK_EQ(ctx.exit_target, ctx.loop.exits[i].target);
  }
#endif  // DEBUG

  // Build the PEM with header-phi-mirror phis.
  const int exit_edge_count = static_cast<int>(ctx.loop.exits.size());
  BasicBlock** pem_preds =
      zone()->AllocateArray<BasicBlock*>(2 * exit_edge_count);
  // The PEM's predecessors alternate per exit: slot 2*idx is the peeled side,
  // 2*idx+1 the original side. pem_pred_id tracks the lowest slot the exits
  // occupy in a (merge) exit target; that becomes the PEM's anchor slot.
  int pem_pred_id = std::numeric_limits<int>::max();
  for (int idx = 0; idx < exit_edge_count; ++idx) {
    const ExitEdge& exit_edge = ctx.loop.exits[idx];
    const ExitEdgeSplits& exit_splits = ctx.exit_edge_splits[idx];
    if (exit_splits.kind == OriginalSplitKind::kNone) {
      DCHECK_NULL(exit_splits.cloned_es);
      DCHECK_NULL(exit_splits.original_es);
      pem_preds[2 * idx] = ctx.block_map[exit_edge.src];
      pem_preds[2 * idx + 1] = exit_edge.src;
    } else {
      DCHECK_NOT_NULL(exit_splits.cloned_es);
      DCHECK_NOT_NULL(exit_splits.original_es);
      pem_preds[2 * idx] = exit_splits.cloned_es;
      pem_preds[2 * idx + 1] = exit_splits.original_es;
    }
    if (exit_edge.predecessor_id >= 0) {
      pem_pred_id = std::min(pem_pred_id, exit_edge.predecessor_id);
    }
  }
  // The PEM mirrors the loop header's frame layout: its phis must line up with
  // the header phis so a downstream use redirected from a header phi to its PEM
  // phi resolves to the same owner. The values copied from the header are valid
  // at the exit too — phi'd registers are overwritten by the pem_phis below,
  // and non-phi registers are loop-invariant (same value at header and exit).
  // Liveness is copied from the header, a conservative superset of the exit's
  // (it may over-extend a few lifetimes but is never wrong).
  MergePointInterpreterFrameState* pem_state =
      MergePointInterpreterFrameState::NewForPeel(
          ctx.loop.header()->state()->unit(), *ctx.loop.header()->state(),
          pem_preds, 2 * exit_edge_count);
  ctx.pem = zone()->New<BasicBlock>(pem_state, zone());
  DCHECK_IMPLIES(ctx.exit_target->has_state(),
                 pem_pred_id != std::numeric_limits<int>::max());
  ctx.pem_pred_id = ctx.exit_target->has_state() ? pem_pred_id : 0;
  if (ctx.loop.header()->has_phi()) {
    for (Phi* phi : *ctx.loop.header()->phis()) {
      DCHECK(!phi->is_exception_phi());
      // The peeled-path value of a header phi is its pre-header input — exactly
      // what CloneBodySubgraph seeds into value_map for the header phis.
      DCHECK_EQ(Remap(ctx.value_map, phi), phi->input(0).node());
      Phi* pem_phi = NodeBase::New<Phi>(zone(), 2 * exit_edge_count, pem_state,
                                        phi->owner());
      for (int idx = 0; idx < exit_edge_count; ++idx) {
        pem_phi->set_input(2 * idx, phi->input(0).node());
        pem_phi->set_input(2 * idx + 1, phi);
      }
      pem_state->phis()->Add(pem_phi);
      RegisterIfLabelled(graph_, pem_phi);
      ctx.header_phi_to_pem_phi[phi] = pem_phi;
    }
  }
}

// Emit Jumps that wire each edge-split into the PEM, and emit the
// PEM's outgoing Jump to its target.
//
//  Peeled (Cloned) iteration       Original iteration
//     ┌──────────────┐             ┌──────────────┐
//     │  Cloned Src  │             │ Original Src │
//     └──────┬───────┘             └──────┬───────┘
//            │ (Branch)                   │ (Branch)
//            ▼                            ▼
//     ┌──────────────┐             ┌──────────────┐
//     │  Cloned ES   │             │ Original ES  │
//     └──────┬───────┘             └──────┬───────┘
//            │                            │
//            │ Jump (pred 2*idx)          │ Jump (pred 2*idx + 1)
//            └─────────────┐  ┌───────────┘
//                          ▼  ▼
//                    ┌────────────┐
//                    │    PEM     │
//                    └─────┬──────┘
//                          │ Jump (pred pem_pred_id)
//                          ▼
//                    ┌────────────┐
//                    │Exit Target │
//                    └────────────┘
void MaglevLoopPeeler::WireExitControl(PeelContext& ctx) {
  auto build_jump = [&](BasicBlock* from, BasicBlock* to, int predecessor_id) {
    BasicBlockRef* ref = zone()->New<BasicBlockRef>();
    Jump* jmp = NodeBase::New<Jump>(zone(), 0, ref);
    ref->Bind(to);
    jmp->set_owner(from);
    jmp->set_predecessor_id(predecessor_id);
    from->set_control_node(jmp);
    RegisterIfLabelled(graph_, jmp);
    return jmp;
  };

  // Wire the per-exit edge-splits into the PEM. Predecessor slots alternate:
  // the peeled side at 2*i, the original side at 2*i + 1. kNone (Jump source)
  // has no edge split here; its original Jump is retargeted to the PEM in
  // RetargetOriginalLoopExits.
  for (size_t i = 0; i < ctx.loop.exits.size(); ++i) {
    const ExitEdge& exit_edge = ctx.loop.exits[i];
    const ExitEdgeSplits& exit_splits = ctx.exit_edge_splits[i];
    DCHECK_NOT_NULL(ctx.pem);
    if (exit_splits.kind == OriginalSplitKind::kNone) continue;

    const int peeled_slot = 2 * static_cast<int>(i);
    BasicBlock* cloned_src = ctx.block_map[exit_edge.src];
    DCHECK_NOT_NULL(exit_splits.cloned_es);
    DCHECK_NOT_NULL(exit_splits.original_es);

    // Peeled side: cloned_es -> PEM at peeled_slot.
    build_jump(exit_splits.cloned_es, ctx.pem, peeled_slot);
    exit_splits.cloned_es->set_edge_split_block(cloned_src);

    // Original side: original_es -> PEM at peeled_slot + 1. For a freshly
    // allocated split (kNew) emit the Jump now; for an absorbed existing
    // kEdgeSplit (kExisting) retarget its Jump. (Mutations of the ORIGINAL
    // Branch's exit-side are deferred to RetargetOriginalLoopExits.)
    if (exit_splits.kind == OriginalSplitKind::kNew) {
      build_jump(exit_splits.original_es, ctx.pem, peeled_slot + 1);
      exit_splits.original_es->set_edge_split_block(exit_edge.src);
    } else {
      DCHECK_EQ(exit_splits.kind, OriginalSplitKind::kExisting);
      DCHECK(exit_splits.original_es->control_node()->Is<Jump>());
      Jump* existing_jmp =
          exit_splits.original_es->control_node()->Cast<Jump>();
      existing_jmp->set_target(ctx.pem);
      existing_jmp->set_predecessor_id(peeled_slot + 1);
    }
  }

  // PEM → target Jump. predecessor_id is 0 for kEdgeSplit/kOther targets,
  // and the group's pem_pred_id for kMerge no-phi targets (after
  // compaction the PEM occupies that slot).
  build_jump(ctx.pem, ctx.exit_target,
             ctx.exit_target->has_state() ? ctx.pem_pred_id : 0);
}

// Clone the control nodes of all body blocks in the cloned body, and
// apply the deferred original-source retargets so that the original
// exits now point to original_es / PEM.
void MaglevLoopPeeler::CloneLoopBodyControl(PeelContext& ctx) {
  auto clone_control_and_remap_inputs =
      [&](ControlNode* control) -> ControlNode* {
    ControlNode* clone = nullptr;
    switch (control->opcode()) {
#define CLONE_CONTROL_CASE(Name)                                     \
  case Opcode::k##Name:                                              \
    clone = NodeBase::CloneRaw<Name>(control->Cast<Name>(), zone()); \
    break;
      CONDITIONAL_CONTROL_NODE_LIST(CLONE_CONTROL_CASE)
      UNCONDITIONAL_CONTROL_NODE_LIST(CLONE_CONTROL_CASE)
      TERMINAL_CONTROL_NODE_LIST(CLONE_CONTROL_CASE)
#undef CLONE_CONTROL_CASE
      default:
        UNREACHABLE();
    }
    CHECK_NOT_NULL(clone);
    for (int i = 0; i < clone->input_count(); ++i) {
      clone->set_input(i, Remap(ctx.value_map, control->input(i).node()));
    }
    RemapClonedDeoptInfo(control, clone, ctx.value_map, zone());
    return clone;
  };

  struct ClonedExit {
    BasicBlock* target;
    int predecessor_id;
  };
  auto find_exit_target = [&](BasicBlock* block,
                              BasicBlock* original_target) -> ClonedExit {
    for (size_t i = 0; i < ctx.loop.exits.size(); ++i) {
      const ExitEdge& exit_edge = ctx.loop.exits[i];
      if (exit_edge.src == block) {
        BasicBlock* immediate_orig_target = (exit_edge.original_es != nullptr)
                                                ? exit_edge.original_es
                                                : exit_edge.target;
        if (immediate_orig_target == original_target) {
          const ExitEdgeSplits& exit_splits = ctx.exit_edge_splits[i];
          if (exit_splits.kind != OriginalSplitKind::kNone) {
            return {exit_splits.cloned_es, 0};
          }
          return {ctx.pem, 2 * static_cast<int>(i)};
        }
      }
    }
    UNREACHABLE();
  };

  auto retarget_successors = [&](BasicBlock* block, ControlNode* control,
                                 ControlNode* cloned_control) {
    auto target_for = [&](BasicBlock* original_target) -> BasicBlock* {
      auto it = ctx.block_map.find(original_target);
      if (it != ctx.block_map.end()) return it->second;
      return find_exit_target(block, original_target).target;
    };

    if (control->Is<BranchControlNode>()) {
      auto* original_br = control->Cast<BranchControlNode>();
      auto* cloned_br = cloned_control->Cast<BranchControlNode>();
      cloned_br->set_if_true(target_for(original_br->if_true()));
      cloned_br->set_if_false(target_for(original_br->if_false()));
    } else if (control->Is<UnconditionalControlNode>()) {
      auto* uc = control->Cast<UnconditionalControlNode>();
      auto* clone_uc = cloned_control->Cast<UnconditionalControlNode>();
      auto it = ctx.block_map.find(uc->target());
      if (it != ctx.block_map.end()) {
        clone_uc->set_target(it->second);
      } else {
        // Exit edge: target the PEM/edge-split at this exit's slot, not slot 0.
        ClonedExit cloned_exit = find_exit_target(block, uc->target());
        clone_uc->set_target(cloned_exit.target);
        clone_uc->set_predecessor_id(cloned_exit.predecessor_id);
      }
    } else if (control->Is<TerminalControlNode>()) {
      // Terminal control nodes have no successors, nothing to retarget.
    } else {
      UNREACHABLE();
    }
  };

  // Emit control for each cloned body block. We do this after node-cloning
  // because the cloned body's control may reference the tail merge.
  for (BasicBlock* block : ctx.loop.body) {
    BasicBlock* cloned_block = ctx.block_map[block];
    ControlNode* control = block->control_node();
    ControlNode* cloned_control = nullptr;

    if (block == ctx.loop.backedge()) {
      // Back-edge: original is JumpLoop. Replace with a CheckpointedJump (not a
      // plain Jump) targeting the original loop header directly at predecessor
      // slot 0 (the forward pre-header slot). The header's pre-header phi
      // inputs get rewired below to consume the cloned-backedge values. It must
      // stay checkpointed: JumpLoop carries a deopt checkpoint that phi
      // untagging depends on -- MaglevPhiRepresentationSelector hoists loop-phi
      // untagging onto the forward predecessor and unconditionally casts its
      // control node to CheckpointedJump. Clone the JumpLoop's checkpoint frame
      // (remapped to the peeled values) into the new node.
      JumpLoop* orig = control->Cast<JumpLoop>();
      BasicBlockRef* ref = zone()->New<BasicBlockRef>();
      CheckpointedJump* jump = NodeBase::New<CheckpointedJump>(zone(), 0, ref);
      ref->Bind(ctx.loop.header());
      jump->set_predecessor_id(0);
      DeoptFrame* top = CloneDeoptFrame(&orig->eager_deopt_info()->top_frame(),
                                        ctx.value_map, zone());
      jump->SetEagerDeoptInfo(zone(), top,
                              orig->eager_deopt_info()->feedback_to_update());
      cloned_control = jump;
    } else {
      cloned_control = clone_control_and_remap_inputs(control);
      retarget_successors(block, control, cloned_control);
    }

    DCHECK_NOT_NULL(cloned_control);
    cloned_control->set_owner(cloned_block);
    cloned_block->set_control_node(cloned_control);
    // The clone was allocated as a plain kOther block; restore edge-split-ness
    // now that its Jump control is set.
    if (block->is_edge_split_block()) {
      cloned_block->set_edge_split_block(cloned_block->predecessor());
    }
    RegisterClonedNode(graph_, cloned_control, control);
  }
}

// Apply deferred mutations to the original loop's exit control nodes (original
// Jumps and Branches) now that all cloned body blocks and their control nodes
// have been fully emitted. We must defer these mutations until this point so
// that CloneLoopBodyControl reads the original control flow topology instead
// of the mutated exit topology.
//
// Before retargeting:
//   Branch case:                 Jump case:
//     [Src] ──► [Target]           [Src] ──► [Target] (Jump)
//
// After retargeting:
//   Branch case (with fresh ES):  Jump case (retarget to PEM):
//     [Src] ──► [Original ES]      [Src] ──► [PEM] (Jump)
void MaglevLoopPeeler::RetargetOriginalLoopExits(PeelContext& ctx) {
  for (size_t i = 0; i < ctx.loop.exits.size(); ++i) {
    const ExitEdge& exit_edge = ctx.loop.exits[i];
    const ExitEdgeSplits& edge_splits = ctx.exit_edge_splits[i];
    switch (edge_splits.kind) {
      case OriginalSplitKind::kNew:
        // Freshly allocated original_es: retarget the Branch's exit-side to it.
        DCHECK_NOT_NULL(edge_splits.original_es);
        RetargetControlSuccessors(exit_edge.src->control_node(),
                                  exit_edge.target, edge_splits.original_es);
        break;
      case OriginalSplitKind::kNone: {
        // Jump source: retarget the original Jump from e.target to the PEM at
        // its original-side slot (2*i + 1).
        UnconditionalControlNode* uc =
            exit_edge.src->control_node()->Cast<UnconditionalControlNode>();
        uc->set_target(ctx.pem);
        uc->set_predecessor_id(2 * static_cast<int>(i) + 1);
        break;
      }
      case OriginalSplitKind::kExisting:
        // The absorbed kEdgeSplit's Jump was already retargeted to the PEM in
        // WireExitControl; the original Branch's exit-side still points at the
        // kEdgeSplit, which is correct. Nothing to do.
        break;
    }
  }
}

// Rewire connections from the original loop to the new peeled loop structure:
// 1. Connect the cloned backedge block to predecessor slot 0 of the original
// loop header.
// 2. Funnel all exiting edges to the Peeled Exit Merge (PEM) block at the exit
// target.
// 3. Retarget the pre-header block's control flow to the cloned loop header.
//
// Before rewiring:
//      ┌─────────────┐
//      │  Preheader  │────────┐
//      └─────────────┘        │
//                             ▼
//                      ┌─────────────┐
//                      │   Header    │
//                      └─────────────┘
//
// After rewiring:
//      ┌─────────────┐
//      │  Preheader  │
//      └──────┬──────┘
//             │
//             ▼
//      ┌─────────────┐
//      │Cloned Header│
//      └─────────────┘
//            ...
//      ┌─────────────┐
//      │ Cloned Back │────────┐
//      └─────────────┘        │
//                             ▼ (slot 0)
//                      ┌─────────────┐
//                      │   Header    │
//                      └─────────────┘
void MaglevLoopPeeler::RewireLoopConnections(PeelContext& ctx) {
  BasicBlock* clone_back = ctx.block_map[ctx.loop.backedge()];
  ctx.loop.header()->state()->set_predecessor_at(0, clone_back);

  // Update the no-phi target (at most one target block):
  //   - kEdgeSplit/kOther (no merge state): set_predecessor(PEM).
  //   - kMerge: We replace the multiple original loop-exit predecessors
  //     with the single funneling PEM block. To avoid index shifting issues
  //     when calling `RemovePredecessorAt`, we sort the slots in descending
  //     order and anchor the PEM at the lowest predecessor ID (pem_pred_id).
  if (!ctx.exit_target->has_state()) {
    ctx.exit_target->set_predecessor(ctx.pem);
  } else {
    ZoneVector<int> exit_slots(zone());
    exit_slots.reserve(ctx.loop.exits.size());
    std::ranges::transform(ctx.loop.exits, std::back_inserter(exit_slots),
                           [](const auto& e) {
                             DCHECK_GE(e.predecessor_id, 0);
                             return e.predecessor_id;
                           });
    std::ranges::sort(exit_slots, std::greater<int>());
    ctx.exit_target->state()->set_predecessor_at(ctx.pem_pred_id, ctx.pem);
    for (int pred_id : exit_slots) {
      if (pred_id != ctx.pem_pred_id) {
        ctx.exit_target->state()->RemovePredecessorAt(pred_id);
      }
    }
  }

  // Retarget the pre-header's control flow to the cloned loop header block.
  RetargetControlSuccessors(ctx.loop.preheader->control_node(),
                            ctx.loop.header(),
                            ctx.block_map[ctx.loop.header()]);
}

// Swaps downstream references to the original loop header phis with the
// corresponding Peeled Exit Merge (PEM) phis. We skip the original loop
// body blocks because they must continue to reference local values. (Cloned
// body blocks and the PEM are not yet spliced into the graph, so they are
// naturally skipped).
//
// Before rewiring:
//
//      [Preheader]           [Backedge]
//           │                    │
//           ▼                    ▼
//     ┌────────────────────────────────┐
//     │ Loop Header                    │
//     │  old_phi = Phi(pre_val, back) ◄┼─── Used inside loop body and
//     └────────────────────────────────┘    downstream.
//
// After rewiring:
//
//     ┌────────────────────────────────┐
//     │ Loop Header                    │
//     │  loop_phi = Phi(cloned, back) ◄┼─── Used only inside loop body
//     └────────────────────────────────┘
//
//     ┌────────────────────────────────────┐
//     │ PEM Block                          │
//     │  pem_phi = Phi(pre_val, loop_phi) ◄┼─ Merges peeled and loop values
//     │  old_phi = Identity(pem_phi)      ◄┼─── Satisfies all downstream
//     └────────────────────────────────────┘    uses of old_phi.
void MaglevLoopPeeler::RewireDownstreamPhiRefs(PeelContext& ctx) {
  if (ctx.header_phi_to_pem_phi.empty()) return;

  struct PhiRewire {
    Phi* old_phi;
    Phi* pem_phi;
    Phi* loop_phi;
  };
  ZoneVector<PhiRewire> rewires(zone());
  rewires.reserve(ctx.header_phi_to_pem_phi.size());
  PhiMap replacements(zone());

  // After peeling, the loop header has exactly two predecessors regardless of
  // the original phi's arity: a loop can have several forward edges but only
  // one back-edge, so the peeled loop's only predecessors are the peeled
  // back-edge (slot 0) and the original back-edge (slot 1). Every loop phi
  // therefore has two inputs after peeling.
  static constexpr int kPhiInputCount = 2;

  // Create a new loop phi for each header phi and collect the rewires.
  for (auto& [old_phi, pem_phi] : ctx.header_phi_to_pem_phi) {
    Phi* loop_phi = NodeBase::New<Phi>(
        zone(), kPhiInputCount, old_phi->merge_state(), old_phi->owner());
    // Slot 0 takes the peeled clone of the back-edge value (via value_map);
    // slot 1 the original back-edge value. If the back-edge value is itself a
    // header phi (a recursive loop phi), slot 1 still points at the old phi
    // here; it is rewired to the matching loop phi in the cross-reference pass
    // below, once every old->loop phi mapping has been recorded.
    loop_phi->set_input(0,
                        Remap(ctx.value_map, old_phi->backedge_input().node()));
    loop_phi->set_input(1, old_phi->backedge_input().node());
    RegisterIfLabelled(graph_, loop_phi);
    replacements[old_phi] = loop_phi;
    rewires.push_back({old_phi, pem_phi, loop_phi});
  }

  // Fix loop-phi cross-references (including self-references) now that the
  // combined map is complete.
  for (const PhiRewire& r : rewires) {
    RewireNodeInputs(r.loop_phi, replacements);
  }

  // Rewrite all references inside the loop body blocks from the old header
  // phis to their loop phis in a single walk. Body blocks are never dead:
  // CloneBodySubgraph and CloneLoopBodyControl already clone every body block
  // unconditionally, so a dead one would have broken earlier.
  for (BasicBlock* block : ctx.loop.body) {
    DCHECK(!block->is_dead());
    block->ForEachNodeAndControl([&](NodeBase* n) {
      RewireNodeInputs(n, replacements);
      if (n->properties().has_eager_deopt_info()) {
        RewireDeoptFramePhiRefs(n->eager_deopt_info(), replacements);
      }
      if (n->properties().can_lazy_deopt()) {
        RewireDeoptFramePhiRefs(n->lazy_deopt_info(), replacements);
      }
    });
  }

  // Downstream deopt frames reference the old header phi (soon an Identity to
  // the PEM phi). Later passes can't move deopt-frame uses off an Identity, so
  // point them straight at the PEM phi to keep it live.
  for (BasicBlock* block : graph_->blocks()) {
    if (block->is_dead() || ctx.loop.body_set.contains(block)) continue;
    block->ForEachNodeAndControl([&](NodeBase* n) {
      if (n->properties().has_eager_deopt_info()) {
        RewireDeoptFramePhiRefs(n->eager_deopt_info(),
                                ctx.header_phi_to_pem_phi);
      }
      if (n->properties().can_lazy_deopt()) {
        RewireDeoptFramePhiRefs(n->lazy_deopt_info(),
                                ctx.header_phi_to_pem_phi);
      }
    });
  }

  // Every header phi is being replaced, so drop them all at once rather than
  // removing each one individually (which would be O(n^2)).
  ctx.loop.header()->state()->phis()->Clear();

  // Finalize each rewire: redirect the PEM phi, turn the old header phi into
  // an Identity to the PEM phi, move it into the PEM block, and install the new
  // loop phi on the header.
  for (const PhiRewire& r : rewires) {
    // Rewrite the PEM phi's inputs to point to loop_phi instead of old_phi.
    for (int i = 0; i < r.pem_phi->input_count(); ++i) {
      if (r.pem_phi->input(i).node() == r.old_phi) {
        r.pem_phi->change_input(i, r.loop_phi);
      }
    }

    // Overwrite old_phi with an Identity node pointing to pem_phi. The PEM has
    // no nodes yet, so append at the back (cheaper than inserting at the
    // front).
    r.old_phi->OverwriteWithIdentityTo(r.pem_phi);
    r.old_phi->set_owner(ctx.pem);
    ctx.pem->nodes().push_back(static_cast<Node*>(r.old_phi));

    // Add loop_phi to the loop header phis list.
    ctx.loop.header()->AddPhi(r.loop_phi);
  }
}

// Splice cloned body / edge-splits / PEM into graph_->blocks().
// Cloned body and cloned_es blocks go immediately before the original
// loop header. Freshly-allocated original_es and the PEM go before the
// exit's target.
void MaglevLoopPeeler::SplicePeeledBlocks(PeelContext& ctx) {
  ZoneVector<BasicBlock*> new_blocks_before_header(zone());
  new_blocks_before_header.reserve(ctx.loop.body.size() +
                                   ctx.loop.exits.size());
  // Cloned body for peeled iteration.
  for (BasicBlock* b : ctx.loop.body) {
    new_blocks_before_header.push_back(ctx.block_map[b]);
  }
  // Cloned exit edge split blocks.
  for (const ExitEdgeSplits& w : ctx.exit_edge_splits) {
    if (w.cloned_es != nullptr) new_blocks_before_header.push_back(w.cloned_es);
  }
  // Freshly-allocated original_es blocks (kNew) aren't in the graph yet; splice
  // them before ctx.exit_target. (kExisting absorbed splits are already in the
  // graph; kNone has no original_es.)
  ZoneVector<BasicBlock*> new_blocks_before_exit_target(zone());
  for (const ExitEdgeSplits& w : ctx.exit_edge_splits) {
    if (w.kind == OriginalSplitKind::kNew) {
      DCHECK_NOT_NULL(w.original_es);
      new_blocks_before_exit_target.push_back(w.original_es);
    }
  }
  // ... and PEM.
  new_blocks_before_exit_target.push_back(ctx.pem);

  auto& blocks = graph_->blocks();
  auto header_it = std::find(blocks.begin(), blocks.end(), ctx.loop.header());
  CHECK_NE(header_it, blocks.end());
  size_t header_idx = std::distance(blocks.begin(), header_it);

  auto preheader_it =
      std::find(blocks.begin(), blocks.end(), ctx.loop.preheader);
  CHECK_NE(preheader_it, blocks.end());
  size_t preheader_idx = std::distance(blocks.begin(), preheader_it);
  DCHECK_LT(preheader_idx, header_idx);
  graph_->AddBlocksAt(new_blocks_before_header, preheader_idx);

  DCHECK_NOT_NULL(ctx.exit_target);
  // Find the exit target in the updated blocks vector. The search can start
  // after the newly inserted blocks.
  auto exit_target_it =
      std::find(blocks.begin() + header_idx + new_blocks_before_header.size(),
                blocks.end(), ctx.exit_target);
  CHECK_NE(exit_target_it, blocks.end());
  size_t exit_target_idx = std::distance(blocks.begin(), exit_target_it);
  graph_->AddBlocksAt(new_blocks_before_exit_target, exit_target_idx - 1);
}

bool MaglevLoopPeeler::Run() {
  bool mutated = false;

  ResolveLoopHeader();

  while (!graph_->peelable_loops().empty()) {
    MaglevLoopPeelInfo info = graph_->peelable_loops().back();
    graph_->peelable_loops().pop_back();

    TRACE_PEEL("candidate offset=" << info.loop_header_offset
                                   << " size=" << info.bytecode_size);

    if (info.header == nullptr || info.header->is_dead()) {
      TRACE_PEEL_SKIP("@" << info.loop_header_offset
                          << ": skip (no matching loop header in post-inlining "
                             "graph)");
      continue;
    }

    std::optional<LoopInfo> loop = BuildLoopInfo(info.header);
    if (!loop) continue;
    if (!IsCloneable(*loop)) continue;
    PeelLoop(*loop);
    mutated = true;
  }
  return mutated;
}

}  // namespace v8::internal::maglev
