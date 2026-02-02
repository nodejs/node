// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_KNOWN_NODE_ASPECTS_H_
#define V8_MAGLEV_MAGLEV_KNOWN_NODE_ASPECTS_H_

#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

struct LoopEffects;

using PossibleMaps = compiler::ZoneRefSet<Map>;

enum ContextSlotMutability { kImmutable, kMutable };

class NodeInfo {
 public:
  NodeInfo() = default;

  struct ClearUnstableMapsOnCopy {
    const NodeInfo& val;
  };
  explicit NodeInfo(ClearUnstableMapsOnCopy other) V8_NOEXCEPT {
    type_ = other.val.type_;
    alternative_ = other.val.alternative_;
    if (other.val.possible_maps_are_known_ && !other.val.any_map_is_unstable_) {
      possible_maps_ = other.val.possible_maps_;
      possible_maps_are_known_ = true;
    }
  }

  NodeType type() const { return type_; }
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

#define ALTERNATIVES(V)                                \
  V(tagged, Tagged)                                    \
  V(int32, Int32)                                      \
  V(truncated_int32_to_number, TruncatedInt32ToNumber) \
  V(float64, Float64)                                  \
  V(checked_value, CheckedValue)

    enum Kind {
#define KIND(name, Name) k##Name,
      ALTERNATIVES(KIND)
#undef KIND
          kNumberOfAlternatives
    };

#define API(name, Name)                                      \
  ValueNode* name() const {                                  \
    if (!store_[Kind::k##Name]) return nullptr;              \
    return store_[Kind::k##Name]->UnwrapIdentities();        \
  }                                                          \
  ValueNode* set_##name(ValueNode* val) {                    \
    return store_[Kind::k##Name] = val;                      \
  }                                                          \
  template <typename Function>                               \
  ValueNode* get_or_set_##name(Function create) {            \
    if (store_[Kind::k##Name]) return store_[Kind::k##Name]; \
    return store_[Kind::k##Name] = create();                 \
  }
    ALTERNATIVES(API)
#undef API
#undef ALTERNATIVES

    bool has_none() const { return store_ == AlternativeNodes().store_; }

    void MergeWith(const AlternativeNodes& other) {
      for (size_t i = 0; i < Kind::kNumberOfAlternatives; ++i) {
        if (store_[i] && store_[i] != other.store_[i]) {
          store_[i] = nullptr;
        }
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
                 bool& any_merged_map_is_unstable) {
    UnionType(other.type_);
    alternative_.MergeWith(other.alternative_);
    if (possible_maps_are_known_) {
      if (other.possible_maps_are_known_) {
        // Map sets are the set of _possible_ maps, so on a merge we need to
        // _union_ them together (i.e. intersect the set of impossible maps).
        // Remember whether _any_ of these merges observed unstable maps.
        possible_maps_.Union(other.possible_maps_, zone);
      } else {
        possible_maps_.clear();
        possible_maps_are_known_ = false;
      }
    }

    any_map_is_unstable_ = possible_maps_are_known_ &&
                           (any_map_is_unstable_ || other.any_map_is_unstable_);
    any_merged_map_is_unstable =
        any_merged_map_is_unstable || any_map_is_unstable_;
  }

  bool possible_maps_are_unstable() const { return any_map_is_unstable_; }

  void ClearUnstableMaps() {
    if (!any_map_is_unstable_) return;
    possible_maps_.clear();
    possible_maps_are_known_ = false;
    type_ = MakeTypeStable(type_);
    any_map_is_unstable_ = false;
  }

  template <typename Function>
  void ClearUnstableMapsIfAny(const Function& condition) {
    if (!any_map_is_unstable_) return;
    for (auto map : possible_maps_) {
      if (condition(map)) {
        ClearUnstableMaps();
        return;
      }
    }
  }

  bool possible_maps_are_known() const { return possible_maps_are_known_; }

  const PossibleMaps& possible_maps() const {
    // If !possible_maps_are_known_ then every map is possible and using the
    // (probably empty) possible_maps_ set is definitely wrong.
    CHECK(possible_maps_are_known_);
    return possible_maps_;
  }

  void SetPossibleMaps(const PossibleMaps& possible_maps,
                       bool any_map_is_unstable, NodeType possible_type,
                       compiler::JSHeapBroker* broker) {
    possible_maps_ = possible_maps;
    possible_maps_are_known_ = true;
    any_map_is_unstable_ = any_map_is_unstable;
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
  }

  bool any_map_is_unstable() const { return any_map_is_unstable_; }

  void set_node_type_is_unstable() {
    // Reuse any_map_is_unstable to signal that the node type is unstable.
    any_map_is_unstable_ = true;
  }

 private:
  NodeType type_ = NodeType::kUnknown;

  bool any_map_is_unstable_ = false;

  // Maps for a node. Sets of maps that only contain stable maps are valid
  // across side-effecting calls, as long as we install a dependency, otherwise
  // they are cleared on side-effects.
  // TODO(v8:7700): Investigate a better data structure to use than ZoneMap.
  bool possible_maps_are_known_ = false;
  PossibleMaps possible_maps_;

  AlternativeNodes alternative_;
};

class KnownNodeAspects {
 public:
  // Permanently valid if checked in a dominator.
  using NodeInfos = ZoneMap<ValueNode*, NodeInfo>;

  // Copy constructor is defaulted but private so that we explicitly call the
  // Clone method.
  KnownNodeAspects& operator=(const KnownNodeAspects& other) = delete;
  KnownNodeAspects(KnownNodeAspects&& other) = delete;
  KnownNodeAspects& operator=(KnownNodeAspects&& other) = delete;

  KnownNodeAspects* Clone(Zone* zone) const {
    return zone->New<KnownNodeAspects>(*this);
  }

  // Loop headers can safely clone the node types, since those won't be
  // invalidated in the loop body, and similarly stable maps will have
  // dependencies installed. Unstable maps however might be invalidated by
  // calls, and we don't know about these until it's too late.
  KnownNodeAspects* CloneForLoopHeader(bool optimistic_initial_state,
                                       LoopEffects* loop_effects,
                                       Zone* zone) const;

  void ClearUnstableNodeAspects(bool is_tracing_enabled);

  void ClearUnstableMaps() {
    // A side effect could change existing objects' maps. For stable maps we
    // know this hasn't happened (because we added a dependency on the maps
    // staying stable and therefore not possible to transition away from), but
    // we can no longer assume that objects with unstable maps still have the
    // same map. Unstable maps can also transition to stable ones, so we have to
    // clear _all_ maps for a node if it had _any_ unstable map.
    if (!any_map_for_any_node_is_unstable_) return;
    for (auto& it : node_infos_) {
      it.second.ClearUnstableMaps();
    }
    any_map_for_any_node_is_unstable_ = false;
  }

  template <typename Function>
  void ClearUnstableMapsIfAny(const Function& condition) {
    if (!any_map_for_any_node_is_unstable_) return;
    for (auto& it : node_infos_) {
      it.second.ClearUnstableMapsIfAny(condition);
    }
  }

  void ClearAvailableExpressions() { available_expressions_.clear(); }

  void ClearAll() {
    loaded_constant_properties_.clear();
    loaded_properties_.clear();
    loaded_context_constants_.clear();
    loaded_context_slots_.clear();
    available_expressions_.clear();
    any_map_for_any_node_is_unstable_ = false;
    may_have_aliasing_contexts_ = ContextSlotLoadsAlias::kNone;
    node_infos_.clear();
    virtual_objects_ = {};
  }

  NodeInfos::iterator FindInfo(ValueNode* node) {
    return node_infos_.find(node);
  }
  NodeInfos::const_iterator FindInfo(ValueNode* node) const {
    return node_infos_.find(node);
  }
  bool IsValid(NodeInfos::iterator& it) { return it != node_infos_.end(); }
  bool IsValid(NodeInfos::const_iterator& it) const {
    return it != node_infos_.end();
  }

  const NodeInfo* TryGetInfoFor(ValueNode* node) const {
    return const_cast<KnownNodeAspects*>(this)->TryGetInfoFor(node);
  }

  NodeInfo* TryGetInfoFor(ValueNode* node) {
    auto info_it = FindInfo(node);
    if (!IsValid(info_it)) return nullptr;
    return &info_it->second;
  }

  NodeInfo* GetOrCreateInfoFor(compiler::JSHeapBroker* broker,
                               ValueNode* node) {
    auto info_it = FindInfo(node);
    if (IsValid(info_it)) return &info_it->second;
    auto res = &node_infos_.emplace(node, NodeInfo()).first->second;
    res->IntersectType(node->GetStaticType(broker));
    return res;
  }

  std::optional<PossibleMaps> TryGetPossibleMaps(ValueNode* node) {
    DCHECK_NOT_NULL(node);
    if (NodeInfo* info = TryGetInfoFor(node)) {
      if (info->possible_maps_are_known()) {
        return info->possible_maps();
      }
      return {};
    }
    return {};
  }

  NodeType GetType(compiler::JSHeapBroker* broker, ValueNode* node) const {
    // We first check the KnownNodeAspects in order to return the most precise
    // type possible.
    auto info = TryGetInfoFor(node);
    if (info == nullptr) {
      // If this node has no NodeInfo (or not known type in its NodeInfo), we
      // fall back to its static type.
      return node->GetStaticType(broker);
    }
    NodeType actual_type = info->type();
    if (auto phi = node->TryCast<Phi>()) {
      actual_type = IntersectType(actual_type, phi->type());
    }
    if (node->Is<ReturnedValue>()) {
      // The returned value might be more precise than the one stored in the
      // node info.
      actual_type =
          IntersectType(actual_type, GetType(broker, node->input_node(0)));
    }
#ifdef DEBUG
    NodeType static_type = node->GetStaticType(broker);
    if (!NodeTypeIs(actual_type, static_type)) {
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

  bool CheckType(compiler::JSHeapBroker* broker, ValueNode* node, NodeType type,
                 NodeType* current_type = nullptr) {
    NodeType static_type = node->GetStaticType(broker);
    if (current_type) *current_type = static_type;
    if (NodeTypeIs(static_type, type)) return true;
    if (IsEmptyNodeType(IntersectType(static_type, type))) return false;
    auto it = FindInfo(node);
    if (!IsValid(it)) return false;
    if (current_type) *current_type = it->second.type();
    return NodeTypeIs(it->second.type(), type);
  }

  NodeType CheckTypes(compiler::JSHeapBroker* broker, ValueNode* node,
                      std::initializer_list<NodeType> types) {
    auto it = FindInfo(node);
    bool has_kna = IsValid(it);
    for (NodeType type : types) {
      if (node->StaticTypeIs(broker, type)) return type;
      if (has_kna) {
        if (NodeTypeIs(it->second.type(), type)) return type;
      }
    }
    return NodeType::kUnknown;
  }

  bool MayBeNullOrUndefined(compiler::JSHeapBroker* broker, ValueNode* node) {
    NodeType static_type = node->GetStaticType(broker);
    if (!NodeTypeMayBeNullOrUndefined(static_type)) return false;
    auto it = FindInfo(node);
    if (!IsValid(it)) return true;
    return NodeTypeMayBeNullOrUndefined(it->second.type());
  }

  bool EnsureType(compiler::JSHeapBroker* broker, ValueNode* node,
                  NodeType type, NodeType* old_type = nullptr) {
    NodeType static_type = node->GetStaticType(broker);
    if (old_type) *old_type = static_type;
    if (NodeTypeIs(static_type, type)) return true;
    NodeInfo* known_info = GetOrCreateInfoFor(broker, node);
    if (old_type) *old_type = known_info->type();
    if (NodeTypeIs(known_info->type(), type)) return true;
    known_info->IntersectType(type);
    if (auto phi = node->TryCast<Phi>()) {
      known_info->IntersectType(phi->type());
    }
    if (NodeTypeIsUnstable(type)) {
      known_info->set_node_type_is_unstable();
      any_map_for_any_node_is_unstable_ = true;
    }
    return false;
  }

  template <typename Function>
  bool EnsureType(compiler::JSHeapBroker* broker, ValueNode* node,
                  NodeType type, Function ensure_new_type) {
    if (node->StaticTypeIs(broker, type)) return true;
    NodeInfo* known_info = GetOrCreateInfoFor(broker, node);
    if (NodeTypeIs(known_info->type(), type)) return true;
    ensure_new_type(known_info->type());
    known_info->IntersectType(type);
    if (NodeTypeIsUnstable(type)) {
      known_info->set_node_type_is_unstable();
      any_map_for_any_node_is_unstable_ = true;
    }
    return false;
  }

  // Returns true if we statically know that {lhs} and {rhs} have disjoint
  // types.
  bool HaveDisjointTypes(compiler::JSHeapBroker* broker, ValueNode* lhs,
                         ValueNode* rhs) {
    return HasDisjointType(broker, lhs, GetType(broker, rhs));
  }

  bool HasDisjointType(compiler::JSHeapBroker* broker, ValueNode* lhs,
                       NodeType rhs_type) {
    return IsEmptyNodeType(IntersectType(GetType(broker, lhs), rhs_type));
  }

  void Merge(const KnownNodeAspects& other, Zone* zone);

  // If IsCompatibleWithLoopHeader(other) returns true, it means that
  // Merge(other) would not remove any information from `this`.
  bool IsCompatibleWithLoopHeader(const KnownNodeAspects& other) const;

  // TODO(leszeks): Store these more efficiently than with std::map -- in
  // particular, clear out entries that are no longer reachable, perhaps also
  // allow lookup by interpreter register rather than by node pointer.

  void MarkAnyMapForAnyNodeIsUnstable() {
    any_map_for_any_node_is_unstable_ = true;
  }

  VirtualObjectList& virtual_objects() { return virtual_objects_; }
  const VirtualObjectList& virtual_objects() const { return virtual_objects_; }

  // Cached property loads.

  // Represents a key into the cache. This is either a NameRef, or an enum
  // value.
  class LoadedPropertyMapKey {
   public:
    enum Type {
      // kName must be zero so that pointers are unaffected.
      kName = 0,
      kElements,
      kTypedArrayLength,
      // TODO(leszeks): We could probably share kStringLength with
      // kTypedArrayLength if needed.
      kStringLength
    };
    static constexpr int kTypeMask = 0x3;
    static_assert((kName & ~kTypeMask) == 0);
    static_assert((kElements & ~kTypeMask) == 0);
    static_assert((kTypedArrayLength & ~kTypeMask) == 0);
    static_assert((kStringLength & ~kTypeMask) == 0);

    static LoadedPropertyMapKey Elements() {
      return LoadedPropertyMapKey(kElements);
    }

    static LoadedPropertyMapKey TypedArrayLength() {
      return LoadedPropertyMapKey(kTypedArrayLength);
    }

    static LoadedPropertyMapKey StringLength() {
      return LoadedPropertyMapKey(kStringLength);
    }

    // Allow implicit conversion from NameRef to key, so that callers in the
    // common path can use a NameRef directly.
    // NOLINTNEXTLINE
    LoadedPropertyMapKey(compiler::NameRef ref)
        : data_(reinterpret_cast<Address>(ref.data())) {
      DCHECK_EQ(data_ & kTypeMask, kName);
    }

    bool operator==(const LoadedPropertyMapKey& other) const {
      return data_ == other.data_;
    }
    bool operator<(const LoadedPropertyMapKey& other) const {
      return data_ < other.data_;
    }

    compiler::NameRef name() {
      DCHECK_EQ(type(), kName);
      return compiler::NameRef(reinterpret_cast<compiler::ObjectData*>(data_),
                               false);
    }

    Type type() { return static_cast<Type>(data_ & kTypeMask); }

   private:
    explicit LoadedPropertyMapKey(Type type) : data_(type) {
      DCHECK_NE(type, kName);
    }

    Address data_;
  };
  // Maps key->object->value, so that stores to a key can invalidate all loads
  // of that key (in case the objects are aliasing).
  using LoadedPropertyMap =
      ZoneMap<LoadedPropertyMapKey, ZoneMap<ValueNode*, ValueNode*>>;

  using LoadedContextSlotsKey = std::tuple<ValueNode*, int>;
  using LoadedContextSlots = ZoneMap<LoadedContextSlotsKey, ValueNode*>;

  ValueNode* TryFindLoadedProperty(ValueNode* lookup_start_object,
                                   LoadedPropertyMapKey name) {
    return TryFindLoadedProperty(loaded_properties_, lookup_start_object, name);
  }
  ValueNode* TryFindLoadedConstantProperty(ValueNode* lookup_start_object,
                                           LoadedPropertyMapKey name) {
    return TryFindLoadedProperty(loaded_constant_properties_,
                                 lookup_start_object, name);
  }

  ZoneMap<ValueNode*, ValueNode*>& GetLoadedPropertiesForKey(
      Zone* zone, bool is_const, KnownNodeAspects::LoadedPropertyMapKey key) {
    LoadedPropertyMap& properties =
        is_const ? loaded_constant_properties_ : loaded_properties_;
    // Try to get loaded_properties[key] if it already exists, otherwise
    // construct loaded_properties[key] = ZoneMap{zone()}.
    return properties.try_emplace(key, zone).first->second;
  }

  bool ClearLoadedPropertiesForKey(KnownNodeAspects::LoadedPropertyMapKey key) {
    auto it = loaded_properties_.find(
        KnownNodeAspects::LoadedPropertyMapKey::Elements());
    if (it != loaded_properties_.end()) {
      it->second.clear();
      return true;
    }
    return false;
  }

  void increment_effect_epoch() {
    if (effect_epoch_ < kEffectEpochOverflow) effect_epoch_++;
  }

  template <typename NodeT, typename... Args>
  NodeT* FindExpression(uint32_t hash,
                        std::array<ValueNode*, NodeT::kInputCount>& inputs,
                        Args&&... args) {
    auto it = available_expressions_.find(hash);
    if (it == available_expressions_.end()) return nullptr;

    static constexpr Opcode op = Node::opcode_of<NodeT>;
    auto candidate = it->second.node;

    if (candidate->Is<Identity>()) {
      // This expression was removed from the graph. Do not reuse it.
      available_expressions_.erase(it);
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
          if (inp != candidate->input(i).node()) {
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
      available_expressions_.erase(it);
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
    kNone,
    kOnlyLoadsRelativeToCurrentContext,
    kOnlyLoadsRelativeToConstant,
    kYes,
  };
  ContextSlotLoadsAlias may_have_aliasing_contexts() const {
    return may_have_aliasing_contexts_;
  }
  static ContextSlotLoadsAlias ContextSlotLoadsAliasMerge(
      ContextSlotLoadsAlias m1, ContextSlotLoadsAlias m2) {
    if (m1 == m2) return m1;
    if (m1 == ContextSlotLoadsAlias::kNone) return m2;
    if (m2 == ContextSlotLoadsAlias::kNone) return m1;
    return ContextSlotLoadsAlias::kYes;
  }
  void UpdateMayHaveAliasingContexts(compiler::JSHeapBroker* broker,
                                     LocalIsolate* local_isolate,
                                     ValueNode* context);

  LoadedContextSlots& loaded_context_slots() { return loaded_context_slots_; }

  ValueNode*& GetContextCachedValue(ValueNode* context, int offset,
                                    ContextSlotMutability slot_mutability) {
    ValueNode*& cached_value =
        slot_mutability == kMutable
            ? loaded_context_slots_[{context, offset}]
            : loaded_context_constants_[{context, offset}];
    if (cached_value) {
      cached_value = cached_value->UnwrapIdentities();
    }
    return cached_value;
  }
  bool HasContextCacheValue(ValueNode* context, int offset,
                            ContextSlotMutability slot_mutability) {
    return slot_mutability == kMutable
               ? loaded_context_slots_.contains({context, offset})
               : loaded_context_constants_.contains({context, offset});
  }
  bool IsContextCacheEmpty(ContextSlotMutability slot_mutability) {
    return slot_mutability == kMutable ? loaded_context_slots_.empty()
                                       : loaded_context_constants_.empty();
  }

  explicit KnownNodeAspects(Zone* zone)
      : loaded_constant_properties_(zone),
        loaded_properties_(zone),
        loaded_context_constants_(zone),
        loaded_context_slots_(zone),
        available_expressions_(zone),
        any_map_for_any_node_is_unstable_(false),
        may_have_aliasing_contexts_(ContextSlotLoadsAlias::kNone),
        effect_epoch_(0),
        node_infos_(zone),
        virtual_objects_() {}

 private:
  static constexpr uint32_t kEffectEpochForPureInstructions =
      std::numeric_limits<uint32_t>::max();
  static constexpr uint32_t kEffectEpochOverflow =
      kEffectEpochForPureInstructions - 1;

  struct AvailableExpression {
    NodeBase* node;
    uint32_t effect_epoch;
  };

  // Valid across side-effecting calls, as long as we install a dependency.
  LoadedPropertyMap loaded_constant_properties_;
  // Flushed after side-effecting calls.
  LoadedPropertyMap loaded_properties_;
  // Unconditionally valid across side-effecting calls.
  ZoneMap<std::tuple<ValueNode*, int>, ValueNode*> loaded_context_constants_;
  // Flushed after side-effecting calls.
  LoadedContextSlots loaded_context_slots_;
  // For CSE.
  ZoneMap<uint32_t, AvailableExpression> available_expressions_;
  bool any_map_for_any_node_is_unstable_;
  // This field indicates if the current state of loaded_context_slots might
  // contain contexts aliases. If that is the case, then we need to be more
  // conservative about updating the state on stores.
  ContextSlotLoadsAlias may_have_aliasing_contexts_;
  uint32_t effect_epoch_;
  NodeInfos node_infos_;
  VirtualObjectList virtual_objects_;

  friend KnownNodeAspects* Zone::New<KnownNodeAspects, const KnownNodeAspects&>(
      const KnownNodeAspects&);
  KnownNodeAspects(const KnownNodeAspects& other) V8_NOEXCEPT = default;
  // Copy constructor for CloneForLoopHeader
  friend KnownNodeAspects* Zone::New<KnownNodeAspects, const KnownNodeAspects&,
                                     bool&, LoopEffects*&, Zone*&>(
      const KnownNodeAspects&, bool&, maglev::LoopEffects*&, Zone*&);
  KnownNodeAspects(const KnownNodeAspects& other, bool optimistic_initial_state,
                   LoopEffects* loop_effects, Zone* zone);

  ValueNode* TryFindLoadedProperty(const LoadedPropertyMap& properties,
                                   ValueNode* lookup_start_object,
                                   LoadedPropertyMapKey name) {
    auto props_for_name = properties.find(name);
    if (props_for_name == properties.end()) return nullptr;

    auto it = props_for_name->second.find(lookup_start_object);
    if (it == props_for_name->second.end()) return nullptr;

    return it->second->UnwrapIdentities();
  }
};

class KnownMapsMerger {
 public:
  explicit KnownMapsMerger(compiler::JSHeapBroker* broker, Zone* zone,
                           base::Vector<const compiler::MapRef> requested_maps)
      : broker_(broker), zone_(zone), requested_maps_(requested_maps) {}

  void IntersectWithKnownNodeAspects(
      ValueNode* object, const KnownNodeAspects& known_node_aspects) {
    auto node_info_it = known_node_aspects.FindInfo(object);
    bool has_node_info = known_node_aspects.IsValid(node_info_it);
    NodeType type =
        has_node_info ? node_info_it->second.type() : NodeType::kUnknown;
    if (has_node_info && node_info_it->second.possible_maps_are_known()) {
      // TODO(v8:7700): Make intersection non-quadratic.
      for (compiler::MapRef possible_map :
           node_info_it->second.possible_maps()) {
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
          if (IsInstanceOfNodeType(possible_map, type, broker_)) {
            InsertMap(possible_map);
          }
        } else {
          known_maps_are_subset_of_requested_maps_ = false;
        }
      }
      if (intersect_set_.is_empty()) {
        node_type_ = EmptyNodeType();
      }
    } else {
      // A missing entry here means the universal set, i.e., we don't know
      // anything about the possible maps of the object. Intersect with the
      // universal set, which means just insert all requested maps.
      known_maps_are_subset_of_requested_maps_ = false;
      existing_known_maps_found_ = false;
      for (compiler::MapRef map : requested_maps_) {
        InsertMap(map);
      }
    }
  }

  void UpdateKnownNodeAspects(ValueNode* object,
                              KnownNodeAspects& known_node_aspects) {
    // Update known maps.
    auto node_info = known_node_aspects.GetOrCreateInfoFor(broker_, object);
    node_info->SetPossibleMaps(intersect_set_, any_map_is_unstable_, node_type_,
                               broker_);
    // Make sure known_node_aspects.any_map_for_any_node_is_unstable is updated
    // in case any_map_is_unstable changed to true for this object -- this can
    // happen if this was an intersection with the universal set which added new
    // possible unstable maps.
    if (any_map_is_unstable_) {
      known_node_aspects.MarkAnyMapForAnyNodeIsUnstable();
    }
    // At this point, known_node_aspects.any_map_for_any_node_is_unstable may be
    // true despite there no longer being any unstable maps for any nodes (if
    // this was the only node with unstable maps and this intersection removed
    // those). This is ok, because that's at worst just an overestimate -- we
    // could track whether this node's any_map_is_unstable flipped from true to
    // false, but this is likely overkill.
    // Insert stable map dependencies which weren't inserted yet. This is only
    // needed if our set of known maps was empty and we created it anew based on
    // maps we checked.
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
  }

  bool known_maps_are_subset_of_requested_maps() const {
    return known_maps_are_subset_of_requested_maps_;
  }
  bool emit_check_with_migration() const { return emit_check_with_migration_; }

  const compiler::ZoneRefSet<Map>& intersect_set() const {
    return intersect_set_;
  }

  NodeType node_type() const { return node_type_; }

 private:
  compiler::JSHeapBroker* broker_;
  Zone* zone_;
  base::Vector<const compiler::MapRef> requested_maps_;
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
