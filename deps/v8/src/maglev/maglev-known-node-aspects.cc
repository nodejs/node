// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-known-node-aspects.h"

#include <algorithm>
#include <iostream>
#include <optional>

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/handles/handles-inl.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-tracer.h"
#include "src/objects/objects-inl.h"
#include "src/roots/roots-inl.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace {

const char* kRed = "\033[31m";
const char* kGreen = "\033[32m";
const char* kYellow = "\033[33m";
const char* kCyan = "\033[36m";
const char* kReset = "\033[0m";
const char* kBold = "\033[1m";

void PrintPossibleMaps(std::ostream& os, const PossibleMaps& maps) {
  os << "{ ";
  bool first = true;
  for (compiler::MapRef map : maps) {
    if (!first) os << ", ";
    first = false;
    os << Brief(*map.object());
  }
  os << " }";
}

void TraceMerge(ValueNode* node, NodeInfo& lhs, const NodeInfo& rhs) {
  // Only print if there is something interesting to merge.
  if (!lhs.possible_maps_are_known() && !rhs.possible_maps_are_known()) {
    return;
  }
  std::cout << kYellow << "[KNA] Merge: " << PrintNodeLabel(node) << " "
            << kReset;
  if (lhs.possible_maps_are_known()) {
    PrintPossibleMaps(std::cout, lhs.possible_maps());
  } else {
    std::cout << "<unknown>";
  }
  std::cout << " (stale=" << lhs.maps_are_stale() << ") U ";
  if (rhs.possible_maps_are_known()) {
    PrintPossibleMaps(std::cout, rhs.possible_maps());
  } else {
    std::cout << "<unknown>";
  }
  std::cout << " (stale=" << rhs.maps_are_stale() << ")" << std::endl;
}

struct DefaultMergeFunc {
  template <typename Key, typename Value>
  bool operator()(const Key&, const Value& lhs, const Value& rhs) {
    return lhs == rhs;
  }
};

struct NeverKeepLhsOnly {
  template <typename Key, typename Value>
  bool operator()(const Key&, Value&) {
    return false;
  }
};

// Destructively merges the right map into the left map, such that the left
// map is mutated to become the result. Values that are in both maps are
// passed to `merge` to be merged with each other -- the LHS here is expected
// to be mutated. Entries only in the left map are passed to `keep_lhs_only`,
// which may also mutate the value and decides whether the entry survives;
// entries only in the right map are dropped. Both functors see keys in
// ascending order.
template <typename Key, typename Value, typename MergeFunc,
          typename KeepLhsOnlyFunc>
void DestructivelyMerge(ZoneMap<Key, Value>& lhs_map,
                        const ZoneMap<Key, Value>& rhs_map, MergeFunc&& merge,
                        KeepLhsOnlyFunc&& keep_lhs_only) {
  // Walk the two maps in lock step. This relies on the fact that ZoneMaps are
  // sorted.
  typename ZoneMap<Key, Value>::iterator lhs_it = lhs_map.begin();
  typename ZoneMap<Key, Value>::const_iterator rhs_it = rhs_map.begin();
  while (lhs_it != lhs_map.end() && rhs_it != rhs_map.end()) {
    if (lhs_it->first < rhs_it->first) {
      if (keep_lhs_only(lhs_it->first, lhs_it->second)) {
        ++lhs_it;
      } else {
        lhs_it = lhs_map.erase(lhs_it);
      }
    } else if (rhs_it->first < lhs_it->first) {
      // Skip over elements that are only in RHS.
      ++rhs_it;
    } else {
      // Apply the merge function to the values of the two iterators. If the
      // function returns false, remove the value.
      bool keep_value = merge(lhs_it->first, lhs_it->second, rhs_it->second);
      if constexpr (std::is_same_v<decltype(lhs_it->second), ValueNode*>) {
        if (!keep_value && lhs_it->second && rhs_it->second) {
          auto l = lhs_it->second->UnwrapIdentities();
          auto r = rhs_it->second->UnwrapIdentities();
          if (l != lhs_it->second || r != rhs_it->second) {
            lhs_it->second = l;
            keep_value = merge(lhs_it->first, l, r);
          }
        }
      }
      if (keep_value) {
        ++lhs_it;
      } else {
        lhs_it = lhs_map.erase(lhs_it);
      }
      ++rhs_it;
    }
  }
  // The remaining LHS items have no RHS counterpart.
  if constexpr (std::is_same_v<std::decay_t<KeepLhsOnlyFunc>,
                               NeverKeepLhsOnly>) {
    if (lhs_it != lhs_map.end()) {
      lhs_map.erase(lhs_it, lhs_map.end());
    }
  } else {
    while (lhs_it != lhs_map.end()) {
      if (keep_lhs_only(lhs_it->first, lhs_it->second)) {
        ++lhs_it;
      } else {
        lhs_it = lhs_map.erase(lhs_it);
      }
    }
  }
}

