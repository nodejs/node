// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"

#ifdef ENABLE_SLOW_DCHECKS
#include <algorithm>
#endif

#include "src/codegen/code-factory.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/handles/handles-inl.h"
#include "src/ic/handler-configuration.h"
#include "src/init/bootstrapper.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(broker, x) TRACE_BROKER(broker, x)

// These definitions are here in order to please the linker, which in debug mode
// sometimes requires static constants to be defined in .cc files.
const uint32_t JSHeapBroker::kMinimalRefsBucketCount;
const uint32_t JSHeapBroker::kInitialRefsBucketCount;

void JSHeapBroker::IncrementTracingIndentation() { ++trace_indentation_; }

void JSHeapBroker::DecrementTracingIndentation() { --trace_indentation_; }

JSHeapBroker::JSHeapBroker(Isolate* isolate, Zone* broker_zone,
                           bool tracing_enabled, bool is_concurrent_inlining,
                           CodeKind code_kind)
    : isolate_(isolate),
      zone_(broker_zone),
      refs_(zone()->New<RefsMap>(kMinimalRefsBucketCount, AddressMatcher(),
                                 zone())),
      root_index_map_(isolate),
      array_and_object_prototypes_(zone()),
      tracing_enabled_(tracing_enabled),
      is_concurrent_inlining_(is_concurrent_inlining),
      is_isolate_bootstrapping_(isolate->bootstrapper()->IsActive()),
      code_kind_(code_kind),
      feedback_(zone()),
      property_access_infos_(zone()),
      minimorphic_property_access_infos_(zone()),
      typed_array_string_tags_(zone()),
      serialized_functions_(zone()) {
  // Note that this initialization of {refs_} with the minimal initial capacity
  // is redundant in the normal use case (concurrent compilation enabled,
  // standard objects to be serialized), as the map is going to be replaced
  // immediately with a larger-capacity one.  It doesn't seem to affect the
  // performance in a noticeable way though.
  TRACE(this, "Constructing heap broker");
}

JSHeapBroker::~JSHeapBroker() { DCHECK_NULL(local_isolate_); }

void JSHeapBroker::SetPersistentAndCopyCanonicalHandlesForTesting(
    std::unique_ptr<PersistentHandles> persistent_handles,
    std::unique_ptr<CanonicalHandlesMap> canonical_handles) {
  set_persistent_handles(std::move(persistent_handles));
  CopyCanonicalHandlesForTesting(std::move(canonical_handles));
}

void JSHeapBroker::CopyCanonicalHandlesForTesting(
    std::unique_ptr<CanonicalHandlesMap> canonical_handles) {
  DCHECK_NULL(canonical_handles_);
  canonical_handles_ = std::make_unique<CanonicalHandlesMap>(
      isolate_->heap(), ZoneAllocationPolicy(zone()));

  CanonicalHandlesMap::IteratableScope it_scope(canonical_handles.get());
  for (auto it = it_scope.begin(); it != it_scope.end(); ++it) {
    Address* entry = *it.entry();
    Object key = it.key();
    canonical_handles_->Insert(key, entry);
  }
}

std::string JSHeapBroker::Trace() const {
  std::ostringstream oss;
  oss << "[" << this << "] ";
  for (unsigned i = 0; i < trace_indentation_ * 2; ++i) oss.put(' ');
  return oss.str();
}

void JSHeapBroker::AttachLocalIsolate(OptimizedCompilationInfo* info,
                                      LocalIsolate* local_isolate) {
  set_canonical_handles(info->DetachCanonicalHandles());
  DCHECK_NULL(local_isolate_);
  local_isolate_ = local_isolate;
  DCHECK_NOT_NULL(local_isolate_);
  local_isolate_->heap()->AttachPersistentHandles(
      info->DetachPersistentHandles());

  if (is_concurrent_inlining()) {
    // Ensure any serialization that happens on the background has been
    // performed.
    target_native_context().SerializeOnBackground();
  }
}

void JSHeapBroker::DetachLocalIsolate(OptimizedCompilationInfo* info) {
  DCHECK_NULL(ph_);
  DCHECK_NOT_NULL(local_isolate_);
  std::unique_ptr<PersistentHandles> ph =
      local_isolate_->heap()->DetachPersistentHandles();
  local_isolate_ = nullptr;
  info->set_canonical_handles(DetachCanonicalHandles());
  info->set_persistent_handles(std::move(ph));
}

void JSHeapBroker::StopSerializing() {
  CHECK_EQ(mode_, kSerializing);
  TRACE(this, "Stopping serialization");
  mode_ = kSerialized;
}

void JSHeapBroker::Retire() {
  CHECK_EQ(mode_, kSerialized);
  TRACE(this, "Retiring");
  mode_ = kRetired;

#ifdef DEBUG
  PrintRefsAnalysis();
#endif  // DEBUG
}

void JSHeapBroker::SetTargetNativeContextRef(
    Handle<NativeContext> native_context) {
  DCHECK((mode() == kDisabled && !target_native_context_.has_value()) ||
         (mode() == kSerializing &&
          target_native_context_->object().is_identical_to(native_context) &&
          target_native_context_->is_unserialized_heap_object()));
  target_native_context_ = MakeRef(this, *native_context);
}

