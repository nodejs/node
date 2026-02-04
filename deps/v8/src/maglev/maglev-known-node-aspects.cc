// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-known-node-aspects.h"

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/handles/handles-inl.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace {

// Destructively intersects the right map into the left map, such that the
// left map is mutated to become the result of the intersection. Values that
// are in both maps are passed to the merging function to be merged with each
// other -- again, the LHS here is expected to be mutated.
template <typename Key, typename Value,
          typename MergeFunc = std::equal_to<Value>>
void DestructivelyIntersect(ZoneMap<Key, Value>& lhs_map,
                            const ZoneMap<Key, Value>& rhs_map,
                            MergeFunc&& func = MergeFunc()) {
  // Walk the two maps in lock step. This relies on the fact that ZoneMaps are
  // sorted.
  typename ZoneMap<Key, Value>::iterator lhs_it = lhs_map.begin();
  typename ZoneMap<Key, Value>::const_iterator rhs_it = rhs_map.begin();
  while (lhs_it != lhs_map.end() && rhs_it != rhs_map.end()) {
    if (lhs_it->first < rhs_it->first) {
      // Remove from LHS elements that are not in RHS.
      lhs_it = lhs_map.erase(lhs_it);
    } else if (rhs_it->first < lhs_it->first) {
      // Skip over elements that are only in RHS.
      ++rhs_it;
    } else {
      // Apply the merge function to the values of the two iterators. If the
      // function returns false, remove the value.
      bool keep_value = func(lhs_it->second, rhs_it->second);
      if (keep_value) {
        ++lhs_it;
      } else {
        lhs_it = lhs_map.erase(lhs_it);
      }
      ++rhs_it;
    }
  }
  // If we haven't reached the end of LHS by now, then we have reached the end
  // of RHS, and the remaining items are therefore not in RHS. Remove them.
  if (lhs_it != lhs_map.end()) {
    lhs_map.erase(lhs_it, lhs_map.end());
  }
}

template <typename Key>
bool NextInIgnoreList(typename ZoneSet<Key>::const_iterator& ignore,
                      typename ZoneSet<Key>::const_iterator& ignore_end,
                      const Key& cur) {
  while (ignore != ignore_end && *ignore < cur) {
    ++ignore;
  }
  return ignore != ignore_end && *ignore == cur;
}

// Takes two ordered maps and ensures that every element in `as` is
//  * also present in `bs` and
//  * `Compare(a, b)` holds for each value.
template <typename As, typename Bs, typename CompareFunction,
          typename IsEmptyFunction = std::nullptr_t>
bool AspectIncludes(const As& as, const Bs& bs, const CompareFunction& Compare,
                    const IsEmptyFunction IsEmpty = nullptr) {
  typename As::const_iterator a = as.begin();
  typename Bs::const_iterator b = bs.begin();
  while (a != as.end()) {
    if constexpr (!std::is_same_v<IsEmptyFunction, std::nullptr_t>) {
      if (IsEmpty(a->second)) {
        ++a;
        continue;
      }
    }
    if (b == bs.end()) return false;
    while (b->first < a->first) {
      ++b;
      if (b == bs.end()) return false;
    }
    if (!(a->first == b->first)) return false;
    if (!Compare(a->second, b->second)) {
      return false;
    }
    ++a;
    ++b;
  }
  return true;
}

// Same as above but allows `as` to contain empty collections as values, which
// do not need to be present in `bs`.
template <typename As, typename Bs, typename Function>
bool MaybeEmptyAspectIncludes(const As& as, const Bs& bs,
                              const Function& Compare) {
  return AspectIncludes<As, Bs, Function>(as, bs, Compare,
                                          [](auto x) { return x.empty(); });
}

template <typename As, typename Bs, typename Function>
bool MaybeNullAspectIncludes(const As& as, const Bs& bs,
                             const Function& Compare) {
  return AspectIncludes<As, Bs, Function>(as, bs, Compare,
                                          [](auto x) { return x == nullptr; });
}

bool NodeInfoIncludes(const NodeInfo& before, const NodeInfo& after) {
  // TODO(428667907): Ideally we should bail out early for the kNone type.
  if (!NodeTypeIs(after.type(), before.type(), NodeTypeIsVariant::kAllowNone)) {
    return false;
  }
  if (before.possible_maps_are_known() && before.any_map_is_unstable()) {
    if (!after.possible_maps_are_known()) {
      return false;
    }
    if (!before.possible_maps().contains(after.possible_maps())) {
      return false;
    }
  }
  return true;
}

bool NodeInfoIsEmpty(const NodeInfo& info) {
  return info.type() == NodeType::kUnknown && !info.possible_maps_are_known();
}

bool NodeInfoTypeIs(const NodeInfo& before, const NodeInfo& after) {
  // TODO(428667907): Ideally we should bail out early for the kNone type.
  return NodeTypeIs(after.type(), before.type(), NodeTypeIsVariant::kAllowNone);
}

bool SameValue(ValueNode* before, ValueNode* after) { return before == after; }

}  // namespace

