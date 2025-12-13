// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph.h"

#include <vector>

#include "src/compiler/js-heap-broker-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/objects/objects-inl.h"

namespace v8::internal::maglev {

Constant* Graph::GetHeapNumberConstant(double constant) {
  uint64_t bits = Float64(constant).get_bits();
  auto it = heap_number_constants_.find(bits);
  if (it == heap_number_constants_.end()) {
    Handle<HeapNumber> number =
        broker()
            ->local_isolate()
            ->factory()
            ->NewHeapNumberFromBits<AllocationType::kOld>(bits);
    compiler::HeapNumberRef number_ref =
        MakeRef(broker(), broker()->CanonicalPersistentHandle(number));
    Constant* node = CreateNewConstantNode<Constant>(0, number_ref);
    heap_number_constants_.emplace(bits, node);
    return node;
  }
  return it->second;
}

compiler::OptionalScopeInfoRef Graph::TryGetScopeInfoForContextLoad(
    ValueNode* context, int offset) {
  compiler::OptionalScopeInfoRef cur = TryGetScopeInfo(context);
  if (offset == Context::OffsetOfElementAt(Context::EXTENSION_INDEX)) {
    return cur;
  }
  CHECK_EQ(offset, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX));
  if (cur.has_value()) {
    cur = (*cur).OuterScopeInfo(broker());
    while (!cur->HasContext() && cur->HasOuterScopeInfo()) {
      cur = cur->OuterScopeInfo(broker());
    }
    if (cur->HasContext()) {
      return cur;
    }
  }
  return {};
}

template <typename Function>
void Graph::IterateGraphAndSweepDeadBlocks(Function&& is_dead) {
  auto current = blocks_.begin();
  auto last_non_dead = current;
  while (current != blocks_.end()) {
    if (is_dead(*current)) {
      (*current)->mark_dead();
    } else {
      if (current != last_non_dead) {
        // Move current to last non dead position.
        *last_non_dead = *current;
      }
      ++last_non_dead;
    }
    ++current;
  }
  if (current != last_non_dead) {
    blocks_.resize(blocks_.size() - (current - last_non_dead));
  }
}

compiler::OptionalScopeInfoRef Graph::TryGetScopeInfo(ValueNode* context) {
  auto it = scope_infos_.find(context);
  if (it != scope_infos_.end()) {
    return it->second;
  }
  compiler::OptionalScopeInfoRef res;
  if (auto context_const = context->TryCast<Constant>()) {
    res = context_const->object().AsContext().scope_info(broker());
    DCHECK(res->HasContext());
  } else if (auto load = context->TryCast<LoadContextSlotNoCells>()) {
    compiler::OptionalScopeInfoRef cur =
        TryGetScopeInfoForContextLoad(load->input(0).node(), load->offset());
    if (cur.has_value()) res = cur;
  } else if (auto load_script = context->TryCast<LoadContextSlot>()) {
    compiler::OptionalScopeInfoRef cur = TryGetScopeInfoForContextLoad(
        load_script->input(0).node(), load_script->offset());
    if (cur.has_value()) res = cur;
  } else if (context->Is<InitialValue>()) {
    // We should only fail to keep track of initial contexts originating from
    // the OSR prequel.
    // TODO(olivf): Keep track of contexts when analyzing OSR Prequel.
    DCHECK(is_osr());
  } else {
    // Any context created within a function must be registered in
    // graph()->scope_infos(). Initial contexts must be registered before
    // BuildBody. We don't track context in generators (yet) and around eval
    // the bytecode compiler creates contexts by calling
    // Runtime::kNewFunctionInfo directly.
    DCHECK(context->Is<Phi>() || context->Is<GeneratorRestoreRegister>() ||
           context->Is<RegisterInput>() || context->Is<CallRuntime>());
  }
  return scope_infos_[context] = res;
}

