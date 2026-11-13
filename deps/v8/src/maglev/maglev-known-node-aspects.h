// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_KNOWN_NODE_ASPECTS_H_
#define V8_MAGLEV_MAGLEV_KNOWN_NODE_ASPECTS_H_

#include <iosfwd>
#include <utility>

#include "src/base/logging.h"
#include "src/base/sanitizer/asan.h"
#include "src/common/globals.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-kna-map.h"
#include "src/maglev/maglev-node-type.h"
#include "src/objects/contexts.h"

namespace v8 {
namespace internal {
namespace maglev {

class Graph;
struct LoopEffects;
class KnownNodeAspects;
class TraceLogger;

enum class V8_NODISCARD EnsureTypeResult {
  kAlreadyHadType,
  kTypeUpdated,
  kContradiction
};
using UpdateTypeOnContradiction =
    base::StrongAlias<struct UpdateTypeOnContradictionTag, bool>;

using PossibleMaps = compiler::ZoneRefSet<Map>;

#ifdef DEBUG
V8_EXPORT_PRIVATE bool IsStringRootIndex(RootIndex index);

inline bool IsInternalizedStringConstant(ValueNode* node) {
  if (auto constant = node->TryCast<HeapConstant>()) {
    return constant->ref().IsInternalizedString();
  }
  if (auto root_constant = node->TryCast<RootConstant>()) {
    return IsStringRootIndex(root_constant->index());
  }
  return false;
}

inline bool IsNonInternalizedStringConstant(ValueNode* node) {
  if (auto constant = node->TryCast<HeapConstant>()) {
    return constant->ref().IsString() &&
           !constant->ref().IsInternalizedString();
  }
  // We don't need to check RootConstants because they are always internalized
  // strings.
  return false;
}
#endif  // DEBUG

class NodeInfo {
 public:
  NodeInfo() = default;

  NodeType type() const { return type_; }
  void set_type(NodeType type) { type_ = type; }
  NodeType IntersectType(NodeType other) {
    return type_ = maglev::IntersectType(type_, other);
  }
  NodeType UnionType(NodeType other) {
    return type_ = maglev::UnionType(type_, other);
  }

  // Optional alternative nodes with the equivalent value but a different
  // representation.
  class AlternativeNodes {
   public:
    AlternativeNodes() { store_.fill(nullptr); }

#define ALTERNATIVES(V)                                       \
  V(tagged, Tagged, Tagged)                                   \
  V(int32, Int32, Int32)                                      \
  V(truncated_int32_to_number, TruncatedInt32ToNumber, Int32) \
  V(float64, Float64, Float64)                                \
  V(holey_float64, HoleyFloat64, HoleyFloat64)                \
  V(checked_value, CheckedValue, Tagged)

    enum Kind {
#define KIND(name, Name, repr) k##Name,
      ALTERNATIVES(KIND)
#undef KIND
          kNumberOfAlternatives
    };