void KnownNodeAspects::Merge(const KnownNodeAspects& other, Zone* zone) {
  bool any_merged_map_is_unstable = false;
  DestructivelyIntersect(node_infos_, other.node_infos_,
                         [&](NodeInfo& lhs, const NodeInfo& rhs) {
                           lhs.MergeWith(rhs, zone, any_merged_map_is_unstable);
                           return !lhs.no_info_available();
                         });

  if (effect_epoch_ != other.effect_epoch_) {
    effect_epoch_ = std::max(effect_epoch_, other.effect_epoch_) + 1;
  }
  DestructivelyIntersect(
      available_expressions_, other.available_expressions_,
      [&](const AvailableExpression& lhs, const AvailableExpression& rhs) {
        DCHECK_IMPLIES(lhs.node == rhs.node,
                       lhs.effect_epoch == rhs.effect_epoch);
        DCHECK_NE(lhs.effect_epoch, kEffectEpochOverflow);
        DCHECK_IMPLIES(
            !lhs.node->Is<Identity>(),
            Node::needs_epoch_check(lhs.node->opcode()) ==
                (lhs.effect_epoch != kEffectEpochForPureInstructions));
        return !lhs.node->Is<Identity>() && lhs.node == rhs.node &&
               lhs.effect_epoch >= effect_epoch_;
      });

  this->any_map_for_any_node_is_unstable_ = any_merged_map_is_unstable;

  auto merge_loaded_properties =
      [](ZoneMap<ValueNode*, ValueNode*>& lhs,
         const ZoneMap<ValueNode*, ValueNode*>& rhs) {
        // Loaded properties are maps of maps, so just do the destructive
        // intersection recursively.
        DestructivelyIntersect(lhs, rhs);
        return !lhs.empty();
      };
  DestructivelyIntersect(loaded_constant_properties_,
                         other.loaded_constant_properties_,
                         merge_loaded_properties);
  DestructivelyIntersect(loaded_properties_, other.loaded_properties_,
                         merge_loaded_properties);
  DestructivelyIntersect(loaded_context_constants_,
                         other.loaded_context_constants_);
  may_have_aliasing_contexts_ = ContextSlotLoadsAliasMerge(
      may_have_aliasing_contexts_, other.may_have_aliasing_contexts());
  DestructivelyIntersect(loaded_context_slots_, other.loaded_context_slots_);
}