// Destructively intersects the right map into the left map, such that the
// left map is mutated to become the result of the intersection. Values that
// are in both maps are passed to the merging function to be merged with each
// other -- again, the LHS here is expected to be mutated.
template <typename Key, typename Value, typename MergeFunc = DefaultMergeFunc>
void DestructivelyIntersect(ZoneMap<Key, Value>& lhs_map,
                            const ZoneMap<Key, Value>& rhs_map,
                            MergeFunc&& func = DefaultMergeFunc()) {
  DestructivelyMerge(lhs_map, rhs_map, std::forward<MergeFunc>(func),
                     NeverKeepLhsOnly());
}

// LINT.IfChange(LoopInvariantFacts)

// Membership queries against a sorted ZoneSet, advanced in lock step with a
// caller iterating its own sorted container: between Resets, keys must be
// queried in ascending order.
template <typename Key>
class IgnoreListCursor {
 public:
  explicit IgnoreListCursor(const ZoneSet<Key>& set)
      : set_(set), it_(set.begin()) {}

  bool Contains(const Key& key) {
#ifdef DEBUG
    DCHECK_IMPLIES(last_.has_value(), !(key < *last_));
    last_ = key;
#endif
    while (it_ != set_.end() && *it_ < key) {
      ++it_;
    }
    return it_ != set_.end() && *it_ == key;
  }

  void Reset() {
    it_ = set_.begin();
#ifdef DEBUG
    last_.reset();
#endif
  }

 private:
  const ZoneSet<Key>& set_;
  typename ZoneSet<Key>::const_iterator it_;
#ifdef DEBUG
  std::optional<Key> last_;
#endif
};

// Decides whether a cached property fact provably survives a loop whose
// writes are summarized by `loop_effects`. StartKey must be called with
// ascending keys, and ObjectInvariant with ascending objects per key.
class LoopInvariantPropertyFilter {
 public:
  LoopInvariantPropertyFilter(const LoopEffects* loop_effects,
                              const ZoneSet<ValueNode*>& objects_written,
                              const KnownNodeAspects* aspects)
      : loop_effects_(loop_effects),
        keys_cleared_(loop_effects->keys_cleared),
        objects_written_(objects_written),
        aspects_(aspects) {}

  bool StartKey(PropertyKey key) {
    key_cleared_ = keys_cleared_.Contains(key);
    check_elements_transition_ = loop_effects_->elements_kind_transitioned &&
                                 key == PropertyKey::Elements();
    objects_written_.Reset();
    return !key_cleared_;
  }

  bool ObjectInvariant(ValueNode* obj) {
    if (key_cleared_) return false;
    if (objects_written_.Contains(obj)) return false;
    if (check_elements_transition_ && !aspects_->TryGetInfoWithFreshMaps(obj)) {
      return false;
    }
    return true;
  }

 private:
  const LoopEffects* loop_effects_;
  IgnoreListCursor<PropertyKey> keys_cleared_;
  IgnoreListCursor<ValueNode*> objects_written_;
  const KnownNodeAspects* aspects_;
  bool key_cleared_ = false;
  bool check_elements_transition_ = false;
};

// LINT.ThenChange(:IsCompatibleWithLoopHeader)

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