    static void DCheckAlternativeOverwrite(Kind kind, ValueNode* old_alt,
                                           ValueNode* new_alt) {
#ifdef DEBUG
      if (old_alt == nullptr) return;

      if (kind == Kind::kCheckedValue) {
        // For checked_value, we only allow:
        //     1. Overwriting a non-internalized constant by
        //     CheckedInternalizedString(constant string input).
        //
        //     2. Overwriting CheckedInternalizedString(non-constant input) by a
        //     string constant.
        //
        //     3. Overwriting CheckedInternalizedString(non-internalized
        //     constant input) by an internalized string constant.
        //
        // The idea of those rules is to keep the strongest possible alternative
        // in order to A) elide future CheckedInternalizedString operations, and
        // B) constant-fold future loads from the string (in particular its
        // length).
        // In that regard, 2. doesn't do an amazing job to avoid duplicated
        // CheckedInternalizedString, but it ensures that we duplicate it at
        // most once: we could have the following transitions:
        //
        //      CheckedInternalizedString(non-constant)
        //   -> string constant
        //   -> CheckedInternalizedString(string constant)
        //
        // So we would have emitted 2 CheckedInternalizedString, but we won't
        // emit a third one, and the only allowed transition from this point is
        // to an internalized string constant (which will also prevent future
        // CheckedInternalizedStrings from being emitted).
        if (IsNonInternalizedStringConstant(old_alt)) {
          DCHECK_EQ(new_alt->opcode(), Opcode::kCheckedInternalizedString);
        } else if (old_alt->opcode() == Opcode::kCheckedInternalizedString) {
          if (IsConstantNode(old_alt->input_node(0)->opcode())) {
            DCHECK(IsInternalizedStringConstant(new_alt));
          }
        } else {
          // Any other overwrite of checked_value is forbidden.
          DCHECK(false);
        }
      } else {
        // For other alternatives, the old one shouldn't be a constant and the
        // new one should be.
        DCHECK(!IsConstantNode(old_alt->opcode()));
        DCHECK(IsConstantNode(new_alt->opcode()));
      }
#endif
    }

#define API(name, Name, repr)                                             \
  ValueNode* name() const {                                               \
    if (!store_[Kind::k##Name]) return nullptr;                           \
    return store_[Kind::k##Name]->UnwrapIdentities();                     \
  }                                                                       \
  ValueNode* set_##name(ValueNode* val) {                                 \
    DCHECK_EQ(val->value_representation(), ValueRepresentation::k##repr); \
    DCheckAlternativeOverwrite(Kind::k##Name, name(), val);               \
    return store_[Kind::k##Name] = val;                                   \
  }                                                                       \
  template <typename Function>                                            \
  ValueNode* get_or_set_##name(Function create) {                         \
    ValueNode* existing_alt = name();                                     \
    if (existing_alt != nullptr) return existing_alt;                     \
    ValueNode* new_alt = create();                                        \
    if (new_alt) {                                                        \
      return set_##name(new_alt);                                         \
    }                                                                     \
    return nullptr;                                                       \
  }
    ALTERNATIVES(API)
#undef API
#undef ALTERNATIVES

    ValueNode* get(UseRepresentation repr) const {
      switch (repr) {
        case UseRepresentation::kTagged:
        case UseRepresentation::kTaggedForNumberToString:
          return tagged();
        case UseRepresentation::kInt32:
          return int32();
        case UseRepresentation::kTruncatedInt32:
          return truncated_int32_to_number();
        case UseRepresentation::kFloat64:
          return float64();
        case UseRepresentation::kHoleyFloat64:
          return holey_float64();
        case UseRepresentation::kUint32:
        case UseRepresentation::kNonTruncated:
          UNREACHABLE();
      }
      UNREACHABLE();
    }

    bool has_none() const { return store_ == AlternativeNodes().store_; }

    bool operator==(const AlternativeNodes&) const = default;

    void MergeWith(const AlternativeNodes& other) {
      for (size_t i = 0; i < Kind::kNumberOfAlternatives; ++i) {
        if (store_[i] && store_[i] != other.store_[i]) {
          store_[i] = nullptr;
        }
      }
    }

    // Fills in alternatives we don't have from `other`. Only valid when both
    // alternative sets describe the same underlying value.
    void FillMissingFrom(const AlternativeNodes& other) {
      for (size_t i = 0; i < Kind::kNumberOfAlternatives; ++i) {
        if (store_[i] == nullptr) store_[i] = other.store_[i];
      }
    }

   private:
    // TODO(leszeks): At least one of these is redundant for every node,
    // consider a more compressed form or even linked list.
    std::array<ValueNode*, Kind::kNumberOfAlternatives> store_;

    // Prevent callers from copying these when they try to update the
    // alternatives by making these private.
    AlternativeNodes(const AlternativeNodes&) V8_NOEXCEPT = default;
    AlternativeNodes& operator=(const AlternativeNodes&) V8_NOEXCEPT = default;
    friend class NodeInfo;
  };

  const AlternativeNodes& alternative() const { return alternative_; }
  AlternativeNodes& alternative() { return alternative_; }

  bool no_info_available() const {
    return type_ == NodeType::kUnknown && alternative_.has_none() &&
           !possible_maps_are_known_;
  }

  bool is_smi() const { return NodeTypeIsSmi(type_); }
  bool is_any_heap_object() const { return NodeTypeIsAnyHeapObject(type_); }
  bool is_string() const { return NodeTypeIsString(type_); }
  bool is_internalized_string() const {
    return NodeTypeIsInternalizedString(type_);
  }
  bool is_symbol() const { return NodeTypeIsSymbol(type_); }

  // Mutate this node info by merging in another node info, with the result
  // being a node info that is the subset of information valid in both inputs.
  void MergeWith(const NodeInfo& other, Zone* zone,
                 bool& side_effects_require_invalidation) {
    alternative_.MergeWith(other.alternative_);
    MergeTypeAndMaps(other, zone, side_effects_require_invalidation);
  }

  void MergeTypeAndMaps(const NodeInfo& other, Zone* zone,
                        bool& side_effects_require_invalidation) {
    UnionType(other.type_);
    if (possible_maps_are_known_) {
      if (other.possible_maps_are_known_) {
        // Map sets are the set of _possible_ maps, so on a merge we need to
        // _union_ them together (i.e. intersect the set of impossible maps).
        possible_maps_.Union(other.possible_maps_, zone);
        // If either path is stale, the result is stale.
        maps_are_stale_ = maps_are_stale_ || other.maps_are_stale_;
      } else {
        possible_maps_.clear();
        possible_maps_are_known_ = false;
        maps_are_stale_ = false;
      }
    }

    any_map_is_unstable_ = possible_maps_are_known_ &&
                           (any_map_is_unstable_ || other.any_map_is_unstable_);

    if (any_map_is_unstable_ && !maps_are_stale_) {
      side_effects_require_invalidation = true;
    }
  }

  // Combines info from `other`, which (after key-normalization) describes the
  // same underlying value as `this`. Every fact in either entry is valid, so we
  // keep the most precise type and the union of conversion alternatives.
  // TODO(victorgomes): also union possible_maps_ on collision (needs a zone).
  void CombineSameValueFrom(const NodeInfo& other) {
    IntersectType(other.type_);
    alternative_.FillMissingFrom(other.alternative_);
  }

  void OnSideEffect() {
    if (any_map_is_unstable_) maps_are_stale_ = true;
  }

  template <typename Function>
  bool MarkMapsStaleIfAny(const Function& condition) {
    if (!any_map_is_unstable_) return false;
    for (auto map : possible_maps_) {
      if (condition(map)) {
        maps_are_stale_ = true;
        return true;
      }
    }
    return false;
  }

  bool possible_maps_are_known() const { return possible_maps_are_known_; }

  const PossibleMaps& possible_maps() const {
    // If !possible_maps_are_known_ then every map is possible and using the
    // (probably empty) possible_maps_ set is definitely wrong.
    CHECK(possible_maps_are_known_);
    return possible_maps_;
  }

  static void TraceSetPossibleMaps(const KnownNodeAspects& known_node_aspects,
                                   const NodeInfo* node_info,
                                   const PossibleMaps& possible_maps);

  V8_NODISCARD bool SetPossibleMaps(
      const PossibleMaps& possible_maps, bool any_map_is_unstable,
      NodeType possible_type, compiler::JSHeapBroker* broker,
      const KnownNodeAspects& known_node_aspects) {
    if (V8_UNLIKELY(v8_flags.trace_maglev_kna)) {
      TraceSetPossibleMaps(known_node_aspects, this, possible_maps);
    }

    possible_maps_ = possible_maps;
    possible_maps_are_known_ = true;
    any_map_is_unstable_ = any_map_is_unstable;
    maps_are_stale_ = false;
#ifdef DEBUG
    if (possible_maps.size()) {
      NodeType expected = StaticTypeForMap(*possible_maps.begin(), broker);
      for (auto map : possible_maps) {
        expected = maglev::UnionType(StaticTypeForMap(map, broker), expected);
      }
      // Ensure the claimed type is not narrower than what can be learned from
      // the map checks.
      DCHECK(NodeTypeIs(expected, possible_type));
    } else {
      DCHECK_EQ(possible_type, NodeType::kUnknown);
    }
#endif
    IntersectType(possible_type);
#ifdef DEBUG
    DCHECK_IMPLIES(
        !possible_maps_.is_empty() && NodeTypeCanBe(type_, NodeType::kSmi),
        std::any_of(
            possible_maps_.begin(), possible_maps_.end(),
            [](compiler::MapRef map) { return map.IsHeapNumberMap(); }));
#endif
    return !IsEmptyNodeType(type_);
  }

  // "Stale" means that 1. we've seen unstable maps, and 2. a side effect may
  // have occurred in the meantime.
  void MarkFresh() { maps_are_stale_ = false; }
  bool maps_are_stale() const { return maps_are_stale_; }

  // Objects with "unstable" maps may transition and thus need protection with
  // map checks. (Objects with "stable" maps invalidate compilation
  // dependencies when transitioning.)
  bool any_map_is_unstable() const { return any_map_is_unstable_; }

  bool operator==(const NodeInfo&) const = default;

 private:
  NodeType type_ = NodeType::kUnknown;

  bool any_map_is_unstable_ = false;
  bool maps_are_stale_ = false;

  // Maps for a node. Sets of maps that only contain stable maps are valid
  // across side-effecting calls, as long as we install a dependency, otherwise
  // they are cleared on side-effects.
  // TODO(v8:7700): Investigate a better data structure to use than ZoneMap.
  bool possible_maps_are_known_ = false;
  PossibleMaps possible_maps_;

  AlternativeNodes alternative_;
};

class KnownNodeAspects {
  struct AvailableExpression {
    NodeBase* node;
    uint32_t effect_epoch;
    bool operator==(const AvailableExpression&) const = default;
  };
  using AvailableExpressionsMap = KnaMap<uint32_t, AvailableExpression>;
  using TaggedKeyedPropertiesMap =
      KnaMap<std::pair<ValueNode*, ValueNode*>, ValueNode*>;

 public:
  // Permanently valid if checked in a dominator.
  using NodeInfos = KnaMap<ValueNode*, NodeInfo>;

  struct KNAPool;
  struct Deleter {
    inline void operator()(KnownNodeAspects* kna) const noexcept;
  };
  using Ptr = std::unique_ptr<KnownNodeAspects, Deleter>;

  static inline Ptr New(KNAPool* pool);

  KnownNodeAspects& operator=(const KnownNodeAspects& other) = delete;
  KnownNodeAspects(KnownNodeAspects&& other) = delete;
  KnownNodeAspects& operator=(KnownNodeAspects&& other) = delete;

  inline Ptr Clone() const;

  // Unwraps identities and single-input phis in the node-valued keys of the
  // aspect maps (node_infos_ keys, loaded-property object keys and context-slot
  // context keys), rebuilding them and combining node_infos entries whose keys
  // collide afterwards. Used at loop headers to normalize the cached backedge
  // KNA before merging it into the forward KNA.
  void UnwrapIdentitiesAndPhisInKeys();

  // Loop headers can safely clone the node types, since those won't be
  // invalidated in the loop body, and similarly stable maps will have
  // dependencies installed. Unstable maps however might be invalidated by
  // calls, and we don't know about these until it's too late.
  Ptr CloneForLoopHeader(bool optimistic_initial_state,
                         LoopEffects* loop_effects) const;

  void ClearUnstableNodeAspectsForStoreMap(StoreMap* node,
                                           bool is_tracing_enabled);
  void ClearUnstableNodeAspectsForMigration(ValueNode* object,
                                            bool is_tracing_enabled);
  void ClearUnstableNodeAspectsForElementsTransition(
      const ZoneVector<compiler::MapRef>& transition_sources,
      bool is_tracing_enabled);
  void ClearUnstableNodeAspects(bool is_tracing_enabled);

  // Returns the node info for `object` if its possible maps are known and not
  // stale, otherwise nullptr. Cached [Elements] facts survive an elements-kind
  // transition only for such objects: stale or unknown maps mean the object
  // may alias a transition source, whose elements store can be reallocated.
  const NodeInfo* TryGetInfoWithFreshMaps(ValueNode* object) const;

  void OnSideEffect() {
    // A side effect could change existing objects' maps. For stable maps we
    // know this hasn't happened (because we added a dependency on the maps
    // staying stable and therefore not possible to transition away from), but
    // we can no longer assume that objects with unstable maps still have the
    // same map. Unstable maps can also transition to stable ones, so we have to
    // clear _all_ maps for a node if it had _any_ unstable map.
    if (!side_effects_require_invalidation_) return;
    node_infos_.MutateValues(
        [](ValueNode*, NodeInfo& info) { info.OnSideEffect(); });
    side_effects_require_invalidation_ = false;
  }

  template <typename Function>
  bool MarkMapsStaleIfAny(const Function& condition) {
    if (!side_effects_require_invalidation_) return false;
    bool any_stale = false;
    node_infos_.MutateValues([&](ValueNode*, NodeInfo& info) {
      any_stale |= info.MarkMapsStaleIfAny(condition);
    });
    return any_stale;
  }

  void ClearAvailableExpressions() { available_expressions_.clear(); }

  void ClearAll() {
    loaded_constant_properties_.clear();
    loaded_properties_.clear();
    loaded_tagged_keyed_properties_.clear();
    loaded_context_constants_.clear();
    loaded_context_slots_.clear();
    available_expressions_.clear();
    side_effects_require_invalidation_ = false;
    may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kNever;
    node_infos_.clear();
    virtual_objects_ = {};
  }

  // Returned `NodeInfo*` pointers (from `TryGetInfoFor`,
  // `TryGetMutableInfoFor`, and `GetOrCreateInfoFor`) point directly into
  // `node_infos_` storage and are invalidated by any subsequent insertion,
  // erasure, merge, or `Clone()` on this `KnownNodeAspects` (including emitted
  // IR nodes that trigger `MarkPossibleSideEffect`).
  const NodeInfo* TryGetInfoFor(ValueNode* node) const {
    auto info_it = FindInfo(node);
    if (!IsValid(info_it)) return nullptr;
    return &info_it->second;
  }

  NodeInfo* TryGetMutableInfoFor(ValueNode* node) {
    auto info_it = FindInfo(node);
    if (!IsValid(info_it)) return nullptr;
    return &info_it->second;
  }

  // Slow linear lookup, for debug code only.
  ValueNode* FindNode(const NodeInfo* info) const {
    for (const auto& pair : node_infos_) {
      if (&pair.second == info) return pair.first;
    }
    return nullptr;
  }

  // Note: the returned `NodeInfo*` is invalidated by subsequent insertions,
  // erasures, merges, or `Clone()` on `KnownNodeAspects`.
  NodeInfo* GetOrCreateInfoFor(compiler::JSHeapBroker* broker,
                               ValueNode* node) {
    node = GetCanonical(node);
    auto [info_it, inserted] = node_infos_.try_emplace(node);
    NodeInfo* res = &info_it->second;
    if (inserted) {
      res->IntersectType(node->GetStaticType(broker));
    }
    return res;
  }

  ValueNode* TryGetAlternativeFor(ValueNode* node,
                                  UseRepresentation repr) const {
    auto info_it = FindInfo(node);
    if (!IsValid(info_it)) return nullptr;
    return info_it->second.alternative().get(repr);
  }

  std::optional<PossibleMaps> TryGetPossibleMaps(ValueNode* node) const {
    DCHECK_NOT_NULL(node);
    if (const NodeInfo* info = TryGetInfoFor(node)) {
      if (info->possible_maps_are_known()) {
        return info->possible_maps();
      }
      return {};
    }
    return {};
  }

  V8_NODISCARD bool CheckType(compiler::JSHeapBroker* broker, ValueNode* node,
                              NodeType type,
                              NodeType* current_type = nullptr) const {
    if (!current_type && node->StaticTypeIs(broker, type)) return true;
    NodeType known_type = GetTypeUnchecked(broker, node);
    if (current_type) *current_type = known_type;
    return NodeTypeIs(known_type, type);
  }

  V8_NODISCARD NodeType
  CheckTypes(compiler::JSHeapBroker* broker, ValueNode* node,
             std::initializer_list<NodeType> types) const {
    NodeType known_type = GetTypeUnchecked(broker, node);
    for (NodeType type : types) {
      if (NodeTypeIs(known_type, type)) return type;
    }
    return NodeType::kUnknown;
  }

  bool MayBeNullOrUndefined(compiler::JSHeapBroker* broker,
                            ValueNode* node) const {
    if (!NodeTypeMayBeNullOrUndefined(node->GetStaticType(broker))) {
      return false;
    }
    return NodeTypeMayBeNullOrUndefined(GetTypeUnchecked(broker, node));
  }

  bool IsEmptyNodeType(NodeType type) const {
    return v8::internal::maglev::IsEmptyNodeType(type);
  }

  bool IsEmptyNodeType(compiler::JSHeapBroker* broker, ValueNode* node) const {
    // We use GetTypeUnchecked to avoid introducing Traps on None under
    // maglev-assert-types (as the caller of this function is already currently
    // handling the empty node type case).
    return v8::internal::maglev::IsEmptyNodeType(
        GetTypeUnchecked(broker, node));
  }

  EnsureTypeResult EnsureType(compiler::JSHeapBroker* broker, ValueNode* node,
                              NodeType type, NodeType* old_type = nullptr) {
    return EnsureTypeHelper(broker, node, type, old_type,
                            UpdateTypeOnContradiction{true});
  }

  EnsureTypeResult TryEnsureType(compiler::JSHeapBroker* broker,
                                 ValueNode* node, NodeType type,
                                 NodeType* old_type = nullptr) {
    return EnsureTypeHelper(broker, node, type, old_type,
                            UpdateTypeOnContradiction{false});
  }

  void Merge(const KnownNodeAspects& other, Zone* zone);
  V8_EXPORT_PRIVATE void MergeForLoop(const KnownNodeAspects& backedge,
                                      Zone* zone,
                                      const LoopEffects* loop_effects,
                                      bool loop_has_effects);

  // If IsCompatibleWithLoopHeader(other) returns true, it means that
  // Merge(other) would not remove any information from `this`.
  bool IsCompatibleWithLoopHeader(const KnownNodeAspects& other) const;

  // Side effects can require invalidation e.g. if unstable maps or node types
  // (which are two different conceps) have been recorded.
  void MarkSideEffectsRequireInvalidation() {
    side_effects_require_invalidation_ = true;
  }

  VirtualObjectList& virtual_objects() { return virtual_objects_; }
  const VirtualObjectList& virtual_objects() const { return virtual_objects_; }

  // Cached property loads.

  // Maps key->object->value, so that stores to a key can invalidate all loads
  // of that key (in case the objects are aliasing).
  using PropertyObjectMap = KnaMap<ValueNode*, ValueNode*>;
  using LoadedPropertyMap = KnaMap<PropertyKey, PropertyObjectMap>;

  using LoadedContextSlotsKey = std::tuple<ValueNode*, int>;
  using LoadedContextSlots = KnaMap<LoadedContextSlotsKey, ValueNode*>;

  ValueNode* TryFindLoadedProperty(ValueNode* lookup_start_object,
                                   PropertyKey name) const {
    return TryFindLoadedProperty(loaded_properties_, lookup_start_object, name);
  }
  ValueNode* TryFindLoadedConstantProperty(ValueNode* lookup_start_object,
                                           PropertyKey name) const {
    return TryFindLoadedProperty(loaded_constant_properties_,
                                 lookup_start_object, name);
  }
  ValueNode* TryFindLoadedProperty(ValueNode* lookup_start_object,
                                   PropertyKey name,
                                   PropertyConstness constness) const {
    switch (constness) {
      case PropertyConstness::kConst:
        return TryFindLoadedConstantProperty(lookup_start_object, name);
      case PropertyConstness::kMutable:
        return TryFindLoadedProperty(lookup_start_object, name);
    }
  }

  // Note: the returned `PropertyObjectMap&` reference is invalidated by
  // subsequent insertions, erasures, merges, or `Clone()` on the outer
  // `LoadedPropertyMap`.
  PropertyObjectMap& GetLoadedPropertiesForKey(PropertyConstness constness,
                                               PropertyKey key) {
    LoadedPropertyMap& properties = constness == PropertyConstness::kConst
                                        ? loaded_constant_properties_
                                        : loaded_properties_;
    // Try to get loaded_properties[key] if it already exists, otherwise
    // construct loaded_properties[key] = PropertyObjectMap{pool}.
    return properties.try_emplace(key, &pool_->property_object_pool)
        .first->second;
  }

  bool ClearLoadedPropertiesForKey(PropertyKey key) {
    auto it = std::as_const(loaded_properties_).find(key);
    if (it != loaded_properties_.end()) {
      if (!it->second.empty()) {
        loaded_properties_.find(key)->second.clear();
      }
      return true;
    }
    return false;
  }

  // Cache of LoadFixedArrayElement results keyed by (elements, index). Any
  // write that does not pass PreservesTaggedKeyedProperties clears it.
  LoadFixedArrayElement* TryFindTaggedKeyedProperty(ValueNode* elements,
                                                    ValueNode* index) const {
    auto it = loaded_tagged_keyed_properties_.find(
        {elements->UnwrapIdentities(), index->UnwrapIdentities()});
    if (it == loaded_tagged_keyed_properties_.end()) return nullptr;
    return it->second->UnwrapIdentities()->TryCast<LoadFixedArrayElement>();
  }
  void RecordTaggedKeyedProperty(ValueNode* elements, ValueNode* index,
                                 LoadFixedArrayElement* load) {
    std::pair<ValueNode*, ValueNode*> key{elements->UnwrapIdentities(),
                                          index->UnwrapIdentities()};
    auto it = loaded_tagged_keyed_properties_.find(key);
    if (it != loaded_tagged_keyed_properties_.end()) {
      it->second = load;
      return;
    }
    if (loaded_tagged_keyed_properties_.size() >= kMaxTaggedKeyedProperties) {
      return;
    }
    loaded_tagged_keyed_properties_.emplace(key, load);
  }
  void ClearTaggedKeyedProperties() { loaded_tagged_keyed_properties_.clear(); }

  void increment_effect_epoch() {
    if (effect_epoch_ < kEffectEpochOverflow) effect_epoch_++;
  }

  template <typename NodeT, typename... Args>
  NodeT* FindExpression(uint32_t hash,
                        std::array<ValueNode*, NodeT::kInputCount>& inputs,
                        Args&&... args) {
    auto it = std::as_const(available_expressions_).find(hash);
    if (it == available_expressions_.end()) return nullptr;

    static constexpr Opcode op = Node::opcode_of<NodeT>;
    auto candidate = it->second.node;

    if (candidate->Is<Identity>()) {
      // This expression was removed from the graph. Do not reuse it.
      available_expressions_.erase(hash);
      return nullptr;
    }

    const bool sanity_check =
        candidate->template Is<NodeT>() &&
        static_cast<size_t>(candidate->input_count()) == inputs.size();
    DCHECK_IMPLIES(sanity_check,
                   (StaticPropertiesForOpcode(op) & candidate->properties()) ==
                       candidate->properties());
    const bool epoch_check = !Node::needs_epoch_check(op) ||
                             effect_epoch_ <= it->second.effect_epoch;
    if (sanity_check && epoch_check) {
      if (static_cast<NodeT*>(candidate)->options() ==
          std::forward_as_tuple(std::forward<Args>(args)...)) {
        int i = 0;
        for (const auto& inp : inputs) {
          if (inp->UnwrapIdentities() !=
              candidate->input(i).node()->UnwrapIdentities()) {
            break;
          }
          i++;
        }
        if (static_cast<size_t>(i) == inputs.size()) {
          return static_cast<NodeT*>(candidate);
        }
      }
    }
    if (!epoch_check) {
      available_expressions_.erase(hash);
    }
    return nullptr;
  }

  template <typename NodeT>
  void AddExpression(uint32_t hash, NodeT* node) {
    static constexpr Opcode op = Node::opcode_of<NodeT>;
    uint32_t epoch = Node::needs_epoch_check(op)
                         ? effect_epoch_
                         : KnownNodeAspects::kEffectEpochForPureInstructions;
    if (epoch == kEffectEpochOverflow) return;
    available_expressions_.emplace(hash, AvailableExpression{node, epoch});
  }

  enum class ContextSlotLoadsAlias : uint8_t {
    kNever,
    kOnlyLoadsRelativeToCurrentContext,
    kOnlyLoadsRelativeToConstant,
    kAlways,
  };
  ContextSlotLoadsAlias may_have_aliasing_contexts() const {
    return may_have_aliasing_contexts_;
  }
  static ContextSlotLoadsAlias ContextSlotLoadsAliasMerge(
      ContextSlotLoadsAlias m1, ContextSlotLoadsAlias m2) {
    if (m1 == m2) return m1;
    if (m1 == ContextSlotLoadsAlias::kNever) return m2;
    if (m2 == ContextSlotLoadsAlias::kNever) return m1;
    return ContextSlotLoadsAlias::kAlways;
  }
  struct ContextStoreResult {
    enum Type {
      kNone,
      kUpdatedValue,
      kSetNewValue,
    } type;
    SmallZoneVector<LoadedContextSlotsKey, 8> aliased_slots;
  };

  void UpdateMayHaveAliasingContexts(compiler::JSHeapBroker* broker,
                                     LocalIsolate* local_isolate,
                                     ValueNode* context);

  ContextStoreResult RecordContextSlotStore(Graph* graph, ValueNode* context,
                                            int offset, ValueNode* value,
                                            MaybeAssignedFlag assigned);

  // Returns the value in the cache if exists without adding a new cache entry.
  ValueNode* TryGetContextCachedValue(ValueNode* context, int offset,
                                      MaybeAssignedFlag assigned) {
    auto& map = (assigned == kMaybeAssigned) ? loaded_context_slots_
                                             : loaded_context_constants_;
    auto it = std::as_const(map).find({context, offset});
    if (it == map.end()) return nullptr;
    ValueNode* value = it->second;
    if (value) {
      ValueNode* unwrapped = value->UnwrapIdentities();
      if (unwrapped != value) [[unlikely]] {
        map.find({context, offset})->second = unwrapped;
        return unwrapped;
      }
    }
    return value;
  }
  // Returns a reference to the cached value, inserting a default entry if not
  // present. The returned `ValueNode*&` reference is invalidated by subsequent
  // insertions, erasures, merges, or `Clone()` on the context cache map.
  ValueNode*& GetContextCachedValue(ValueNode* context, int offset,
                                    MaybeAssignedFlag assigned) {
    ValueNode*& cached_value =
        (assigned == kMaybeAssigned)
            ? loaded_context_slots_[{context, offset}]
            : loaded_context_constants_[{context, offset}];
    if (cached_value) {
      cached_value = cached_value->UnwrapIdentities();
    }
    return cached_value;
  }
  bool HasContextCacheValue(ValueNode* context, int offset,
                            MaybeAssignedFlag assigned) const {
    return (assigned == kMaybeAssigned)
               ? loaded_context_slots_.contains({context, offset})
               : loaded_context_constants_.contains({context, offset});
  }
  bool IsContextCacheEmpty(MaybeAssignedFlag assigned) const {
    return (assigned == kMaybeAssigned) ? loaded_context_slots_.empty()
                                        : loaded_context_constants_.empty();
  }

  static bool BuiltinInvalidatesKNA(Builtin builtin) {
    switch (builtin) {
      // TODO(victorgomes): Add more builtins to the list!
      case Builtin::kCloneFastJSArray:
        return false;
      default:
        return true;
    }
  }

  template <typename NodeT>
  void MarkPossibleSideEffect(NodeT* node, compiler::JSHeapBroker* broker,
                              bool is_tracing_enabled) {
    // Don't do anything for nodes without side effects.
    if constexpr (!NodeT::kProperties.can_write()) return;

    increment_effect_epoch();

    if constexpr (!PreservesTaggedKeyedProperties(Node::opcode_of<NodeT>)) {
      loaded_tagged_keyed_properties_.clear();
    }

    if constexpr (Node::opcode_of<NodeT> == Opcode::kCallBuiltin) {
      if (!BuiltinInvalidatesKNA(node->builtin())) return;
    }

    if constexpr (Node::opcode_of<NodeT> == Opcode::kMaybeGrowFastElements) {
      if (ClearLoadedPropertiesForKey(broker->length_string())) {
        if (V8_UNLIKELY(v8_flags.trace_maglev_kna && is_tracing_enabled)) {
          std::cout << "  * Removing non-constant cached \"length\" property";
        }
      }
    }

    if constexpr (IsElementsArrayWrite(Node::opcode_of<NodeT>)) {
      if (ClearLoadedPropertiesForKey(PropertyKey::Elements())) {
        if (V8_UNLIKELY(v8_flags.trace_maglev_kna && is_tracing_enabled)) {
          std::cout << "  * Removing non-constant cached [Elements]";
        }
      }
    } else if constexpr (std::is_same_v<NodeT, CheckMapsWithMigration> ||
                         std::is_same_v<NodeT,
                                        CheckMapsWithMigrationAndDeopt> ||
                         std::is_same_v<NodeT, MigrateMapIfNeeded>) {
      ClearUnstableNodeAspectsForMigration(node->ReceiverInput().node(),
                                           is_tracing_enabled);
    } else if constexpr (std::is_same_v<NodeT, StoreMap>) {
      ClearUnstableNodeAspectsForStoreMap(node, is_tracing_enabled);
    } else if constexpr (std::is_same_v<NodeT, TransitionElementsKind> ||
                         std::is_same_v<NodeT,
                                        TransitionElementsKindOrCheckMap>) {
      ClearUnstableNodeAspectsForElementsTransition(node->transition_sources(),
                                                    is_tracing_enabled);
    } else if constexpr (!IsSimpleFieldStore(Node::opcode_of<NodeT>) &&
                         !IsTypedArrayStore(Node::opcode_of<NodeT>)) {
      // Don't change known node aspects for simple field stores. The only
      // relevant side effect on these is writes to objects which invalidate
      // loaded properties and context slots, and we invalidate these already as
      // part of emitting the store.
      ClearUnstableNodeAspects(is_tracing_enabled);
    }
  }

  void Print(std::ostream& os) const;
  void Print() const { return Print(std::cout); }

  // Shared per-compilation pool container for KnownNodeAspects and its KnaMap
  // instances, allowing destroyed or merged KnownNodeAspects to recycle storage
  // across basic blocks.
  struct KNAPool {
    explicit KNAPool(Zone* zone)
        : zone(zone),
          node_infos_pool(zone),
          property_object_pool(zone),
          loaded_property_pool(zone),
          tagged_keyed_pool(zone),
          context_slots_pool(zone),
          available_expressions_pool(zone) {}

    template <typename... Args>
    Ptr New(Args&&... args) {
      void* mem;
      if (free_kna != nullptr) {
        mem = free_kna;
        free_kna = free_kna->next;
        ASAN_UNPOISON_MEMORY_REGION(
            reinterpret_cast<char*>(mem) + sizeof(FreeKna),
            sizeof(KnownNodeAspects) - sizeof(FreeKna));
      } else {
        mem = zone->Allocate<KnownNodeAspects>(sizeof(KnownNodeAspects));
      }
      return Ptr(new (mem) KnownNodeAspects(std::forward<Args>(args)...));
    }

    void Free(KnownNodeAspects* kna) noexcept {
      static_assert(sizeof(KnownNodeAspects) >= sizeof(FreeKna));
      static_assert(alignof(KnownNodeAspects) >= alignof(FreeKna));
      static_assert(alignof(KnownNodeAspects) <= 8);
      free_kna = new (kna) FreeKna{free_kna};
      ASAN_POISON_MEMORY_REGION(
          reinterpret_cast<char*>(free_kna) + sizeof(FreeKna),
          sizeof(KnownNodeAspects) - sizeof(FreeKna));
    }

    struct FreeKna {
      FreeKna* next;
    };

    Zone* zone;
    FreeKna* free_kna = nullptr;
    NodeInfos::Pool node_infos_pool;
    PropertyObjectMap::Pool property_object_pool;
    LoadedPropertyMap::Pool loaded_property_pool;
    TaggedKeyedPropertiesMap::Pool tagged_keyed_pool;
    LoadedContextSlots::Pool context_slots_pool;
    AvailableExpressionsMap::Pool available_expressions_pool;
  };

 private:
  EnsureTypeResult EnsureTypeHelper(
      compiler::JSHeapBroker* broker, ValueNode* node, NodeType type,
      NodeType* old_type,
      UpdateTypeOnContradiction update_type_on_contradiction) {
    NodeType current_type = GetTypeUnchecked(broker, node);
    if (old_type) *old_type = current_type;
    NodeType new_type = IntersectType(current_type, type);
    if (IsEmptyNodeType(new_type)) {
      if (update_type_on_contradiction) {
        GetOrCreateInfoFor(broker, node)->set_type(NodeType::kNone);
      }
      return EnsureTypeResult::kContradiction;
    }
    if (NodeTypeIs(current_type, type)) {
      return EnsureTypeResult::kAlreadyHadType;
    }
    GetOrCreateInfoFor(broker, node)->IntersectType(new_type);
    return EnsureTypeResult::kTypeUpdated;
  }

  bool SetContextCachedValue(ValueNode* context, int offset, ValueNode* value,
                             MaybeAssignedFlag assigned);

  SmallZoneVector<LoadedContextSlotsKey, 8> ClearAliasedContextSlotsFor(
      Graph* graph, ValueNode* context, int offset, ValueNode* value);

  bool MarkSingleMapAsStale(ValueNode* node, bool is_tracing_enabled);

  static constexpr uint32_t kEffectEpochForPureInstructions =
      std::numeric_limits<uint32_t>::max();
  static constexpr uint32_t kEffectEpochOverflow =
      kEffectEpochForPureInstructions - 1;

  // These can call GetTypeUnchecked directly. Other uses should ideally go
  // through MaglevReducer::GetType or similar and add type assertions if
  // v8_flags.maglev_assert_types is on.
  // TODO(477184397): Consider unfriending.
  template <typename U>
  friend class MaglevReducer;
  friend class RecomputeKnownNodeAspectsProcessor;
  friend class MergePointInterpreterFrameState;
  friend class LoopMergePointInterpreterFrameState;

  NodeType GetTypeUnchecked(compiler::JSHeapBroker* broker,
                            ValueNode* node) const {
    // We first check the KnownNodeAspects in order to return the most precise
    // type possible.
    auto info = TryGetInfoFor(node);
    if (info == nullptr) {
      // If this node has no NodeInfo (or not known type in its NodeInfo), we
      // fall back to its static type.
      return node->GetStaticType(broker);
    }
    NodeType actual_type =
        IntersectType(info->type(), node->GetStaticType(broker));
    if (node->Is<ReturnedValue>()) {
      // The returned value might be more precise than the one stored in the
      // node info.
      actual_type = IntersectType(
          actual_type, GetTypeUnchecked(broker, node->input_node(0)));
    }
#ifdef DEBUG
    NodeType static_type = node->GetStaticType(broker);
    // TODO(428667907): Ideally we should bail out early for the kNone type.
    if (!NodeTypeIs(actual_type, static_type, NodeTypeIsVariant::kAllowNone)) {
      // In case we needed a numerical alternative of a smi value, the type
      // must generalize. In all other cases the node info type should reflect
      // the actual type.
      DCHECK(static_type == NodeType::kSmi &&
             actual_type == NodeType::kNumber &&
             !TryGetInfoFor(node)->alternative().has_none());
    }
#endif  // DEBUG
    return actual_type;
  }

  KNAPool* pool_;
  // Valid across side-effecting calls, as long as we install a dependency.
  LoadedPropertyMap loaded_constant_properties_;
  // Flushed after side-effecting calls.
  LoadedPropertyMap loaded_properties_;
  // (elements, index) -> LoadFixedArrayElement. Flushed by any write that
  // does not pass PreservesTaggedKeyedProperties. Bounded by
  // kMaxTaggedKeyedProperties, since the map is copied at every clone.
  static constexpr size_t kMaxTaggedKeyedProperties = 100;
  TaggedKeyedPropertiesMap loaded_tagged_keyed_properties_;
  // Unconditionally valid across side-effecting calls.
  LoadedContextSlots loaded_context_constants_;
  // Flushed after side-effecting calls.
  LoadedContextSlots loaded_context_slots_;
  // For CSE.
  AvailableExpressionsMap available_expressions_;
  // This is an optimization. We track whether any NodeInfos require work on
  // side effects. If not, we don't even iterate over NodeInfos.
  bool side_effects_require_invalidation_;
  // This field indicates if the current state of loaded_context_slots might
  // contain contexts aliases. If that is the case, then we need to be more
  // conservative about updating the state on stores.
  ContextSlotLoadsAlias may_have_aliasing_contexts_;
  uint32_t effect_epoch_;
  NodeInfos node_infos_;
  VirtualObjectList virtual_objects_;

  friend struct KNAPool;
  explicit KnownNodeAspects(KNAPool* pool)
      : pool_(pool),
        loaded_constant_properties_(&pool_->loaded_property_pool),
        loaded_properties_(&pool_->loaded_property_pool),
        loaded_tagged_keyed_properties_(&pool_->tagged_keyed_pool),
        loaded_context_constants_(&pool_->context_slots_pool),
        loaded_context_slots_(&pool_->context_slots_pool),
        available_expressions_(&pool_->available_expressions_pool),
        side_effects_require_invalidation_(false),
        may_have_aliasing_contexts_(ContextSlotLoadsAlias::kNever),
        effect_epoch_(0),
        node_infos_(&pool_->node_infos_pool),
        virtual_objects_() {}
  // Copy constructor is defaulted but private so that we explicitly call the
  // Clone method.
  KnownNodeAspects(const KnownNodeAspects& other) V8_NOEXCEPT = default;
  // Copy constructor for CloneForLoopHeader.
  KnownNodeAspects(const KnownNodeAspects& other, bool optimistic_initial_state,
                   LoopEffects* loop_effects);

  ValueNode* TryFindLoadedProperty(const LoadedPropertyMap& properties,
                                   ValueNode* lookup_start_object,
                                   PropertyKey name) const {
    auto props_for_name = properties.find(name);
    if (props_for_name == properties.end()) return nullptr;

    auto it = props_for_name->second.find(lookup_start_object);
    if (it == props_for_name->second.end()) return nullptr;

    return it->second->UnwrapIdentities();
  }

  // GetCanonical is used to ensure that nodes in the same conversion chain (eg,
  // a `NumberToInt32` flowing into a `Int32ToFloat64` for instance) share the
  // same NodeInfo. To do so, GetCanonical Unwraps identities and conversions.
  // There is a subteltly though: GetCanonical doesn't Unwrap through Tagged
  // nodes, because information about one tagged node at the end of a
  // conversion/identity-chain doesn't necessarily apply to tagged nodes at the
  // beginning of the chain. For instance, consider:
  //
  //   n1: Param
  //   n2: NumberToInt32(n1)
  //   n3: CheckedSmiTagInt32(n2)
  //
  // Here, n3 has NodeType::kSmi, but propagating to n1 would be wrong, since n1
  // could be a smi-ranged HeapNumber.
  // TODO(496266449): Simply use `Unwrap` once `kTagged` is split into
  // `kTaggedSigned` and `kTaggedPointer` representations.
  static ValueNode* GetCanonical(ValueNode* node) {
    node = node->UnwrapIdentitiesAndReturnedValues();
    if (node->is_tagged()) return node;
    while (node->is_conversion()) {
      ValueNode* input =
          node->input_node(0)->UnwrapIdentitiesAndReturnedValues();
      if (input->is_tagged()) break;
      node = input;
    }
    return node;
  }

  NodeInfos::iterator FindInfo(ValueNode* node) {
    node = GetCanonical(node);
    return node_infos_.find(node);
  }
  NodeInfos::const_iterator FindInfo(ValueNode* node) const {
    node = GetCanonical(node);
    return node_infos_.find(node);
  }
  bool IsValid(NodeInfos::iterator& it) { return it != node_infos_.end(); }
  bool IsValid(NodeInfos::const_iterator& it) const {
    return it != node_infos_.end();
  }
};

inline void KnownNodeAspects::Deleter::operator()(
    KnownNodeAspects* kna) const noexcept {
  KNAPool* pool = kna->pool_;
  kna->~KnownNodeAspects();
  pool->Free(kna);
}

// static
inline KnownNodeAspects::Ptr KnownNodeAspects::New(KNAPool* pool) {
  return pool->New(pool);
}

inline KnownNodeAspects::Ptr KnownNodeAspects::Clone() const {
  return pool_->New(*this);
}

std::ostream& operator<<(std::ostream& os, const KnownNodeAspects& aspects);

template <typename MapContainer>
class KnownMapsMerger {
 public:
  explicit KnownMapsMerger(compiler::JSHeapBroker* broker, Zone* zone,
                           const MapContainer& requested_maps)
      : broker_(broker), zone_(zone), requested_maps_(requested_maps) {}

  void IntersectWithKnownNodeAspects(
      ValueNode* object, const KnownNodeAspects& known_node_aspects) {
    const NodeInfo* node_info = known_node_aspects.TryGetInfoFor(object);
    NodeType type = node_info ? node_info->type() : NodeType::kUnknown;
    // KnownMapsMerger is for avoiding map checks that we've already done, so
    // only consider non-stale maps here.
    if (node_info && node_info->possible_maps_are_known() &&
        !node_info->maps_are_stale()) {
      // TODO(v8:7700): Make intersection non-quadratic.
      for (compiler::MapRef possible_map : node_info->possible_maps()) {
        if (std::find(requested_maps_.begin(), requested_maps_.end(),
                      possible_map) != requested_maps_.end()) {
          // No need to add dependencies, we already have them for all known
          // possible maps.
          // Filter maps which are impossible given this objects type. Since we
          // want to prove that an object with map `map` is not an instance of
          // `type`, we cannot use `StaticTypeForMap`, as it only provides an
          // approximation. This filtering is done to avoid creating
          // non-sensical types later (e.g. if we think only a non-string map
          // is possible, after a string check).
          if (IsInstanceOfNodeType(possible_map, type, broker_) ||
              (possible_map.IsHeapNumberMap() &&
               NodeTypeCanBe(type, NodeType::kSmi))) {
            InsertMap(possible_map);
          }
        } else {
          known_maps_are_subset_of_requested_maps_ = false;
        }
      }
      if (intersect_set_.is_empty()) {
        // TODO(marja): Refactor to return false here explicitly.
        node_type_ = EmptyNodeType();
      }
    } else {
      // A missing entry here means the universal set, i.e., we don't know
      // anything about the possible maps of the object. Intersect with the
      // universal set, which means just insert all requested maps.
      known_maps_are_subset_of_requested_maps_ = false;
      existing_known_maps_found_ = false;
      for (const compiler::MapRef map : requested_maps_) {
        InsertMap(map);
      }
    }
  }

  // Returns false if the object now has the empty type, true otherwise.
  V8_NODISCARD bool UpdateKnownNodeAspects(
      ValueNode* object, KnownNodeAspects& known_node_aspects) {
    // Update known maps.
    auto node_info = known_node_aspects.GetOrCreateInfoFor(broker_, object);
    if (!node_info->SetPossibleMaps(intersect_set_, any_map_is_unstable_,
                                    node_type_, broker_, known_node_aspects)) {
      return false;
    }
    // Make sure known_node_aspects.side_effects_require_invalidation is updated
    // in case any_map_is_unstable changed to true for this object
    // -- this can happen if this was an intersection with the universal set
    // which added new possible unstable maps.
    if (any_map_is_unstable_) {
      known_node_aspects.MarkSideEffectsRequireInvalidation();
    }
    // At this point, known_node_aspects.side_effects_require_invalidation may
    // be true despite there no longer being any unstable maps for any nodes (if
    // this was the only node with unstable maps and this intersection removed
    // those). This is ok, because that's at worst just an overestimate -- we
    // could track whether this node's any_map_is_unstable flipped
    // from true to false, but this is likely overkill. Insert stable map
    // dependencies which weren't inserted yet. This is only needed if our set
    // of known maps was empty and we created it anew based on maps we checked.
    if (!existing_known_maps_found_) {
      for (compiler::MapRef map : intersect_set_) {
        if (map.is_stable()) {
          broker_->dependencies()->DependOnStableMap(map);
        }
      }
    } else {
      // TODO(victorgomes): Add a DCHECK_SLOW that checks if the maps already
      // exist in the CompilationDependencySet.
    }
    return true;
  }

  bool known_maps_are_subset_of_requested_maps() const {
    return known_maps_are_subset_of_requested_maps_;
  }
  bool emit_check_with_migration() const { return emit_check_with_migration_; }

  const compiler::ZoneRefSet<Map>& intersect_set() const {
    return intersect_set_;
  }

  NodeType node_type() const { return node_type_; }

  bool RequestedMapsAdmitSmis() const {
    return std::any_of(
        requested_maps_.begin(), requested_maps_.end(),
        [](compiler::MapRef map) { return map.IsHeapNumberMap(); });
  }

 private:
  compiler::JSHeapBroker* broker_;
  Zone* zone_;
  const MapContainer& requested_maps_;
  compiler::ZoneRefSet<Map> intersect_set_;
  bool known_maps_are_subset_of_requested_maps_ = true;
  bool existing_known_maps_found_ = true;
  bool emit_check_with_migration_ = false;
  bool any_map_is_unstable_ = false;
  NodeType node_type_ = EmptyNodeType();

  Zone* zone() const { return zone_; }

  void InsertMap(compiler::MapRef map) {
    if (map.is_migration_target()) {
      emit_check_with_migration_ = true;
    }
    NodeType new_type = StaticTypeForMap(map, broker_);
    if (new_type == NodeType::kHeapNumber) {
      new_type = UnionType(new_type, NodeType::kSmi);
    }
    node_type_ = UnionType(node_type_, new_type);
    if (!map.is_stable()) {
      any_map_is_unstable_ = true;
    }
    intersect_set_.insert(map, zone());
  }
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_KNOWN_NODE_ASPECTS_H_