void KnownNodeAspects::UpdateMayHaveAliasingContexts(
    compiler::JSHeapBroker* broker, LocalIsolate* local_isolate,
    ValueNode* context) {
  if (may_have_aliasing_contexts_ == ContextSlotLoadsAlias::kYes) return;

  while (true) {
    if (auto load_prev_ctxt = context->TryCast<LoadContextSlotNoCells>()) {
      DCHECK_EQ(load_prev_ctxt->offset(),
                Context::OffsetOfElementAt(Context::PREVIOUS_INDEX));
      // Recurse until we find the root.
      context = load_prev_ctxt->input(0).node();
      continue;
    }
    break;
  }

  switch (context->opcode()) {
    case Opcode::kInitialValue:
      may_have_aliasing_contexts_ = ContextSlotLoadsAliasMerge(
          may_have_aliasing_contexts_,
          ContextSlotLoadsAlias::kOnlyLoadsRelativeToCurrentContext);
      break;
    case Opcode::kConstant:
      may_have_aliasing_contexts_ = ContextSlotLoadsAliasMerge(
          may_have_aliasing_contexts_,
          ContextSlotLoadsAlias::kOnlyLoadsRelativeToConstant);
      DCHECK(NodeTypeIs(GetType(broker, context), NodeType::kContext));
      break;
    case Opcode::kCreateFunctionContext:
    case Opcode::kInlinedAllocation:
      // These can be precisely tracked.
      DCHECK(NodeTypeIs(GetType(broker, context), NodeType::kContext));
      break;
    case Opcode::kLoadTaggedField: {
      LoadTaggedField* load = context->Cast<LoadTaggedField>();
      // Currently, the only way to leak a context via load tagged field is with
      // JSFunction or JSGeneratorObject.
      USE(load);
      DCHECK(load->offset() == JSFunction::kContextOffset ||
             load->offset() == JSGeneratorObject::kContextOffset);
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kYes;
      DCHECK(NodeTypeIs(GetType(broker, context), NodeType::kContext));
      break;
    }
    case Opcode::kCallRuntime:
      DCHECK(NodeTypeIs(GetType(broker, context), NodeType::kContext));
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kYes;
      break;
    case Opcode::kGeneratorRestoreRegister:
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kYes;
      break;
    case Opcode::kPhi:
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kYes;
      break;
    default:
      // This DCHECK only shows which kind of nodes can be a context, even if
      // this DCHECK fails, it is always safe to set aliasing context mode to
      // kYes.
      DCHECK(false);
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kYes;
      break;
  }
}

void KnownNodeAspects::ClearUnstableNodeAspectsForStoreMap(
    StoreMap* node, bool is_tracing_enabled) {
  switch (node->kind()) {
    case StoreMap::Kind::kInitializing:
    case StoreMap::Kind::kInlinedAllocation:
      return;
    case StoreMap::Kind::kTransitioning: {
      if (NodeInfo* node_info = TryGetInfoFor(node->ValueInput().node())) {
        if (node_info->possible_maps_are_known() &&
            node_info->possible_maps().size() == 1) {
          compiler::MapRef old_map = node_info->possible_maps().at(0);
          auto MaybeAliases = [&](compiler::MapRef map) -> bool {
            return map.equals(old_map);
          };
          ClearUnstableMapsIfAny(MaybeAliases);
          if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building &&
                          is_tracing_enabled)) {
            std::cout << "  ! StoreMap: Clearing unstable map "
                      << Brief(*old_map.object()) << std::endl;
          }
          return;
        }
      }
      break;
    }
  }
  // TODO(olivf): Only invalidate nodes with the same type.
  ClearUnstableMaps();
  if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building && is_tracing_enabled)) {
    std::cout << "  ! StoreMap: Clearing unstable maps" << std::endl;
  }
}

void KnownNodeAspects::ClearUnstableNodeAspects(bool is_tracing_enabled) {
  if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building && is_tracing_enabled)) {
    std::cout << "  ! Clearing unstable node aspects" << std::endl;
  }
  ClearUnstableMaps();
  // Side-effects can change object contents, so we have to clear
  // our known loaded properties -- however, constant properties are known
  // to not change (and we added a dependency on this), so we don't have to
  // clear those.
  loaded_properties_.clear();
  loaded_context_slots_.clear();
  may_have_aliasing_contexts_ = KnownNodeAspects::ContextSlotLoadsAlias::kNone;
}

KnownNodeAspects* KnownNodeAspects::CloneForLoopHeader(
    bool optimistic, LoopEffects* loop_effects, Zone* zone) const {
  return zone->New<KnownNodeAspects>(*this, optimistic, loop_effects, zone);
}