bool LoopHeaderCompatibleWithBackEdge(const NodeInfo& loop_header,
                                      const NodeInfo& backedge) {
  // TODO(428667907): Ideally we should bail out early for the kNone type.
  if (!NodeTypeIs(backedge.type(), loop_header.type(),
                  NodeTypeIsVariant::kAllowNone)) {
    return false;
  }
  if (loop_header.possible_maps_are_known() &&
      loop_header.any_map_is_unstable()) {
    if (!backedge.possible_maps_are_known()) {
      return false;
    }
    if (!loop_header.maps_are_stale() && backedge.maps_are_stale()) {
      return false;
    }
    if (!loop_header.possible_maps().contains(backedge.possible_maps())) {
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

bool SameValue(ValueNode* before, ValueNode* after) {
  // Nodes from loop headers can contain identities since the KnownNodeAspects
  // constructor used in CloneForLoopHeader does not unwrap them.
  return before == after || (before && before->UnwrapIdentities() == after);
}

}  // namespace

// static
void NodeInfo::TraceSetPossibleMaps(const KnownNodeAspects& known_node_aspects,
                                    const NodeInfo* node_info,
                                    const PossibleMaps& possible_maps) {
  CHECK(v8_flags.trace_maglev_kna);
  ValueNode* node = known_node_aspects.FindNode(node_info);
  CHECK_NOT_NULL(node);
  std::cout << kGreen << "[KNA] Set maps: " << PrintNodeLabel(node) << " "
            << kReset;
  PrintPossibleMaps(std::cout, possible_maps);
  std::cout << std::endl;
}

void KnownNodeAspects::Merge(const KnownNodeAspects& other, Zone* zone) {
  bool side_effects_require_invalidation = false;
  DestructivelyIntersect(
      node_infos_, other.node_infos_,
      [&](ValueNode* node, NodeInfo& lhs, const NodeInfo& rhs) {
        if (V8_UNLIKELY(v8_flags.trace_maglev_kna)) {
          TraceMerge(node, lhs, rhs);
        }
        lhs.MergeWith(rhs, zone, side_effects_require_invalidation);
        return !lhs.no_info_available();
      });

  if (effect_epoch_ != other.effect_epoch_) {
    effect_epoch_ = std::max(effect_epoch_, other.effect_epoch_) + 1;
  }
  DestructivelyIntersect(
      available_expressions_, other.available_expressions_,
      [&](uint32_t hash, const AvailableExpression& lhs,
          const AvailableExpression& rhs) {
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

  side_effects_require_invalidation_ = side_effects_require_invalidation;

  auto merge_loaded_properties =
      [](PropertyKey key, ZoneMap<ValueNode*, ValueNode*>& lhs,
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
  DestructivelyIntersect(loaded_tagged_keyed_properties_,
                         other.loaded_tagged_keyed_properties_);
  DestructivelyIntersect(loaded_context_constants_,
                         other.loaded_context_constants_);
  may_have_aliasing_contexts_ = ContextSlotLoadsAliasMerge(
      may_have_aliasing_contexts_, other.may_have_aliasing_contexts());
  DestructivelyIntersect(loaded_context_slots_, other.loaded_context_slots_);
}

// Loop-header variant of Merge. The forward (LHS) predecessor dominates the
// loop body, so its facts hold throughout the loop: keep them instead of
// intersecting with the backedge by pointer equality (which drops the
// loop-invariant values the peeler cloned), invalidating only what the loop
// body can change.
void KnownNodeAspects::MergeForLoop(const KnownNodeAspects& backedge,
                                    Zone* zone,
                                    const LoopEffects* loop_effects) {
  if (side_effects_require_invalidation_ &&
      (loop_effects == nullptr || loop_effects->unstable_aspects_cleared ||
       loop_effects->elements_kind_transitioned)) {
    ZoneMap<ValueNode*, NodeInfo> cleared(zone);
    for (auto& entry : node_infos_) {
      cleared.emplace(entry.first,
                      NodeInfo::ClearUnstableMapsOnCopy{entry.second});
    }
    node_infos_ = std::move(cleared);
    side_effects_require_invalidation_ = false;
  }

  if (effect_epoch_ != backedge.effect_epoch_) {
    effect_epoch_ = std::max(effect_epoch_, backedge.effect_epoch_) + 1;
  }
  // Forward entries for pure (epoch-exempt) expressions are kept even without
  // a backedge equivalent: the forward predecessor dominates the loop body and
  // loop effects cannot invalidate a pure expression. Other entries need a
  // structurally equal backedge entry with a still-valid epoch.
  for (auto it = available_expressions_.begin();
       it != available_expressions_.end();) {
    const AvailableExpression& lhs = it->second;
    DCHECK_NE(lhs.effect_epoch, kEffectEpochOverflow);
    DCHECK_IMPLIES(!lhs.node->Is<Identity>(),
                   Node::needs_epoch_check(lhs.node->opcode()) ==
                       (lhs.effect_epoch != kEffectEpochForPureInstructions));
    if (lhs.node->Is<Identity>()) {
      it = available_expressions_.erase(it);
      continue;
    }
    if (lhs.effect_epoch == kEffectEpochForPureInstructions) {
      ++it;
      continue;
    }
    bool keep = false;
    auto rhs_it = backedge.available_expressions_.find(it->first);
    if (rhs_it != backedge.available_expressions_.end()) {
      const AvailableExpression& rhs = rhs_it->second;
      DCHECK_IMPLIES(lhs.node == rhs.node,
                     lhs.effect_epoch == rhs.effect_epoch);
      ValueNode* rhs_value = rhs.node->TryCast<ValueNode>();
      NodeBase* rhs_node = rhs_value ? rhs_value->UnwrapIdentities() : rhs.node;
      keep = lhs.node->IsStructurallyEqualTo(rhs_node) &&
             lhs.effect_epoch >= effect_epoch_;
    }
    if (keep) {
      ++it;
    } else {
      it = available_expressions_.erase(it);
    }
  }

  const bool keep_invariant_loads =
      loop_effects != nullptr && !loop_effects->unstable_aspects_cleared;
  auto same_load = [](ValueNode* lhs, ValueNode* rhs) {
    return lhs != nullptr && rhs != nullptr &&
           lhs->UnwrapIdentities()->IsStructurallyEqualTo(
               rhs->UnwrapIdentities());
  };
  auto merge_constant_properties =
      [&](PropertyKey key, ZoneMap<ValueNode*, ValueNode*>& lhs,
          const ZoneMap<ValueNode*, ValueNode*>& rhs) {
        DestructivelyIntersect(lhs, rhs,
                               [&](ValueNode* obj, ValueNode* l, ValueNode* r) {
                                 return l == r || same_load(l, r);
                               });
        return !lhs.empty();
      };
  ZoneSet<ValueNode*> objects_written(zone);
  if (keep_invariant_loads) {
    for (ValueNode* obj : loop_effects->objects_written) {
      objects_written.insert(obj->UnwrapIdentities());
    }
  }
  std::optional<LoopInvariantPropertyFilter> filter;
  if (keep_invariant_loads) {
    filter.emplace(loop_effects, objects_written, this);
  }
  auto entry_invariant = [&](ValueNode* obj) {
    return filter.has_value() && filter->ObjectInvariant(obj);
  };
  auto keep_forward_fact = [&](ValueNode* obj, ValueNode*& l) {
    if (l) l = l->UnwrapIdentities();
    return l != nullptr && entry_invariant(obj);
  };
  auto merge_object_fact = [&](ValueNode* obj, ValueNode*& l, ValueNode* r) {
    if (l) l = l->UnwrapIdentities();
    if (r) r = r->UnwrapIdentities();
    if (!r) return l != nullptr && entry_invariant(obj);
    return l == r || (entry_invariant(obj) && same_load(l, r));
  };
  auto merge_properties_of_key =
      [&](PropertyKey key, ZoneMap<ValueNode*, ValueNode*>& lhs,
          const ZoneMap<ValueNode*, ValueNode*>& rhs) {
        if (filter) filter->StartKey(key);
        DestructivelyMerge(lhs, rhs, merge_object_fact, keep_forward_fact);
        return !lhs.empty();
      };
  auto keep_forward_properties_of_key =
      [&](PropertyKey key, ZoneMap<ValueNode*, ValueNode*>& lhs) {
        if (!filter || !filter->StartKey(key)) return false;
        for (auto it = lhs.begin(); it != lhs.end();) {
          if (keep_forward_fact(it->first, it->second)) {
            ++it;
          } else {
            it = lhs.erase(it);
          }
        }
        return !lhs.empty();
      };
  DestructivelyIntersect(loaded_constant_properties_,
                         backedge.loaded_constant_properties_,
                         merge_constant_properties);
  // The forward predecessor dominates the loop body and the loop effects
  // summarize every write in the body, so a forward fact provably untouched
  // by the loop is kept even without a matching backedge entry.
  DestructivelyMerge(loaded_properties_, backedge.loaded_properties_,
                     merge_properties_of_key, keep_forward_properties_of_key);
  DestructivelyIntersect(loaded_tagged_keyed_properties_,
                         backedge.loaded_tagged_keyed_properties_);
  DestructivelyIntersect(loaded_context_constants_,
                         backedge.loaded_context_constants_,
                         [&](auto key, ValueNode* l, ValueNode* r) {
                           return l == r || same_load(l, r);
                         });
  may_have_aliasing_contexts_ = ContextSlotLoadsAliasMerge(
      may_have_aliasing_contexts_, backedge.may_have_aliasing_contexts());
  const bool context_stores_may_alias =
      may_have_aliasing_contexts_ != ContextSlotLoadsAlias::kNever;
  std::optional<IgnoreListCursor<LoadedContextSlotsKey>> slot_written;
  if (keep_invariant_loads && !loop_effects->may_have_aliasing_contexts) {
    slot_written.emplace(loop_effects->context_slot_written);
  }
  auto context_slot_invariant = [&](const LoadedContextSlotsKey& key) {
    if (!slot_written.has_value()) return false;
    if (slot_written->Contains(key)) return false;
    if (context_stores_may_alias &&
        loop_effects->WritesContextSlotOffset(std::get<int>(key))) {
      return false;
    }
    return true;
  };
  // TODO(victorgomes): Like loaded_properties_ above, an invariant forward
  // context slot could be kept without a backedge witness.
  DestructivelyIntersect(
      loaded_context_slots_, backedge.loaded_context_slots_,
      [&](auto key, ValueNode* l, ValueNode* r) {
        return l == r || (context_slot_invariant(key) && same_load(l, r));
      });
}

void KnownNodeAspects::UnwrapIdentitiesAndPhisInKeys(Zone* zone) {
  auto unwrap = [](ValueNode* node) -> ValueNode* {
    return node == nullptr ? nullptr : node->UnwrapIdentitiesAndPhis();
  };

  // node_infos_ is keyed by value node.
  NodeInfos remapped_infos(zone);
  for (auto& entry : node_infos_) {
    ValueNode* key = unwrap(entry.first);
    auto it = remapped_infos.find(key);
    if (it == remapped_infos.end()) {
      remapped_infos.emplace(key, entry.second);
    } else {
      // A phi and its single input normalize to the same node; both entries
      // then describe the same value, so combine their facts.
      it->second.CombineSameValueFrom(entry.second);
    }
  }
  node_infos_ = std::move(remapped_infos);

  // Cached property loads: key -> object -> value. Only the object is a node
  // key; values are left as-is (the merge already unwraps identities on value
  // comparison, and available_expressions_ is not remapped at all).
  auto remap_loaded_properties = [&](LoadedPropertyMap& map) {
    LoadedPropertyMap remapped(zone);
    for (auto& [property_key, objects] : map) {
      ZoneMap<ValueNode*, ValueNode*> remapped_objects(zone);
      for (auto& [object, value] : objects) {
        remapped_objects[unwrap(object)] = value;
      }
      remapped.emplace(property_key, std::move(remapped_objects));
    }
    map = std::move(remapped);
  };
  remap_loaded_properties(loaded_properties_);
  remap_loaded_properties(loaded_constant_properties_);

  {
    ZoneMap<std::pair<ValueNode*, ValueNode*>, ValueNode*> remapped(zone);
    for (auto& [key, value] : loaded_tagged_keyed_properties_) {
      remapped[{unwrap(key.first), unwrap(key.second)}] = value;
    }
    loaded_tagged_keyed_properties_ = std::move(remapped);
  }

  // Context slot loads: (context node, offset) -> value. Only the context node
  // in the key is remapped.
  auto remap_context_slots =
      [&](ZoneMap<LoadedContextSlotsKey, ValueNode*>& m) {
        ZoneMap<LoadedContextSlotsKey, ValueNode*> remapped(zone);
        for (auto& [key, value] : m) {
          remapped[LoadedContextSlotsKey{unwrap(std::get<0>(key)),
                                         std::get<1>(key)}] = value;
        }
        m = std::move(remapped);
      };
  remap_context_slots(loaded_context_constants_);
  remap_context_slots(loaded_context_slots_);
}

void KnownNodeAspects::UpdateMayHaveAliasingContexts(
    compiler::JSHeapBroker* broker, LocalIsolate* local_isolate,
    ValueNode* context) {
  if (may_have_aliasing_contexts_ == ContextSlotLoadsAlias::kAlways) return;

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
    case Opcode::kHeapConstant:
      may_have_aliasing_contexts_ = ContextSlotLoadsAliasMerge(
          may_have_aliasing_contexts_,
          ContextSlotLoadsAlias::kOnlyLoadsRelativeToConstant);
      DCHECK(NodeTypeIs(GetTypeUnchecked(broker, context), NodeType::kContext));
      break;
    case Opcode::kCreateFunctionContext:
    case Opcode::kInlinedAllocation:
      // These can be precisely tracked.
      DCHECK(NodeTypeIs(GetTypeUnchecked(broker, context), NodeType::kContext));
      break;
    case Opcode::kLoadTaggedField: {
      LoadTaggedField* load = context->Cast<LoadTaggedField>();
      // Currently, the only way to leak a context via load tagged field is with
      // JSFunction or JSGeneratorObject.
      USE(load);
      DCHECK(load->offset() == offsetof(JSFunction, context_) ||
             load->offset() == offsetof(JSGeneratorObject, context_));
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kAlways;
      DCHECK(NodeTypeIs(GetTypeUnchecked(broker, context), NodeType::kContext));
      break;
    }
    case Opcode::kCallRuntime:
      DCHECK(NodeTypeIs(GetTypeUnchecked(broker, context), NodeType::kContext));
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kAlways;
      break;
    case Opcode::kGeneratorRestoreRegister:
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kAlways;
      break;
    case Opcode::kPhi:
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kAlways;
      break;
    default:
      // This DCHECK only shows which kind of nodes can be a context, even if
      // this DCHECK fails, it is always safe to set aliasing context mode to
      // kAlways.
      DCHECK(false);
      may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kAlways;
      break;
  }
}

void KnownNodeAspects::ClearUnstableNodeAspectsForStoreMap(
    StoreMap* node, bool is_tracing_enabled) {
  if (!node->is_transitioning()) return;

  if (NodeInfo* node_info = TryGetInfoFor(node->ValueInput().node())) {
    if (node_info->possible_maps_are_known() && !node_info->maps_are_stale() &&
        node_info->possible_maps().size() == 1) {
      compiler::MapRef old_map = node_info->possible_maps().at(0);
      auto MaybeAliases = [&](compiler::MapRef map) -> bool {
        return map.equals(old_map);
      };
      // Mark aliasing maps as stale.
      if (MarkMapsStaleIfAny(MaybeAliases)) {
        if (V8_UNLIKELY(v8_flags.trace_maglev_kna && is_tracing_enabled)) {
          std::cout << kRed << "[KNA] StoreMap: Invalidate alias "
                    << Brief(*old_map.object()) << kReset << std::endl;
        }
      }
      return;
    }
  }

  // TODO(olivf): Only invalidate nodes with the same type.
  OnSideEffect();
  if (V8_UNLIKELY(v8_flags.trace_maglev_kna && is_tracing_enabled)) {
    std::cout << kRed << "[KNA] StoreMap: Invalidate all unstable maps"
              << kReset << std::endl;
  }
}

void KnownNodeAspects::ClearUnstableNodeAspectsForElementsTransition(
    const ZoneVector<compiler::MapRef>& transition_sources,
    bool is_tracing_enabled) {
  // The transition only fires on objects whose current map is one of the
  // sources, so only facts about nodes that may hold such an object can go
  // stale; everything else survives.
  auto MaybeAliases = [&](compiler::MapRef map) -> bool {
    for (compiler::MapRef source : transition_sources) {
      if (source.equals(map)) return true;
    }
    return false;
  };
  if (MarkMapsStaleIfAny(MaybeAliases)) {
    if (V8_UNLIKELY(v8_flags.trace_maglev_kna && is_tracing_enabled)) {
      std::cout << kRed
                << "[KNA] ElementsTransition: Invalidate source-map aliases"
                << kReset << std::endl;
    }
  }
  // A transitioning object's elements store may be reallocated; drop cached
  // elements for any node that may alias a source-mapped object (including
  // nodes whose map is unknown or already stale).
  auto it = loaded_properties_.find(PropertyKey::Elements());
  if (it == loaded_properties_.end()) return;
  bool dropped_any =
      std::erase_if(it->second, [&](const auto& entry) {
        const NodeInfo* info = TryGetInfoWithFreshMaps(entry.first);
        if (!info) return true;
        const auto& maps = info->possible_maps();
        return std::any_of(maps.begin(), maps.end(), MaybeAliases);
      }) > 0;
  if (dropped_any &&
      V8_UNLIKELY(v8_flags.trace_maglev_kna && is_tracing_enabled)) {
    std::cout << kRed << "[KNA] ElementsTransition: Drop aliased [Elements]"
              << kReset << std::endl;
  }
}

const NodeInfo* KnownNodeAspects::TryGetInfoWithFreshMaps(
    ValueNode* object) const {
  const NodeInfo* info = TryGetInfoFor(object);
  if (!info || !info->possible_maps_are_known() || info->maps_are_stale()) {
    return nullptr;
  }
  return info;
}

void KnownNodeAspects::ClearUnstableNodeAspects(bool is_tracing_enabled) {
  if (V8_UNLIKELY(v8_flags.trace_maglev_kna)) {
    std::cout << kRed << "[KNA] Invalidate: all unstable aspects" << kReset
              << std::endl;
  }
  OnSideEffect();
  // Side-effects can change object contents, so we have to clear
  // our known loaded properties -- however, constant properties are known
  // to not change (and we added a dependency on this), so we don't have to
  // clear those.
  loaded_properties_.clear();
  loaded_tagged_keyed_properties_.clear();
  loaded_context_slots_.clear();
  may_have_aliasing_contexts_ = KnownNodeAspects::ContextSlotLoadsAlias::kNever;
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
      loaded_tagged_keyed_properties_(zone),
      loaded_context_constants_(other.loaded_context_constants_),
      loaded_context_slots_(zone),
      available_expressions_(zone),
      side_effects_require_invalidation_(false),
      may_have_aliasing_contexts_(
          KnownNodeAspects::ContextSlotLoadsAlias::kNever),
      effect_epoch_(other.effect_epoch_),
      node_infos_(zone),
      virtual_objects_(other.virtual_objects_) {
  if (!other.side_effects_require_invalidation_) {
    node_infos_ = other.node_infos_;
#ifdef DEBUG
    for (const auto& it : node_infos_) {
      DCHECK(!it.second.any_map_is_unstable() || it.second.maps_are_stale());
    }
#endif
  } else if (optimistic_initial_state &&
             !loop_effects->unstable_aspects_cleared &&
             !loop_effects->elements_kind_transitioned) {
    node_infos_ = other.node_infos_;
    side_effects_require_invalidation_ =
        other.side_effects_require_invalidation_;
  } else {
    for (const auto& it : other.node_infos_) {
      node_infos_.emplace(it.first,
                          NodeInfo::ClearUnstableMapsOnCopy{it.second});
    }
  }
  // LINT.IfChange(CloneForLoopHeader)
  if (optimistic_initial_state && !loop_effects->unstable_aspects_cleared) {
    // IMPORTANT: Whatever we clone here needs to be checked for consistency
    // in when we try to terminate the loop in `IsCompatibleWithLoopHeader`.
    if (loop_effects->objects_written.empty() &&
        loop_effects->keys_cleared.empty() &&
        !loop_effects->elements_kind_transitioned) {
      loaded_properties_ = other.loaded_properties_;
    } else {
      LoopInvariantPropertyFilter filter(loop_effects,
                                         loop_effects->objects_written, this);
      for (auto loaded_key : other.loaded_properties_) {
        if (!filter.StartKey(loaded_key.first)) continue;
        auto& props_for_key =
            loaded_properties_.try_emplace(loaded_key.first, zone)
                .first->second;
        for (auto loaded_obj : loaded_key.second) {
          if (!filter.ObjectInvariant(loaded_obj.first)) continue;
          props_for_key.emplace(loaded_obj);
        }
      }
    }
    if (loop_effects->context_slot_written.empty()) {
      loaded_context_slots_ = other.loaded_context_slots_;
    } else {
      IgnoreListCursor<LoadedContextSlotsKey> slot_written(
          loop_effects->context_slot_written);
      for (auto loaded : other.loaded_context_slots_) {
        if (!loaded.second) continue;
        loaded.second = loaded.second->UnwrapIdentities();
        if (!slot_written.Contains(loaded.first)) {
          loaded_context_slots_.emplace(loaded);
        }
      }
    }
    if (!loaded_context_slots_.empty()) {
      if (loop_effects->may_have_aliasing_contexts) {
        may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kAlways;
      } else {
        DCHECK_EQ(may_have_aliasing_contexts_, ContextSlotLoadsAlias::kNever);
        may_have_aliasing_contexts_ = other.may_have_aliasing_contexts();
      }
    }
  }
  // LINT.ThenChange(:IsCompatibleWithLoopHeader)

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

// LINT.IfChange(IsCompatibleWithLoopHeader)
bool KnownNodeAspects::IsCompatibleWithLoopHeader(
    const KnownNodeAspects& loop_header) const {

  // Analysis state can change with loads.
  if (!loop_header.loaded_context_slots_.empty() &&
      loop_header.may_have_aliasing_contexts() !=
          ContextSlotLoadsAlias::kAlways &&
      loop_header.may_have_aliasing_contexts() !=
          may_have_aliasing_contexts() &&
      may_have_aliasing_contexts() != ContextSlotLoadsAlias::kNever) {
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

  if (!AspectIncludes(loop_header.node_infos_, node_infos_,
                      LoopHeaderCompatibleWithBackEdge, NodeInfoIsEmpty)) {
    if (V8_UNLIKELY(v8_flags.trace_maglev_loop_speeling)) {
      std::cout << "KNA after loop has incompatible node_infos\n";
    }
    // When `!had_effects`, LoopHeaderCompatibleWithBackEdge only runs as
    // debug-mode verification of effect_epoch_ maintenance. However, there are
    // semantic differences between the epoch and NodeInfo, which means that the
    // two can occasionally disagree and *both are correct*.
    //
    // * The maps in NodeInfo are collected by lower tiers and reflect all
    // knowledge for a bytecode position.
    // * The epoch is maintained by maglev compilation; in particular, this
    // means that due to things like loop peeling and deopts, semantics may
    // differ from NodeInfo.
    //   * Loop peeling: the peeled iteration may generate specialized code and
    //   thus contain no side effects whereas the actual loop code does.
    //   * Deopts: Maglev may emit unconditional deopts, marking all successors
    //   of the deopts dead from the Maglev perspective while they are not
    //   actually dead in lower tiers. Side effects in such zombie blocks will
    //   not be considered in the epoch, but are considered in NodeInfo.
    //
    // That means the following check, which we originally had here, does not
    // hold in general wrt the expectation that `!had_effects ->
    // LoopHeaderCompatibleWithBackEdge`. It *is* possible for the latter to be
    // `false`, while the epoch says "no side effects occurred".
    //
    // Disable this check for now, BUT come back soon and find a better
    // solution.
    //
    // Note that when `had_effects == true`, LoopHeaderCompatibleWithBackEdge
    // is NOT just for verification. Instead, it detects when node_infos_
    // indicate relevant side effects.
    //
    // TODO(jgruber,olivf): Investigate further what an appropriate check could
    // look like here.
    //
    // DCHECK(had_effects);
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
// LINT.ThenChange(:LoopInvariantFacts, :CloneForLoopHeader)

SmallZoneVector<KnownNodeAspects::LoadedContextSlotsKey, 8>
KnownNodeAspects::ClearAliasedContextSlotsFor(Graph* graph, ValueNode* context,
                                              int offset, ValueNode* value) {
  SmallZoneVector<LoadedContextSlotsKey, 8> aliased_slots_(graph->zone());
  UpdateMayHaveAliasingContexts(graph->broker(),
                                graph->broker()->local_isolate(), context);
  if (may_have_aliasing_contexts() == ContextSlotLoadsAlias::kAlways) {
    for (auto& cache : loaded_context_slots_) {
      int cached_offset = std::get<int>(cache.first);
      ValueNode* cached_context = std::get<ValueNode*>(cache.first);
      if (cached_offset == offset && cached_context != context) {
        if (graph->ContextMayAlias(cached_context, {}) &&
            cache.second != value) {
          if (V8_UNLIKELY(v8_flags.trace_maglev_kna)) {
            std::cout << kRed << "[KNA] Clear context slot: "
                      << PrintNodeLabel(std::get<ValueNode*>(cache.first))
                      << "[" << offset << "] alias " << PrintNode(value)
                      << kReset << std::endl;
          }
          cache.second = nullptr;
          aliased_slots_.push_back(cache.first);
        }
      }
    }
  }
  return aliased_slots_;
}

bool KnownNodeAspects::SetContextCachedValue(ValueNode* context, int offset,
                                             ValueNode* value,
                                             MaybeAssignedFlag assigned) {
  value = value->UnwrapIdentities();
  auto& target_map = (assigned == kMaybeAssigned) ? loaded_context_slots_
                                                  : loaded_context_constants_;

  auto [it, inserted] = target_map.insert({{context, offset}, value});

  if (!inserted) {
    it->second = value;
    return false;
  }

  return true;
}

KnownNodeAspects::ContextStoreResult KnownNodeAspects::RecordContextSlotStore(
    Graph* graph, ValueNode* context, int offset, ValueNode* value,
    MaybeAssignedFlag assigned) {
  SmallZoneVector<LoadedContextSlotsKey, 8> aliased_slots =
      ClearAliasedContextSlotsFor(graph, context, offset, value);

  bool inserted_or_updated =
      SetContextCachedValue(context, offset, value, assigned);

  if (!inserted_or_updated) {
    return {ContextStoreResult::kNone, std::move(aliased_slots)};
  }
  return {ContextStoreResult::kSetNewValue, std::move(aliased_slots)};
}

#ifdef DEBUG
bool IsStringRootIndex(RootIndex index) {
  if (index > RootIndex::kLastReadOnlyRoot) return false;
  ReadOnlyRoots roots = GetReadOnlyRoots();
  Tagged<Object> obj = roots.object_at(index);
  return IsInternalizedString(obj);
}
#endif

void KnownNodeAspects::Print(std::ostream& os) const {
  os << kBold << kCyan << "=== KnownNodeAspects (epoch: " << effect_epoch_
     << ", side_effects_require_invalidation: "
     << side_effects_require_invalidation_
     << ", aliasing_contexts: " << static_cast<int>(may_have_aliasing_contexts_)
     << ") ===" << kReset << "\n";

  if (!node_infos_.empty()) {
    os << kBold << "Node Infos:" << kReset << "\n";
    for (const auto& [node, info] : node_infos_) {
      os << "  - " << kGreen << PrintNodeLabel(node) << kReset
         << ": type=" << kYellow << info.type() << kReset;
      if (info.possible_maps_are_known()) {
        os << ", maps=";
        PrintPossibleMaps(os, info.possible_maps());
        if (info.maps_are_stale()) {
          os << " " << kRed << "[stale]" << kReset;
        }
      }
      if (info.any_map_is_unstable()) {
        os << " " << kRed << "[unstable]" << kReset;
      }
      if (!info.alternative().has_none()) {
        os << ", alternatives={";
        bool first_alt = true;
        auto print_alt = [&](const char* name, ValueNode* alt_node) {
          if (alt_node) {
            if (!first_alt) os << ", ";
            first_alt = false;
            os << name << ": " << kGreen << PrintNodeLabel(alt_node) << kReset;
          }
        };
        print_alt("tagged", info.alternative().tagged());
        print_alt("int32", info.alternative().int32());
        print_alt("truncated_int32_to_number",
                  info.alternative().truncated_int32_to_number());
        print_alt("float64", info.alternative().float64());
        print_alt("holey_float64", info.alternative().holey_float64());
        print_alt("checked_value", info.alternative().checked_value());
        os << "}";
      }
      os << "\n";
    }
  }

  auto print_properties = [&](const char* label,
                              const LoadedPropertyMap& properties) {
    if (properties.empty()) return;
    os << kBold << label << ":" << kReset << "\n";
    for (const auto& [key, map] : properties) {
      os << "  - " << kYellow << key << kReset << ": { ";
      bool is_first = true;
      for (const auto& [object, value] : map) {
        if (!is_first) os << ", ";
        is_first = false;
        os << kGreen << PrintNodeLabel(object) << kReset << " => " << kGreen
           << PrintNodeLabel(value) << kReset;
      }
      os << " }\n";
    }
  };

  print_properties("Loaded Constant Properties", loaded_constant_properties_);
  print_properties("Loaded Properties", loaded_properties_);

  auto print_context_slots = [&](const char* label,
                                 const LoadedContextSlots& slots) {
    if (slots.empty()) return;
    os << kBold << label << ":" << kReset << "\n";
    for (const auto& [key, object] : slots) {
      if (!object) continue;
      os << "  - " << kGreen << PrintNodeLabel(std::get<ValueNode*>(key))
         << kReset << "@" << std::get<int>(key) << " => " << kGreen
         << PrintNodeLabel(object) << kReset << "\n";
    }
  };

  print_context_slots("Loaded Context Constants", loaded_context_constants_);
  print_context_slots("Loaded Context Slots", loaded_context_slots_);

  if (!available_expressions_.empty()) {
    os << kBold << "Available Expressions:" << kReset << "\n";
    for (const auto& [hash, expr] : available_expressions_) {
      os << "  - " << kYellow << "0x" << std::hex << hash << std::dec << kReset
         << ": " << kGreen << PrintNodeLabel(expr.node) << kReset
         << " (epoch: " << expr.effect_epoch << ")\n";
    }
  }

  if (!virtual_objects_.is_empty()) {
    os << kBold << "Virtual Objects:" << kReset << "\n";
    virtual_objects_.Print(os, "  ");
  }
}

std::ostream& operator<<(std::ostream& os, const KnownNodeAspects& aspects) {
  aspects.Print(os);
  return os;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