void JSHeapBroker::CollectArrayAndObjectPrototypes() {
  DisallowGarbageCollection no_gc;
  CHECK_EQ(mode(), kSerializing);
  CHECK(array_and_object_prototypes_.empty());

  Object maybe_context = isolate()->heap()->native_contexts_list();
  while (!maybe_context.IsUndefined(isolate())) {
    Context context = Context::cast(maybe_context);
    Object array_prot = context.get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
    Object object_prot = context.get(Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
    array_and_object_prototypes_.emplace(JSObject::cast(array_prot), isolate());
    array_and_object_prototypes_.emplace(JSObject::cast(object_prot),
                                         isolate());
    maybe_context = context.next_context_link();
  }

  CHECK(!array_and_object_prototypes_.empty());
}

StringRef JSHeapBroker::GetTypedArrayStringTag(ElementsKind kind) {
  DCHECK(IsTypedArrayElementsKind(kind));
  switch (kind) {
#define TYPED_ARRAY_STRING_TAG(Type, type, TYPE, ctype) \
  case ElementsKind::TYPE##_ELEMENTS:                   \
    return MakeRef(this, isolate()->factory()->Type##Array_string());
    TYPED_ARRAYS(TYPED_ARRAY_STRING_TAG)
#undef TYPED_ARRAY_STRING_TAG
    default:
      UNREACHABLE();
  }
}

bool JSHeapBroker::ShouldBeSerializedForCompilation(
    const SharedFunctionInfoRef& shared, const FeedbackVectorRef& feedback,
    const HintsVector& arguments) const {
  if (serialized_functions_.size() >= kMaxSerializedFunctionsCacheSize) {
    TRACE_BROKER_MISSING(this,
                         "opportunity - serialized functions cache is full.");
    return false;
  }
  SerializedFunction function{shared, feedback};
  auto matching_functions = serialized_functions_.equal_range(function);
  return std::find_if(matching_functions.first, matching_functions.second,
                      [&arguments](const auto& entry) {
                        return entry.second == arguments;
                      }) == matching_functions.second;
}

void JSHeapBroker::SetSerializedForCompilation(
    const SharedFunctionInfoRef& shared, const FeedbackVectorRef& feedback,
    const HintsVector& arguments) {
  SerializedFunction function{shared, feedback};
  serialized_functions_.insert({function, arguments});
  TRACE(this, "Set function " << shared << " with " << feedback
                              << " as serialized for compilation");
}

bool JSHeapBroker::IsSerializedForCompilation(
    const SharedFunctionInfoRef& shared,
    const FeedbackVectorRef& feedback) const {
  if (mode() == kDisabled) return true;

  SerializedFunction function = {shared, feedback};
  return serialized_functions_.find(function) != serialized_functions_.end();
}

bool JSHeapBroker::IsArrayOrObjectPrototype(const JSObjectRef& object) const {
  return IsArrayOrObjectPrototype(object.object());
}

bool JSHeapBroker::IsArrayOrObjectPrototype(Handle<JSObject> object) const {
  if (mode() == kDisabled) {
    return isolate()->IsInAnyContext(*object,
                                     Context::INITIAL_ARRAY_PROTOTYPE_INDEX) ||
           isolate()->IsInAnyContext(*object,
                                     Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
  }
  CHECK(!array_and_object_prototypes_.empty());
  return array_and_object_prototypes_.find(object) !=
         array_and_object_prototypes_.end();
}

ObjectData* JSHeapBroker::TryGetOrCreateData(Object object,
                                             GetOrCreateDataFlags flags) {
  return TryGetOrCreateData(CanonicalPersistentHandle(object), flags);
}

ObjectData* JSHeapBroker::GetOrCreateData(Handle<Object> object,
                                          GetOrCreateDataFlags flags) {
  ObjectData* return_value = TryGetOrCreateData(object, flags | kCrashOnError);
  DCHECK_NOT_NULL(return_value);
  return return_value;
}

ObjectData* JSHeapBroker::GetOrCreateData(Object object,
                                          GetOrCreateDataFlags flags) {
  return GetOrCreateData(CanonicalPersistentHandle(object), flags);
}

bool JSHeapBroker::StackHasOverflowed() const {
  DCHECK_IMPLIES(local_isolate_ == nullptr,
                 ThreadId::Current() == isolate_->thread_id());
  return (local_isolate_ != nullptr)
             ? StackLimitCheck::HasOverflowed(local_isolate_)
             : StackLimitCheck(isolate_).HasOverflowed();
}

bool JSHeapBroker::ObjectMayBeUninitialized(Handle<Object> object) const {
  if (!object->IsHeapObject()) return false;
  return ObjectMayBeUninitialized(HeapObject::cast(*object));
}

bool JSHeapBroker::ObjectMayBeUninitialized(HeapObject object) const {
  return !IsMainThread() && isolate()->heap()->IsPendingAllocation(object);
}

bool CanInlineElementAccess(MapRef const& map) {
  if (!map.IsJSObjectMap()) return false;
  if (map.is_access_check_needed()) return false;
  if (map.has_indexed_interceptor()) return false;
  ElementsKind const elements_kind = map.elements_kind();
  if (IsFastElementsKind(elements_kind)) return true;
  if (IsTypedArrayElementsKind(elements_kind) &&
      elements_kind != BIGUINT64_ELEMENTS &&
      elements_kind != BIGINT64_ELEMENTS) {
    return true;
  }
  return false;
}

ProcessedFeedback::ProcessedFeedback(Kind kind, FeedbackSlotKind slot_kind)
    : kind_(kind), slot_kind_(slot_kind) {}

KeyedAccessMode ElementAccessFeedback::keyed_mode() const {
  return keyed_mode_;
}

ZoneVector<ElementAccessFeedback::TransitionGroup> const&
ElementAccessFeedback::transition_groups() const {
  return transition_groups_;
}

ElementAccessFeedback const& ElementAccessFeedback::Refine(
    ZoneVector<Handle<Map>> const& inferred_maps, Zone* zone) const {
  ElementAccessFeedback& refined_feedback =
      *zone->New<ElementAccessFeedback>(zone, keyed_mode(), slot_kind());
  if (inferred_maps.empty()) return refined_feedback;

  ZoneUnorderedSet<Handle<Map>, Handle<Map>::hash, Handle<Map>::equal_to>
      inferred(zone);
  inferred.insert(inferred_maps.begin(), inferred_maps.end());

  for (auto const& group : transition_groups()) {
    DCHECK(!group.empty());
    TransitionGroup new_group(zone);
    for (size_t i = 1; i < group.size(); ++i) {
      Handle<Map> source = group[i];
      if (inferred.find(source) != inferred.end()) {
        new_group.push_back(source);
      }
    }

    Handle<Map> target = group.front();
    bool const keep_target =
        inferred.find(target) != inferred.end() || new_group.size() > 1;
    if (keep_target) {
      new_group.push_back(target);
      // The target must be at the front, the order of sources doesn't matter.
      std::swap(new_group[0], new_group[new_group.size() - 1]);
    }

    if (!new_group.empty()) {
      DCHECK(new_group.size() == 1 || new_group.front().equals(target));
      refined_feedback.transition_groups_.push_back(std::move(new_group));
    }
  }
  return refined_feedback;
}

InsufficientFeedback::InsufficientFeedback(FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kInsufficient, slot_kind) {}

GlobalAccessFeedback::GlobalAccessFeedback(PropertyCellRef cell,
                                           FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kGlobalAccess, slot_kind),
      cell_or_context_(cell),
      index_and_immutable_(0 /* doesn't matter */) {
  DCHECK(IsGlobalICKind(slot_kind));
}

GlobalAccessFeedback::GlobalAccessFeedback(FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kGlobalAccess, slot_kind),
      index_and_immutable_(0 /* doesn't matter */) {
  DCHECK(IsGlobalICKind(slot_kind));
}

GlobalAccessFeedback::GlobalAccessFeedback(ContextRef script_context,
                                           int slot_index, bool immutable,
                                           FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kGlobalAccess, slot_kind),
      cell_or_context_(script_context),
      index_and_immutable_(FeedbackNexus::SlotIndexBits::encode(slot_index) |
                           FeedbackNexus::ImmutabilityBit::encode(immutable)) {
  DCHECK_EQ(this->slot_index(), slot_index);
  DCHECK_EQ(this->immutable(), immutable);
  DCHECK(IsGlobalICKind(slot_kind));
}

bool GlobalAccessFeedback::IsMegamorphic() const {
  return !cell_or_context_.has_value();
}
bool GlobalAccessFeedback::IsPropertyCell() const {
  return cell_or_context_.has_value() && cell_or_context_->IsPropertyCell();
}
bool GlobalAccessFeedback::IsScriptContextSlot() const {
  return cell_or_context_.has_value() && cell_or_context_->IsContext();
}
PropertyCellRef GlobalAccessFeedback::property_cell() const {
  CHECK(IsPropertyCell());
  return cell_or_context_->AsPropertyCell();
}
ContextRef GlobalAccessFeedback::script_context() const {
  CHECK(IsScriptContextSlot());
  return cell_or_context_->AsContext();
}
int GlobalAccessFeedback::slot_index() const {
  DCHECK(IsScriptContextSlot());
  return FeedbackNexus::SlotIndexBits::decode(index_and_immutable_);
}
bool GlobalAccessFeedback::immutable() const {
  DCHECK(IsScriptContextSlot());
  return FeedbackNexus::ImmutabilityBit::decode(index_and_immutable_);
}

base::Optional<ObjectRef> GlobalAccessFeedback::GetConstantHint() const {
  if (IsPropertyCell()) {
    bool cell_serialized = property_cell().Serialize();
    CHECK(cell_serialized);  // Can't fail on the main thread.
    return property_cell().value();
  } else if (IsScriptContextSlot() && immutable()) {
    return script_context().get(slot_index());
  } else {
    return base::nullopt;
  }
}

KeyedAccessMode KeyedAccessMode::FromNexus(FeedbackNexus const& nexus) {
  FeedbackSlotKind kind = nexus.kind();
  if (IsKeyedLoadICKind(kind)) {
    return KeyedAccessMode(AccessMode::kLoad, nexus.GetKeyedAccessLoadMode());
  }
  if (IsKeyedHasICKind(kind)) {
    return KeyedAccessMode(AccessMode::kHas, nexus.GetKeyedAccessLoadMode());
  }
  if (IsKeyedStoreICKind(kind)) {
    return KeyedAccessMode(AccessMode::kStore, nexus.GetKeyedAccessStoreMode());
  }
  if (IsStoreInArrayLiteralICKind(kind) ||
      IsStoreDataPropertyInLiteralKind(kind)) {
    return KeyedAccessMode(AccessMode::kStoreInLiteral,
                           nexus.GetKeyedAccessStoreMode());
  }
  UNREACHABLE();
}

AccessMode KeyedAccessMode::access_mode() const { return access_mode_; }

bool KeyedAccessMode::IsLoad() const {
  return access_mode_ == AccessMode::kLoad || access_mode_ == AccessMode::kHas;
}
bool KeyedAccessMode::IsStore() const {
  return access_mode_ == AccessMode::kStore ||
         access_mode_ == AccessMode::kStoreInLiteral;
}

KeyedAccessLoadMode KeyedAccessMode::load_mode() const {
  CHECK(IsLoad());
  return load_store_mode_.load_mode;
}

KeyedAccessStoreMode KeyedAccessMode::store_mode() const {
  CHECK(IsStore());
  return load_store_mode_.store_mode;
}

KeyedAccessMode::LoadStoreMode::LoadStoreMode(KeyedAccessLoadMode load_mode)
    : load_mode(load_mode) {}
KeyedAccessMode::LoadStoreMode::LoadStoreMode(KeyedAccessStoreMode store_mode)
    : store_mode(store_mode) {}

KeyedAccessMode::KeyedAccessMode(AccessMode access_mode,
                                 KeyedAccessLoadMode load_mode)
    : access_mode_(access_mode), load_store_mode_(load_mode) {
  CHECK(!IsStore());
  CHECK(IsLoad());
}
KeyedAccessMode::KeyedAccessMode(AccessMode access_mode,
                                 KeyedAccessStoreMode store_mode)
    : access_mode_(access_mode), load_store_mode_(store_mode) {
  CHECK(!IsLoad());
  CHECK(IsStore());
}

ElementAccessFeedback::ElementAccessFeedback(Zone* zone,
                                             KeyedAccessMode const& keyed_mode,
                                             FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kElementAccess, slot_kind),
      keyed_mode_(keyed_mode),
      transition_groups_(zone) {
  DCHECK(IsKeyedLoadICKind(slot_kind) || IsKeyedHasICKind(slot_kind) ||
         IsStoreDataPropertyInLiteralKind(slot_kind) ||
         IsKeyedStoreICKind(slot_kind) ||
         IsStoreInArrayLiteralICKind(slot_kind));
}

bool ElementAccessFeedback::HasOnlyStringMaps(JSHeapBroker* broker) const {
  for (auto const& group : transition_groups()) {
    for (Handle<Map> map : group) {
      if (!MakeRef(broker, map).IsStringMap()) return false;
    }
  }
  return true;
}

MinimorphicLoadPropertyAccessFeedback::MinimorphicLoadPropertyAccessFeedback(
    NameRef const& name, FeedbackSlotKind slot_kind, Handle<Object> handler,
    ZoneVector<Handle<Map>> const& maps, bool has_migration_target_maps)
    : ProcessedFeedback(kMinimorphicPropertyAccess, slot_kind),
      name_(name),
      handler_(handler),
      maps_(maps),
      has_migration_target_maps_(has_migration_target_maps) {
  DCHECK(IsLoadICKind(slot_kind));
}

NamedAccessFeedback::NamedAccessFeedback(NameRef const& name,
                                         ZoneVector<Handle<Map>> const& maps,
                                         FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kNamedAccess, slot_kind), name_(name), maps_(maps) {
  DCHECK(IsLoadICKind(slot_kind) || IsStoreICKind(slot_kind) ||
         IsStoreOwnICKind(slot_kind) || IsKeyedLoadICKind(slot_kind) ||
         IsKeyedHasICKind(slot_kind) || IsKeyedStoreICKind(slot_kind) ||
         IsStoreInArrayLiteralICKind(slot_kind) ||
         IsStoreDataPropertyInLiteralKind(slot_kind));
}

void JSHeapBroker::SetFeedback(FeedbackSource const& source,
                               ProcessedFeedback const* feedback) {
  CHECK(source.IsValid());
  auto insertion = feedback_.insert({source, feedback});
  CHECK(insertion.second);
}

bool JSHeapBroker::HasFeedback(FeedbackSource const& source) const {
  DCHECK(source.IsValid());
  return feedback_.find(source) != feedback_.end();
}

ProcessedFeedback const& JSHeapBroker::GetFeedback(
    FeedbackSource const& source) const {
  DCHECK(source.IsValid());
  auto it = feedback_.find(source);
  CHECK_NE(it, feedback_.end());
  return *it->second;
}

FeedbackSlotKind JSHeapBroker::GetFeedbackSlotKind(
    FeedbackSource const& source) const {
  if (is_concurrent_inlining_) {
    ProcessedFeedback const& processed = GetFeedback(source);
    return processed.slot_kind();
  }
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  return nexus.kind();
}

bool JSHeapBroker::FeedbackIsInsufficient(FeedbackSource const& source) const {
  return is_concurrent_inlining_ ? GetFeedback(source).IsInsufficient()
                                 : FeedbackNexus(source.vector, source.slot,
                                                 feedback_nexus_config())
                                       .IsUninitialized();
}

namespace {

// Remove unupdatable and abandoned prototype maps in-place.
void FilterRelevantReceiverMaps(Isolate* isolate, MapHandles* maps) {
  auto in = maps->begin();
  auto out = in;
  auto end = maps->end();

  for (; in != end; ++in) {
    Handle<Map> map = *in;
    if (Map::TryUpdate(isolate, map).ToHandle(&map) &&
        !map->is_abandoned_prototype_map()) {
      DCHECK(!map->is_deprecated());
      *out = *in;
      ++out;
    }
  }

  // Remove everything between the last valid map and the end of the vector.
  maps->erase(out, end);
}

MaybeObjectHandle TryGetMinimorphicHandler(
    std::vector<MapAndHandler> const& maps_and_handlers, FeedbackSlotKind kind,
    Handle<NativeContext> native_context, bool is_turboprop) {
  if (!is_turboprop || !FLAG_turbo_dynamic_map_checks || !IsLoadICKind(kind)) {
    return MaybeObjectHandle();
  }

  // Don't use dynamic map checks when loading properties from Array.prototype.
  // Using dynamic map checks prevents constant folding and hence does not
  // inline the array builtins. We only care about monomorphic cases here. For
  // polymorphic loads currently we don't inline the builtins even without
  // dynamic map checks.
  if (maps_and_handlers.size() == 1 &&
      *maps_and_handlers[0].first ==
          native_context->initial_array_prototype().map()) {
    return MaybeObjectHandle();
  }

  MaybeObjectHandle initial_handler;
  for (MapAndHandler map_and_handler : maps_and_handlers) {
    auto map = map_and_handler.first;
    MaybeObjectHandle handler = map_and_handler.second;
    if (handler.is_null()) return MaybeObjectHandle();
    DCHECK(!handler->IsCleared());
    // TODO(mythria): extend this to DataHandlers too
    if (!handler.object()->IsSmi()) return MaybeObjectHandle();
    if (LoadHandler::GetHandlerKind(handler.object()->ToSmi()) !=
        LoadHandler::Kind::kField) {
      return MaybeObjectHandle();
    }
    CHECK(!map->IsJSGlobalProxyMap());
    if (initial_handler.is_null()) {
      initial_handler = handler;
    } else if (!handler.is_identical_to(initial_handler)) {
      return MaybeObjectHandle();
    }
  }
  return initial_handler;
}

bool HasMigrationTargets(const MapHandles& maps) {
  for (Handle<Map> map : maps) {
    if (map->is_migration_target()) return true;
  }
  return false;
}

}  // namespace

bool JSHeapBroker::CanUseFeedback(const FeedbackNexus& nexus) const {
  // TODO(jgruber,v8:8888): Currently, nci code does not use any
  // feedback. This restriction will be relaxed in the future.
  return !is_native_context_independent() && !nexus.IsUninitialized();
}

const ProcessedFeedback& JSHeapBroker::NewInsufficientFeedback(
    FeedbackSlotKind kind) const {
  return *zone()->New<InsufficientFeedback>(kind);
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  FeedbackSlotKind kind = nexus.kind();
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(kind);

  std::vector<MapAndHandler> maps_and_handlers;
  nexus.ExtractMapsAndFeedback(&maps_and_handlers);
  MapHandles maps;
  for (auto const& entry : maps_and_handlers) {
    maps.push_back(entry.first);
  }

  base::Optional<NameRef> name =
      static_name.has_value() ? static_name : GetNameFeedback(nexus);
  MaybeObjectHandle handler = TryGetMinimorphicHandler(
      maps_and_handlers, kind, target_native_context().object(),
      is_turboprop());
  if (!handler.is_null()) {
    return *zone()->New<MinimorphicLoadPropertyAccessFeedback>(
        *name, kind, handler.object(),
        ZoneVector<Handle<Map>>(maps.begin(), maps.end(), zone()),
        HasMigrationTargets(maps));
  }

  FilterRelevantReceiverMaps(isolate(), &maps);

  // If no maps were found for a non-megamorphic access, then our maps died
  // and we should soft-deopt.
  if (maps.empty() && nexus.ic_state() != MEGAMORPHIC) {
    return NewInsufficientFeedback(kind);
  }

  if (name.has_value()) {
    // We rely on this invariant in JSGenericLowering.
    DCHECK_IMPLIES(maps.empty(), nexus.ic_state() == MEGAMORPHIC);
    return *zone()->New<NamedAccessFeedback>(
        *name, ZoneVector<Handle<Map>>(maps.begin(), maps.end(), zone()), kind);
  } else if (nexus.GetKeyType() == ELEMENT && !maps.empty()) {
    return ProcessFeedbackMapsForElementAccess(
        maps, KeyedAccessMode::FromNexus(nexus), kind);
  } else {
    // No actionable feedback.
    DCHECK(maps.empty());
    DCHECK_EQ(nexus.ic_state(), MEGAMORPHIC);
    // TODO(neis): Using ElementAccessFeedback here is kind of an abuse.
    return *zone()->New<ElementAccessFeedback>(
        zone(), KeyedAccessMode::FromNexus(nexus), kind);
  }
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  DCHECK(nexus.kind() == FeedbackSlotKind::kLoadGlobalInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kLoadGlobalNotInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalSloppy ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalStrict);
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  if (nexus.ic_state() != MONOMORPHIC || nexus.GetFeedback()->IsCleared()) {
    return *zone()->New<GlobalAccessFeedback>(nexus.kind());
  }

  Handle<Object> feedback_value(nexus.GetFeedback()->GetHeapObjectOrSmi(),
                                isolate());

  if (feedback_value->IsSmi()) {
    // The wanted name belongs to a script-scope variable and the feedback
    // tells us where to find its value.
    int number = feedback_value->Number();
    int const script_context_index =
        FeedbackNexus::ContextIndexBits::decode(number);
    int const context_slot_index = FeedbackNexus::SlotIndexBits::decode(number);
    bool const immutable = FeedbackNexus::ImmutabilityBit::decode(number);
    Handle<Context> context = ScriptContextTable::GetContext(
        isolate(), target_native_context().script_context_table().object(),
        script_context_index);
    {
      ObjectRef contents =
          MakeRef(this, handle(context->get(context_slot_index), isolate()));
      CHECK(!contents.equals(
          MakeRef<Object>(this, isolate()->factory()->the_hole_value())));
    }
    ContextRef context_ref = MakeRef(this, context);
    if (immutable) {
      context_ref.get(context_slot_index,
                      SerializationPolicy::kSerializeIfNeeded);
    }
    return *zone()->New<GlobalAccessFeedback>(context_ref, context_slot_index,
                                              immutable, nexus.kind());
  }

  CHECK(feedback_value->IsPropertyCell());
  // The wanted name belongs (or did belong) to a property on the global
  // object and the feedback is the cell holding its value.
  PropertyCellRef cell =
      MakeRef(this, Handle<PropertyCell>::cast(feedback_value));
  MakeRef(this,
          Handle<PropertyCell>::cast(feedback_value)->value(kAcquireLoad));
  return *zone()->New<GlobalAccessFeedback>(cell, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForBinaryOperation(
    FeedbackSource const& source) const {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  BinaryOperationHint hint = nexus.GetBinaryOperationFeedback();
  DCHECK_NE(hint, BinaryOperationHint::kNone);  // Not uninitialized.
  return *zone()->New<BinaryOperationFeedback>(hint, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForCompareOperation(
    FeedbackSource const& source) const {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  CompareOperationHint hint = nexus.GetCompareOperationFeedback();
  DCHECK_NE(hint, CompareOperationHint::kNone);  // Not uninitialized.
  return *zone()->New<CompareOperationFeedback>(hint, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForForIn(
    FeedbackSource const& source) const {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  ForInHint hint = nexus.GetForInFeedback();
  DCHECK_NE(hint, ForInHint::kNone);  // Not uninitialized.
  return *zone()->New<ForInFeedback>(hint, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForInstanceOf(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  base::Optional<JSObjectRef> optional_constructor;
  {
    MaybeHandle<JSObject> maybe_constructor = nexus.GetConstructorFeedback();
    Handle<JSObject> constructor;
    if (maybe_constructor.ToHandle(&constructor)) {
      optional_constructor = MakeRef(this, constructor);
    }
  }
  return *zone()->New<InstanceOfFeedback>(optional_constructor, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  HeapObject object;
  if (!nexus.GetFeedback()->GetHeapObject(&object)) {
    return NewInsufficientFeedback(nexus.kind());
  }

  AllocationSiteRef site =
      MakeRef(this, handle(AllocationSite::cast(object), isolate()));
  if (site.IsFastLiteral()) {
    site.SerializeBoilerplate();
  }

  return *zone()->New<LiteralFeedback>(site, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  HeapObject object;
  if (!nexus.GetFeedback()->GetHeapObject(&object)) {
    return NewInsufficientFeedback(nexus.kind());
  }

  RegExpBoilerplateDescriptionRef boilerplate = MakeRef(
      this, handle(RegExpBoilerplateDescription::cast(object), isolate()));
  boilerplate.Serialize();
  return *zone()->New<RegExpLiteralFeedback>(boilerplate, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForTemplateObject(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  HeapObject object;
  if (!nexus.GetFeedback()->GetHeapObject(&object)) {
    return NewInsufficientFeedback(nexus.kind());
  }

  JSArrayRef array = MakeRef(this, handle(JSArray::cast(object), isolate()));
  return *zone()->New<TemplateObjectFeedback>(array, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForCall(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  base::Optional<HeapObjectRef> target_ref;
  {
    // TODO(mvstanton): this read has a special danger when done on the
    // background thread, because the CallIC has a site in generated code
    // where a JSFunction is installed in this slot without store ordering.
    // Therefore, we will need to check {maybe_target} to ensure that it
    // has been store ordered by the heap's mechanism for store-ordering
    // batches of new objects.
    MaybeObject maybe_target = nexus.GetFeedback();
    HeapObject target_object;
    if (maybe_target->GetHeapObject(&target_object)) {
      target_ref = MakeRef(this, handle(target_object, isolate()));
    }
  }
  float frequency = nexus.ComputeCallFrequency();
  SpeculationMode mode = nexus.GetSpeculationMode();
  CallFeedbackContent content = nexus.GetCallFeedbackContent();
  return *zone()->New<CallFeedback>(target_ref, frequency, mode, content,
                                    nexus.kind());
}

BinaryOperationHint JSHeapBroker::GetFeedbackForBinaryOperation(
    FeedbackSource const& source) {
  ProcessedFeedback const& feedback =
      is_concurrent_inlining_ ? GetFeedback(source)
                              : ProcessFeedbackForBinaryOperation(source);
  return feedback.IsInsufficient() ? BinaryOperationHint::kNone
                                   : feedback.AsBinaryOperation().value();
}

CompareOperationHint JSHeapBroker::GetFeedbackForCompareOperation(
    FeedbackSource const& source) {
  ProcessedFeedback const& feedback =
      is_concurrent_inlining_ ? GetFeedback(source)
                              : ProcessFeedbackForCompareOperation(source);
  return feedback.IsInsufficient() ? CompareOperationHint::kNone
                                   : feedback.AsCompareOperation().value();
}

ForInHint JSHeapBroker::GetFeedbackForForIn(FeedbackSource const& source) {
  ProcessedFeedback const& feedback = is_concurrent_inlining_
                                          ? GetFeedback(source)
                                          : ProcessFeedbackForForIn(source);
  return feedback.IsInsufficient() ? ForInHint::kNone
                                   : feedback.AsForIn().value();
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  return is_concurrent_inlining_
             ? GetFeedback(source)
             : ProcessFeedbackForPropertyAccess(source, mode, static_name);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForInstanceOf(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForInstanceOf(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForCall(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForCall(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForGlobalAccess(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  return is_concurrent_inlining_
             ? GetFeedback(source)
             : ProcessFeedbackForArrayOrObjectLiteral(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForRegExpLiteral(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForTemplateObject(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForTemplateObject(source);
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback =
      ReadFeedbackForArrayOrObjectLiteral(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForRegExpLiteral(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForTemplateObject(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForTemplateObject(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForBinaryOperation(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForBinaryOperation(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForCompareOperation(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForCompareOperation(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForForIn(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForForIn(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback =
      ReadFeedbackForPropertyAccess(source, mode, static_name);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForInstanceOf(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForInstanceOf(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForCall(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForCall(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForGlobalAccess(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ElementAccessFeedback const& JSHeapBroker::ProcessFeedbackMapsForElementAccess(
    MapHandles const& maps, KeyedAccessMode const& keyed_mode,
    FeedbackSlotKind slot_kind) {
  DCHECK(!maps.empty());

  // Collect possible transition targets.
  MapHandles possible_transition_targets;
  possible_transition_targets.reserve(maps.size());
  for (Handle<Map> map : maps) {
    MapRef map_ref = MakeRef(this, map);
    map_ref.SerializeRootMap();

    if (CanInlineElementAccess(map_ref) &&
        IsFastElementsKind(map->elements_kind()) &&
        GetInitialFastElementsKind() != map->elements_kind()) {
      possible_transition_targets.push_back(map);
    }
  }

  using TransitionGroup = ElementAccessFeedback::TransitionGroup;
  ZoneUnorderedMap<Handle<Map>, TransitionGroup, Handle<Map>::hash,
                   Handle<Map>::equal_to>
      transition_groups(zone());

  // Separate the actual receiver maps and the possible transition sources.
  for (Handle<Map> map : maps) {
    // Don't generate elements kind transitions from stable maps.
    Map transition_target = map->is_stable()
                                ? Map()
                                : map->FindElementsKindTransitionedMap(
                                      isolate(), possible_transition_targets);
    if (transition_target.is_null()) {
      TransitionGroup group(1, map, zone());
      transition_groups.insert({map, group});
    } else {
      Handle<Map> target(transition_target, isolate());
      TransitionGroup new_group(1, target, zone());
      TransitionGroup& actual_group =
          transition_groups.insert({target, new_group}).first->second;
      actual_group.push_back(map);
    }
  }

  ElementAccessFeedback* result =
      zone()->New<ElementAccessFeedback>(zone(), keyed_mode, slot_kind);
  for (auto entry : transition_groups) {
    result->AddGroup(std::move(entry.second));
  }

  CHECK(!result->transition_groups().empty());
  return *result;
}

void ElementAccessFeedback::AddGroup(TransitionGroup&& group) {
  CHECK(!group.empty());
  transition_groups_.push_back(std::move(group));

#ifdef ENABLE_SLOW_DCHECKS
  // Check that each of the group's maps occurs exactly once in the whole
  // feedback. This implies that "a source is not a target".
  for (Handle<Map> map : group) {
    int count = 0;
    for (TransitionGroup const& some_group : transition_groups()) {
      count += std::count_if(
          some_group.begin(), some_group.end(),
          [&](Handle<Map> some_map) { return some_map.equals(map); });
    }
    CHECK_EQ(count, 1);
  }
#endif
}

base::Optional<NameRef> JSHeapBroker::GetNameFeedback(
    FeedbackNexus const& nexus) {
  Name raw_name = nexus.GetName();
  if (raw_name.is_null()) return base::nullopt;
  return MakeRef(this, handle(raw_name, isolate()));
}

PropertyAccessInfo JSHeapBroker::GetPropertyAccessInfo(
    MapRef map, NameRef name, AccessMode access_mode,
    CompilationDependencies* dependencies, SerializationPolicy policy) {
  PropertyAccessTarget target({map, name, access_mode});
  auto it = property_access_infos_.find(target);
  if (it != property_access_infos_.end()) return it->second;

  if (policy == SerializationPolicy::kAssumeSerialized &&
      !FLAG_turbo_concurrent_get_property_access_info) {
    TRACE_BROKER_MISSING(this, "PropertyAccessInfo for "
                                   << access_mode << " of property " << name
                                   << " on map " << map);
    return PropertyAccessInfo::Invalid(zone());
  }

  CHECK_NOT_NULL(dependencies);
  AccessInfoFactory factory(this, dependencies, zone());
  PropertyAccessInfo access_info = factory.ComputePropertyAccessInfo(
      map.object(), name.object(), access_mode);
  if (is_concurrent_inlining_) {
    CHECK_IMPLIES(!FLAG_turbo_concurrent_get_property_access_info,
                  mode() == kSerializing);
    TRACE(this, "Storing PropertyAccessInfo for "
                    << access_mode << " of property " << name << " on map "
                    << map);
    property_access_infos_.insert({target, access_info});
  }
  return access_info;
}

MinimorphicLoadPropertyAccessInfo JSHeapBroker::GetPropertyAccessInfo(
    MinimorphicLoadPropertyAccessFeedback const& feedback,
    FeedbackSource const& source, SerializationPolicy policy) {
  auto it = minimorphic_property_access_infos_.find(source);
  if (it != minimorphic_property_access_infos_.end()) return it->second;

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_BROKER_MISSING(this, "MinimorphicLoadPropertyAccessInfo for slot "
                                   << source.index() << "  "
                                   << ObjectRef(this, source.vector));
    return MinimorphicLoadPropertyAccessInfo::Invalid();
  }

  AccessInfoFactory factory(this, nullptr, zone());
  MinimorphicLoadPropertyAccessInfo access_info =
      factory.ComputePropertyAccessInfo(feedback);
  if (is_concurrent_inlining_) {
    TRACE(this, "Storing MinimorphicLoadPropertyAccessInfo for "
                    << source.index() << "  "
                    << ObjectRef(this, source.vector));
    minimorphic_property_access_infos_.insert({source, access_info});
  }
  return access_info;
}

BinaryOperationFeedback const& ProcessedFeedback::AsBinaryOperation() const {
  CHECK_EQ(kBinaryOperation, kind());
  return *static_cast<BinaryOperationFeedback const*>(this);
}

CallFeedback const& ProcessedFeedback::AsCall() const {
  CHECK_EQ(kCall, kind());
  return *static_cast<CallFeedback const*>(this);
}

CompareOperationFeedback const& ProcessedFeedback::AsCompareOperation() const {
  CHECK_EQ(kCompareOperation, kind());
  return *static_cast<CompareOperationFeedback const*>(this);
}

ElementAccessFeedback const& ProcessedFeedback::AsElementAccess() const {
  CHECK_EQ(kElementAccess, kind());
  return *static_cast<ElementAccessFeedback const*>(this);
}

ForInFeedback const& ProcessedFeedback::AsForIn() const {
  CHECK_EQ(kForIn, kind());
  return *static_cast<ForInFeedback const*>(this);
}

GlobalAccessFeedback const& ProcessedFeedback::AsGlobalAccess() const {
  CHECK_EQ(kGlobalAccess, kind());
  return *static_cast<GlobalAccessFeedback const*>(this);
}

InstanceOfFeedback const& ProcessedFeedback::AsInstanceOf() const {
  CHECK_EQ(kInstanceOf, kind());
  return *static_cast<InstanceOfFeedback const*>(this);
}

NamedAccessFeedback const& ProcessedFeedback::AsNamedAccess() const {
  CHECK_EQ(kNamedAccess, kind());
  return *static_cast<NamedAccessFeedback const*>(this);
}

MinimorphicLoadPropertyAccessFeedback const&
ProcessedFeedback::AsMinimorphicPropertyAccess() const {
  CHECK_EQ(kMinimorphicPropertyAccess, kind());
  return *static_cast<MinimorphicLoadPropertyAccessFeedback const*>(this);
}

LiteralFeedback const& ProcessedFeedback::AsLiteral() const {
  CHECK_EQ(kLiteral, kind());
  return *static_cast<LiteralFeedback const*>(this);
}

RegExpLiteralFeedback const& ProcessedFeedback::AsRegExpLiteral() const {
  CHECK_EQ(kRegExpLiteral, kind());
  return *static_cast<RegExpLiteralFeedback const*>(this);
}

TemplateObjectFeedback const& ProcessedFeedback::AsTemplateObject() const {
  CHECK_EQ(kTemplateObject, kind());
  return *static_cast<TemplateObjectFeedback const*>(this);
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