KnownNodeAspects::KnownNodeAspects(const KnownNodeAspects& other,
                                   bool optimistic_initial_state,
                                   LoopEffects* loop_effects, Zone* zone)
    : loaded_constant_properties_(other.loaded_constant_properties_),
      loaded_properties_(zone),
      loaded_context_constants_(other.loaded_context_constants_),
      loaded_context_slots_(zone),
      available_expressions_(zone),
      any_map_for_any_node_is_unstable_(false),
      may_have_aliasing_contexts_(
          KnownNodeAspects::ContextSlotLoadsAlias::kNone),
      effect_epoch_(other.effect_epoch_),
      node_infos_(zone),
      virtual_objects_(other.virtual_objects_) {
  if (!other.any_map_for_any_node_is_unstable_) {
    node_infos_ = other.node_infos_;
#ifdef DEBUG
    for (const auto& it : node_infos_) {
      DCHECK(!it.second.any_map_is_unstable());
    }
#endif
  } else if (optimistic_initial_state &&
             !loop_effects->unstable_aspects_cleared) {
    node_infos_ = other.node_infos_;
    any_map_for_any_node_is_unstable_ = other.any_map_for_any_node_is_unstable_;
  } else {
    for (const auto& it : other.node_infos_) {
      node_infos_.emplace(it.first,
                          NodeInfo::ClearUnstableMapsOnCopy{it.second});
    }
  }
  if (optimistic_initial_state && !loop_effects->unstable_aspects_cleared) {
    // IMPORTANT: Whatever we clone here needs to be checked for consistency
    // in when we try to terminate the loop in `IsCompatibleWithLoopHeader`.
    if (loop_effects->objects_written.empty() &&
        loop_effects->keys_cleared.empty()) {
      loaded_properties_ = other.loaded_properties_;
    } else {
      auto cleared_key = loop_effects->keys_cleared.begin();
      auto cleared_keys_end = loop_effects->keys_cleared.end();
      auto cleared_obj = loop_effects->objects_written.begin();
      auto cleared_objs_end = loop_effects->objects_written.end();
      for (auto loaded_key : other.loaded_properties_) {
        if (NextInIgnoreList(cleared_key, cleared_keys_end, loaded_key.first)) {
          continue;
        }
        auto& props_for_key =
            loaded_properties_.try_emplace(loaded_key.first, zone)
                .first->second;
        for (auto loaded_obj : loaded_key.second) {
          if (!NextInIgnoreList(cleared_obj, cleared_objs_end,
                                loaded_obj.first)) {
            props_for_key.emplace(loaded_obj);
          }
        }
      }
    }
    if (loop_effects->context_slot_written.empty()) {
      loaded_context_slots_ = other.loaded_context_slots_;
    } else {
      auto slot_written = loop_effects->context_slot_written.begin();
      auto slot_written_end = loop_effects->context_slot_written.end();
      for (auto loaded : other.loaded_context_slots_) {
        if (!NextInIgnoreList(slot_written, slot_written_end, loaded.first)) {
          loaded_context_slots_.emplace(loaded);
        }
      }
    }
    if (!loaded_context_slots_.empty()) {
      if (loop_effects->may_have_aliasing_contexts) {
        may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kYes;
      } else {
        DCHECK_EQ(may_have_aliasing_contexts_, ContextSlotLoadsAlias::kNone);
        may_have_aliasing_contexts_ = other.may_have_aliasing_contexts();
      }
    }
  }

  // To account for the back-jump we must not allow effects to be reshuffled
  // across loop headers.
  // TODO(olivf): Only do this if the loop contains write effects.
  increment_effect_epoch();
  for (const auto& e : other.available_expressions_) {
    if (e.second.effect_epoch >= effect_epoch_) {
      available_expressions_.emplace(e);
    }
  }

  virtual_objects_.Snapshot();
}