bool Graph::ContextMayAlias(ValueNode* context,
                            compiler::OptionalScopeInfoRef scope_info) {
  // Distinguishing contexts by their scope info only works if scope infos are
  // guaranteed to be unique.
  // TODO(crbug.com/401059828): reenable when crashes are gone.
  if ((true) || !v8_flags.reuse_scope_infos) return true;
  if (!scope_info.has_value()) {
    return true;
  }
  auto other = TryGetScopeInfo(context);
  if (!other.has_value()) {
    return true;
  }
  return scope_info->equals(*other);
}

void Graph::RemoveUnreachableBlocks() {
  DCHECK(may_have_unreachable_blocks());

  std::vector<bool> reachable_blocks(max_block_id(), false);
  std::vector<BasicBlock*> worklist;

  DCHECK(!blocks().empty());
  BasicBlock* initial_bb = blocks().front();
  worklist.push_back(initial_bb);
  reachable_blocks[initial_bb->id()] = true;
  DCHECK(!initial_bb->is_loop());

  while (!worklist.empty()) {
    BasicBlock* current = worklist.back();
    worklist.pop_back();
    if (current->is_dead()) continue;

    for (auto handler : current->exception_handlers()) {
      if (!handler->HasExceptionHandler()) continue;
      if (handler->ShouldLazyDeopt()) continue;
      BasicBlock* catch_block = handler->catch_block();
      if (catch_block->is_dead()) continue;
      if (!reachable_blocks[catch_block->id()]) {
        reachable_blocks[catch_block->id()] = true;
        worklist.push_back(catch_block);
      }
    }
    current->ForEachSuccessor([&](BasicBlock* succ) {
      if (!reachable_blocks[succ->id()]) {
        reachable_blocks[succ->id()] = true;
        worklist.push_back(succ);
      }
    });
  }

  // Sweep dead blocks and remove unreachable predecessors.
  IterateGraphAndSweepDeadBlocks([&](BasicBlock* bb) {
    if (!reachable_blocks[bb->id()]) return true;
    DCHECK(!bb->is_dead());
    // If block doesn't have a merge state, it has only one predecessor, so
    // it must be the reachable one.
    if (!bb->has_state()) return false;
    if (bb->is_loop() && !reachable_blocks[bb->backedge_predecessor()->id()]) {
      // If the backedge predecessor is not reachable, we can turn the loop
      // into a regular block.
      bb->state()->TurnLoopIntoRegularBlock();
    }
    for (int i = bb->predecessor_count() - 1; i >= 0; i--) {
      if (!reachable_blocks[bb->predecessor_at(i)->id()]) {
        bb->state()->RemovePredecessorAt(i);
      }
    }
    return false;
  });

  set_may_have_unreachable_blocks(false);
}

ValueNode* Graph::GetConstant(compiler::ObjectRef ref) {
  if (ref.IsSmi()) {
    return GetSmiConstant(ref.AsSmi());
  }
  compiler::HeapObjectRef constant = ref.AsHeapObject();

  auto root_index = broker()->FindRootIndex(constant);
  if (root_index.has_value()) {
    return GetRootConstant(*root_index);
  }

  if (constant.IsString()) {
    constant = constant.AsString().UnpackIfThin(broker());
    root_index = broker()->FindRootIndex(constant);
    if (root_index.has_value()) {
      return GetRootConstant(*root_index);
    }
  }

  return GetOrAddNewConstantNode(constants_, ref.AsHeapObject());
}

ValueNode* Graph::GetTrustedConstant(compiler::HeapObjectRef ref,
                                     IndirectPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  auto it = trusted_constants_.find(ref);
  if (it == trusted_constants_.end()) {
    TrustedConstant* node = CreateNewConstantNode<TrustedConstant>(0, ref, tag);
    trusted_constants_.emplace(ref, node);
    return node;
  }
  SBXCHECK_EQ(it->second->tag(), tag);
  return it->second;
#else
  return GetConstant(ref);
#endif
}

}  // namespace v8::internal::maglev