bool KnownNodeAspects::IsCompatibleWithLoopHeader(
    const KnownNodeAspects& loop_header) const {
  // Needs to be in sync with `CloneForLoopHeader(zone, true)`.

  // Analysis state can change with loads.
  if (!loop_header.loaded_context_slots_.empty() &&
      loop_header.may_have_aliasing_contexts() != ContextSlotLoadsAlias::kYes &&
      loop_header.may_have_aliasing_contexts() !=
          may_have_aliasing_contexts() &&
      may_have_aliasing_contexts() != ContextSlotLoadsAlias::kNone) {
    if (V8_UNLIKELY(v8_flags.trace_maglev_loop_speeling)) {
      std::cout << "KNA after loop has incompatible "
                   "loop_header.may_have_aliasing_contexts\n";
    }
    return false;
  }

  bool had_effects = effect_epoch_ != loop_header.effect_epoch_;

  if (!had_effects) {
    if (!AspectIncludes(loop_header.node_infos_, node_infos_, NodeInfoTypeIs,
                        NodeInfoIsEmpty)) {
      if (V8_UNLIKELY(v8_flags.trace_maglev_loop_speeling)) {
        std::cout << "KNA after effectless loop has incompatible node_infos\n";
      }
      return false;
    }
    // In debug builds we do a full comparison to ensure that without an effect
    // epoch change all unstable properties still hold.
#ifndef DEBUG
    return true;
#endif
  }

  if (!AspectIncludes(loop_header.node_infos_, node_infos_, NodeInfoIncludes,
                      NodeInfoIsEmpty)) {
    if (V8_UNLIKELY(v8_flags.trace_maglev_loop_speeling)) {
      std::cout << "KNA after loop has incompatible node_infos\n";
    }
    DCHECK(had_effects);
    return false;
  }

  if (!MaybeEmptyAspectIncludes(
          loop_header.loaded_properties_, loaded_properties_,
          [](auto a, auto b) { return AspectIncludes(a, b, SameValue); })) {
    if (V8_UNLIKELY(v8_flags.trace_maglev_loop_speeling)) {
      std::cout << "KNA after loop has incompatible loaded_properties\n";
    }
    DCHECK(had_effects);
    return false;
  }

  if (!MaybeNullAspectIncludes(loop_header.loaded_context_slots_,
                               loaded_context_slots_, SameValue)) {
    if (V8_UNLIKELY(v8_flags.trace_maglev_loop_speeling)) {
      std::cout << "KNA after loop has incompatible loaded_context_slots\n";
    }
    DCHECK(had_effects);
    return false;
  }

  return true;
}

SmallZoneVector<KnownNodeAspects::LoadedContextSlotsKey, 8>
KnownNodeAspects::ClearAliasedContextSlotsFor(Graph* graph, ValueNode* context,
                                              int offset, ValueNode* value) {
  SmallZoneVector<LoadedContextSlotsKey, 8> aliased_slots_(graph->zone());
  if (!loaded_context_slots_.empty()) {
    UpdateMayHaveAliasingContexts(graph->broker(),
                                  graph->broker()->local_isolate(), context);
  }
  if (may_have_aliasing_contexts() == ContextSlotLoadsAlias::kYes) {
    compiler::OptionalScopeInfoRef scope_info = graph->TryGetScopeInfo(context);
    for (auto& cache : loaded_context_slots_) {
      int cached_offset = std::get<int>(cache.first);
      ValueNode* cached_context = std::get<ValueNode*>(cache.first);
      if (cached_offset == offset && cached_context != context) {
        if (graph->ContextMayAlias(cached_context, scope_info) &&
            cache.second != value) {
          if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building &&
                          graph->is_tracing_enabled())) {
            std::cout << "  * Clearing probably aliasing value "
                      << PrintNodeLabel(std::get<ValueNode*>(cache.first))
                      << "[" << offset << "]: " << PrintNode(value)
                      << std::endl;
          }
          cache.second = nullptr;
          aliased_slots_.push_back(cache.first);
        }
      }
    }
  }
  return aliased_slots_;
}

void KnownNodeAspects::PrintLoadedProperties() {
  std::cout << "Constant properties:\n";
  for (auto [key, map] : loaded_constant_properties_) {
    std::cout << "  - " << key << ": { ";
    bool is_first = true;
    for (auto [object, value] : map) {
      if (!is_first) std::cout << ", ";
      is_first = false;
      std::cout << PrintNodeLabel(object) << "=>" << PrintNodeLabel(value);
    }
    std::cout << " }\n";
  }

  std::cout << "Non-constant properties:\n";
  for (auto [key, map] : loaded_properties_) {
    std::cout << "  - " << key << ": { ";
    bool is_first = true;
    for (auto [object, value] : map) {
      if (!is_first) std::cout << ", ";
      is_first = false;
      std::cout << PrintNodeLabel(object) << "=>" << PrintNodeLabel(value);
    }
    std::cout << " }\n";
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
