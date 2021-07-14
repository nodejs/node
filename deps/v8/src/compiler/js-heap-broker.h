// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_HEAP_BROKER_H_
#define V8_COMPILER_JS_HEAP_BROKER_H_

#include "src/base/compiler-specific.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/compiler/access-info.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/globals.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/processed-feedback.h"
#include "src/compiler/refs-map.h"
#include "src/compiler/serializer-hints.h"
#include "src/execution/local-isolate.h"
#include "src/handles/handles.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects/code-kind.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/function-kind.h"
#include "src/objects/objects.h"
#include "src/utils/address-map.h"
#include "src/utils/identity-map.h"
#include "src/utils/ostreams.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class ObjectRef;

std::ostream& operator<<(std::ostream& os, const ObjectRef& ref);

#define TRACE_BROKER(broker, x)                                      \
  do {                                                               \
    if (broker->tracing_enabled() && FLAG_trace_heap_broker_verbose) \
      StdoutStream{} << broker->Trace() << x << '\n';                \
  } while (false)

#define TRACE_BROKER_MEMORY(broker, x)                              \
  do {                                                              \
    if (broker->tracing_enabled() && FLAG_trace_heap_broker_memory) \
      StdoutStream{} << broker->Trace() << x << std::endl;          \
  } while (false)

#define TRACE_BROKER_MISSING(broker, x)                                        \
  do {                                                                         \
    if (broker->tracing_enabled())                                             \
      StdoutStream{} << broker->Trace() << "Missing " << x << " (" << __FILE__ \
                     << ":" << __LINE__ << ")" << std::endl;                   \
  } while (false)

struct PropertyAccessTarget {
  MapRef map;
  NameRef name;
  AccessMode mode;

  struct Hash {
    size_t operator()(const PropertyAccessTarget& pair) const {
      return base::hash_combine(
          base::hash_combine(pair.map.object().address(),
                             pair.name.object().address()),
          static_cast<int>(pair.mode));
    }
  };
  struct Equal {
    bool operator()(const PropertyAccessTarget& lhs,
                    const PropertyAccessTarget& rhs) const {
      return lhs.map.equals(rhs.map) && lhs.name.equals(rhs.name) &&
             lhs.mode == rhs.mode;
    }
  };
};

enum GetOrCreateDataFlag {
  // If set, a failure to create the data object results in a crash.
  kCrashOnError = 1 << 0,
  // If set, data construction assumes that the given object is protected by
  // a memory fence (e.g. acquire-release) and thus fields required for
  // construction (like Object::map) are safe to read. The protection can
  // extend to some other situations as well.
  kAssumeMemoryFence = 1 << 1,
};
using GetOrCreateDataFlags = base::Flags<GetOrCreateDataFlag>;
DEFINE_OPERATORS_FOR_FLAGS(GetOrCreateDataFlags)

class V8_EXPORT_PRIVATE JSHeapBroker {
 public:
  JSHeapBroker(Isolate* isolate, Zone* broker_zone, bool tracing_enabled,
               bool is_concurrent_inlining, CodeKind code_kind);

  // For use only in tests, sets default values for some arguments. Avoids
  // churn when new flags are added.
  JSHeapBroker(Isolate* isolate, Zone* broker_zone)
      : JSHeapBroker(isolate, broker_zone, FLAG_trace_heap_broker, false,
                     CodeKind::TURBOFAN) {}

  ~JSHeapBroker();

  // The compilation target's native context. We need the setter because at
  // broker construction time we don't yet have the canonical handle.
  NativeContextRef target_native_context() const {
    return target_native_context_.value();
  }
  void SetTargetNativeContextRef(Handle<NativeContext> native_context);

  void InitializeAndStartSerializing();

  Isolate* isolate() const { return isolate_; }
  Zone* zone() const { return zone_; }
  bool tracing_enabled() const { return tracing_enabled_; }
  bool is_concurrent_inlining() const { return is_concurrent_inlining_; }
  bool is_isolate_bootstrapping() const { return is_isolate_bootstrapping_; }
  bool is_native_context_independent() const {
    // TODO(jgruber,v8:8888): Remove dependent code.
    return false;
  }
  bool generate_full_feedback_collection() const {
    // NCI code currently collects full feedback.
    DCHECK_IMPLIES(is_native_context_independent(),
                   CollectFeedbackInGenericLowering());
    return is_native_context_independent();
  }
  bool is_turboprop() const { return code_kind_ == CodeKind::TURBOPROP; }

  NexusConfig feedback_nexus_config() const {
    // TODO(mvstanton): when the broker gathers feedback on the background
    // thread, this should return a local NexusConfig object which points
    // to the associated LocalHeap.
    return NexusConfig::FromMainThread(isolate());
  }

  enum BrokerMode { kDisabled, kSerializing, kSerialized, kRetired };
  BrokerMode mode() const { return mode_; }

  void StopSerializing();
  void Retire();
  bool SerializingAllowed() const;

  // Remember the local isolate and initialize its local heap with the
  // persistent and canonical handles provided by {info}.
  void AttachLocalIsolate(OptimizedCompilationInfo* info,
                          LocalIsolate* local_isolate);
  // Forget about the local isolate and pass the persistent and canonical
  // handles provided back to {info}. {info} is responsible for disposing of
  // them.
  void DetachLocalIsolate(OptimizedCompilationInfo* info);

  bool StackHasOverflowed() const;

#ifdef DEBUG
  void PrintRefsAnalysis() const;
#endif  // DEBUG

  // Returns the handle from root index table for read only heap objects.
  Handle<Object> GetRootHandle(Object object);

  // Never returns nullptr.
  ObjectData* GetOrCreateData(Handle<Object> object,
                              GetOrCreateDataFlags flags = {});
  ObjectData* GetOrCreateData(Object object, GetOrCreateDataFlags flags = {});

  // Gets data only if we have it. However, thin wrappers will be created for
  // smis, read-only objects and never-serialized objects.
  ObjectData* TryGetOrCreateData(Handle<Object> object,
                                 GetOrCreateDataFlags flags = {});
  ObjectData* TryGetOrCreateData(Object object,
                                 GetOrCreateDataFlags flags = {});

  // Check if {object} is any native context's %ArrayPrototype% or
  // %ObjectPrototype%.
  bool IsArrayOrObjectPrototype(const JSObjectRef& object) const;
  bool IsArrayOrObjectPrototype(Handle<JSObject> object) const;

  bool HasFeedback(FeedbackSource const& source) const;
  void SetFeedback(FeedbackSource const& source,
                   ProcessedFeedback const* feedback);
  ProcessedFeedback const& GetFeedback(FeedbackSource const& source) const;
  FeedbackSlotKind GetFeedbackSlotKind(FeedbackSource const& source) const;

  // TODO(neis): Move these into serializer when we're always in the background.
  ElementAccessFeedback const& ProcessFeedbackMapsForElementAccess(
      MapHandles const& maps, KeyedAccessMode const& keyed_mode,
      FeedbackSlotKind slot_kind);

  // Binary, comparison and for-in hints can be fully expressed via
  // an enum. Insufficient feedback is signaled by <Hint enum>::kNone.
  BinaryOperationHint GetFeedbackForBinaryOperation(
      FeedbackSource const& source);
  CompareOperationHint GetFeedbackForCompareOperation(
      FeedbackSource const& source);
  ForInHint GetFeedbackForForIn(FeedbackSource const& source);

  ProcessedFeedback const& GetFeedbackForCall(FeedbackSource const& source);
  ProcessedFeedback const& GetFeedbackForGlobalAccess(
      FeedbackSource const& source);
  ProcessedFeedback const& GetFeedbackForInstanceOf(
      FeedbackSource const& source);
  ProcessedFeedback const& GetFeedbackForArrayOrObjectLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& GetFeedbackForRegExpLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& GetFeedbackForTemplateObject(
      FeedbackSource const& source);
  ProcessedFeedback const& GetFeedbackForPropertyAccess(
      FeedbackSource const& source, AccessMode mode,
      base::Optional<NameRef> static_name);

  ProcessedFeedback const& ProcessFeedbackForBinaryOperation(
      FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForCall(FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForCompareOperation(
      FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForForIn(
      FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForGlobalAccess(
      FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForInstanceOf(
      FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForPropertyAccess(
      FeedbackSource const& source, AccessMode mode,
      base::Optional<NameRef> static_name);
  ProcessedFeedback const& ProcessFeedbackForArrayOrObjectLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForRegExpLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& ProcessFeedbackForTemplateObject(
      FeedbackSource const& source);

  bool FeedbackIsInsufficient(FeedbackSource const& source) const;

  base::Optional<NameRef> GetNameFeedback(FeedbackNexus const& nexus);

  // If {policy} is {kAssumeSerialized} and the broker doesn't know about the
  // combination of {map}, {name}, and {access_mode}, returns Invalid.
  PropertyAccessInfo GetPropertyAccessInfo(
      MapRef map, NameRef name, AccessMode access_mode,
      CompilationDependencies* dependencies = nullptr,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

  MinimorphicLoadPropertyAccessInfo GetPropertyAccessInfo(
      MinimorphicLoadPropertyAccessFeedback const& feedback,
      FeedbackSource const& source,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

  // Used to separate the problem of a concurrent GetPropertyAccessInfo (GPAI)
  // from serialization. GPAI is currently called both during the serialization
  // phase, and on the background thread. While some crucial objects (like
  // JSObject) still must be serialized, we do the following:
  // - Run GPAI during serialization to discover and serialize required objects.
  // - After the serialization phase, clear cached property access infos.
  // - On the background thread, rerun GPAI in a concurrent setting. The cache
  //   has been cleared, thus the actual logic runs again.
  // Once all required object kinds no longer require serialization, this
  // should be removed together with all GPAI calls during serialization.
  void ClearCachedPropertyAccessInfos() {
    CHECK(FLAG_turbo_concurrent_get_property_access_info);
    property_access_infos_.clear();
  }

  // As above, clear cached ObjectData that can be reconstructed, i.e. is
  // either never-serialized or background-serialized.
  void ClearReconstructibleData();

  StringRef GetTypedArrayStringTag(ElementsKind kind);

  bool ShouldBeSerializedForCompilation(const SharedFunctionInfoRef& shared,
                                        const FeedbackVectorRef& feedback,
                                        const HintsVector& arguments) const;
  void SetSerializedForCompilation(const SharedFunctionInfoRef& shared,
                                   const FeedbackVectorRef& feedback,
                                   const HintsVector& arguments);
  bool IsSerializedForCompilation(const SharedFunctionInfoRef& shared,
                                  const FeedbackVectorRef& feedback) const;

  bool IsMainThread() const {
    return local_isolate() == nullptr || local_isolate()->is_main_thread();
  }

  LocalIsolate* local_isolate() const { return local_isolate_; }

  // TODO(jgruber): Consider always having local_isolate_ set to a real value.
  // This seems not entirely trivial since we currently reset local_isolate_ to
  // nullptr at some point in the JSHeapBroker lifecycle.
  LocalIsolate* local_isolate_or_isolate() const {
    return local_isolate() != nullptr ? local_isolate()
                                      : isolate()->AsLocalIsolate();
  }

  // Return the corresponding canonical persistent handle for {object}. Create
  // one if it does not exist.
  // If we have the canonical map, we can create the canonical & persistent
  // handle through it. This commonly happens during the Execute phase.
  // If we don't, that means we are calling this method from serialization. If
  // that happens, we should be inside a canonical and a persistent handle
  // scope. Then, we would just use the regular handle creation.
  template <typename T>
  Handle<T> CanonicalPersistentHandle(T object) {
    if (canonical_handles_) {
      Address address = object.ptr();
      if (Internals::HasHeapObjectTag(address)) {
        RootIndex root_index;
        if (root_index_map_.Lookup(address, &root_index)) {
          return Handle<T>(isolate_->root_handle(root_index).location());
        }
      }

      Object obj(address);
      auto find_result = canonical_handles_->FindOrInsert(obj);
      if (!find_result.already_exists) {
        // Allocate new PersistentHandle if one wasn't created before.
        DCHECK_NOT_NULL(local_isolate());
        *find_result.entry =
            local_isolate()->heap()->NewPersistentHandle(obj).location();
      }
      return Handle<T>(*find_result.entry);
    } else {
      return Handle<T>(object, isolate());
    }
  }

  template <typename T>
  Handle<T> CanonicalPersistentHandle(Handle<T> object) {
    if (object.is_null()) return object;  // Can't deref a null handle.
    return CanonicalPersistentHandle<T>(*object);
  }

  // Find the corresponding handle in the CanonicalHandlesMap. The entry must be
  // found.
  template <typename T>
  Handle<T> FindCanonicalPersistentHandleForTesting(Object object) {
    Address** entry = canonical_handles_->Find(object);
    return Handle<T>(*entry);
  }

  // Set the persistent handles and copy the canonical handles over to the
  // JSHeapBroker.
  void SetPersistentAndCopyCanonicalHandlesForTesting(
      std::unique_ptr<PersistentHandles> persistent_handles,
      std::unique_ptr<CanonicalHandlesMap> canonical_handles);
  std::string Trace() const;
  void IncrementTracingIndentation();
  void DecrementTracingIndentation();

  RootIndexMap const& root_index_map() { return root_index_map_; }

  // Locks {mutex} through the duration of this scope iff it is the first
  // occurrence. This is done to have a recursive shared lock on {mutex}.
  class V8_NODISCARD MapUpdaterGuardIfNeeded final {
   public:
    explicit MapUpdaterGuardIfNeeded(JSHeapBroker* ptr,
                                     base::SharedMutex* mutex)
        : ptr_(ptr),
          initial_map_updater_mutex_depth_(ptr->map_updater_mutex_depth_),
          shared_mutex(mutex, should_lock()) {
      ptr_->map_updater_mutex_depth_++;
    }

    ~MapUpdaterGuardIfNeeded() {
      ptr_->map_updater_mutex_depth_--;
      DCHECK_EQ(initial_map_updater_mutex_depth_,
                ptr_->map_updater_mutex_depth_);
    }

    // Whether the MapUpdater mutex should be physically locked (if not, we
    // already hold the lock).
    bool should_lock() const { return initial_map_updater_mutex_depth_ == 0; }

   private:
    JSHeapBroker* const ptr_;
    const int initial_map_updater_mutex_depth_;
    base::SharedMutexGuardIf<base::kShared> shared_mutex;
  };

 private:
  friend class HeapObjectRef;
  friend class ObjectRef;
  friend class ObjectData;
  friend class PropertyCellData;

  // If this returns false, the object is guaranteed to be fully initialized and
  // thus safe to read from a memory safety perspective. The converse does not
  // necessarily hold.
  bool ObjectMayBeUninitialized(Handle<Object> object) const;
  bool ObjectMayBeUninitialized(HeapObject object) const;

  bool CanUseFeedback(const FeedbackNexus& nexus) const;
  const ProcessedFeedback& NewInsufficientFeedback(FeedbackSlotKind kind) const;

  // Bottleneck FeedbackNexus access here, for storage in the broker
  // or on-the-fly usage elsewhere in the compiler.
  ProcessedFeedback const& ReadFeedbackForArrayOrObjectLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForBinaryOperation(
      FeedbackSource const& source) const;
  ProcessedFeedback const& ReadFeedbackForCall(FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForCompareOperation(
      FeedbackSource const& source) const;
  ProcessedFeedback const& ReadFeedbackForForIn(
      FeedbackSource const& source) const;
  ProcessedFeedback const& ReadFeedbackForGlobalAccess(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForInstanceOf(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForPropertyAccess(
      FeedbackSource const& source, AccessMode mode,
      base::Optional<NameRef> static_name);
  ProcessedFeedback const& ReadFeedbackForRegExpLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForTemplateObject(
      FeedbackSource const& source);

  void CollectArrayAndObjectPrototypes();

  PerIsolateCompilerCache* compiler_cache() const { return compiler_cache_; }

  void set_persistent_handles(
      std::unique_ptr<PersistentHandles> persistent_handles) {
    DCHECK_NULL(ph_);
    ph_ = std::move(persistent_handles);
    DCHECK_NOT_NULL(ph_);
  }
  std::unique_ptr<PersistentHandles> DetachPersistentHandles() {
    DCHECK_NOT_NULL(ph_);
    return std::move(ph_);
  }

  void set_canonical_handles(
      std::unique_ptr<CanonicalHandlesMap> canonical_handles) {
    DCHECK_NULL(canonical_handles_);
    canonical_handles_ = std::move(canonical_handles);
    DCHECK_NOT_NULL(canonical_handles_);
  }

  std::unique_ptr<CanonicalHandlesMap> DetachCanonicalHandles() {
    DCHECK_NOT_NULL(canonical_handles_);
    return std::move(canonical_handles_);
  }

  // Copy the canonical handles over to the JSHeapBroker.
  void CopyCanonicalHandlesForTesting(
      std::unique_ptr<CanonicalHandlesMap> canonical_handles);

  Isolate* const isolate_;
  Zone* const zone_ = nullptr;
  base::Optional<NativeContextRef> target_native_context_;
  RefsMap* refs_;
  RootIndexMap root_index_map_;
  ZoneUnorderedSet<Handle<JSObject>, Handle<JSObject>::hash,
                   Handle<JSObject>::equal_to>
      array_and_object_prototypes_;
  BrokerMode mode_ = kDisabled;
  bool const tracing_enabled_;
  bool const is_concurrent_inlining_;
  bool const is_isolate_bootstrapping_;
  CodeKind const code_kind_;
  std::unique_ptr<PersistentHandles> ph_;
  LocalIsolate* local_isolate_ = nullptr;
  std::unique_ptr<CanonicalHandlesMap> canonical_handles_;
  unsigned trace_indentation_ = 0;
  PerIsolateCompilerCache* compiler_cache_ = nullptr;
  ZoneUnorderedMap<FeedbackSource, ProcessedFeedback const*,
                   FeedbackSource::Hash, FeedbackSource::Equal>
      feedback_;
  ZoneUnorderedMap<PropertyAccessTarget, PropertyAccessInfo,
                   PropertyAccessTarget::Hash, PropertyAccessTarget::Equal>
      property_access_infos_;
  ZoneUnorderedMap<FeedbackSource, MinimorphicLoadPropertyAccessInfo,
                   FeedbackSource::Hash, FeedbackSource::Equal>
      minimorphic_property_access_infos_;

  ZoneVector<ObjectData*> typed_array_string_tags_;

  struct SerializedFunction {
    SharedFunctionInfoRef shared;
    FeedbackVectorRef feedback;

    bool operator<(const SerializedFunction& other) const {
      if (shared.object().address() < other.shared.object().address()) {
        return true;
      }
      if (shared.object().address() == other.shared.object().address()) {
        return feedback.object().address() < other.feedback.object().address();
      }
      return false;
    }
  };
  ZoneMultimap<SerializedFunction, HintsVector> serialized_functions_;

  // The MapUpdater mutex is used in recursive patterns; for example,
  // ComputePropertyAccessInfo may call itself recursively. Thus we need to
  // emulate a recursive mutex, which we do by checking if this heap broker
  // instance already holds the mutex when a lock is requested. This field
  // holds the locking depth, i.e. how many times the mutex has been
  // recursively locked. Only the outermost locker actually locks underneath.
  int map_updater_mutex_depth_ = 0;

  static constexpr size_t kMaxSerializedFunctionsCacheSize = 200;
  static constexpr uint32_t kMinimalRefsBucketCount = 8;
  STATIC_ASSERT(base::bits::IsPowerOfTwo(kMinimalRefsBucketCount));
  static constexpr uint32_t kInitialRefsBucketCount = 1024;
  STATIC_ASSERT(base::bits::IsPowerOfTwo(kInitialRefsBucketCount));
};

class V8_NODISCARD TraceScope {
 public:
  TraceScope(JSHeapBroker* broker, const char* label)
      : TraceScope(broker, static_cast<void*>(broker), label) {}

  TraceScope(JSHeapBroker* broker, ObjectData* data, const char* label)
      : TraceScope(broker, static_cast<void*>(data), label) {}

  TraceScope(JSHeapBroker* broker, void* subject, const char* label)
      : broker_(broker) {
    TRACE_BROKER(broker_, "Running " << label << " on " << subject);
    broker_->IncrementTracingIndentation();
  }

  ~TraceScope() { broker_->DecrementTracingIndentation(); }

 private:
  JSHeapBroker* const broker_;
};

#define ASSIGN_RETURN_NO_CHANGE_IF_DATA_MISSING(something_var,             \
                                                optionally_something)      \
  auto optionally_something_ = optionally_something;                       \
  if (!optionally_something_)                                              \
    return NoChangeBecauseOfMissingData(broker(), __FUNCTION__, __LINE__); \
  something_var = *optionally_something_;

class Reduction;
Reduction NoChangeBecauseOfMissingData(JSHeapBroker* broker,
                                       const char* function, int line);

// Miscellaneous definitions that should be moved elsewhere once concurrent
// compilation is finished.
bool CanInlineElementAccess(MapRef const& map);

// Scope that unparks the LocalHeap, if:
//   a) We have a JSHeapBroker,
//   b) Said JSHeapBroker has a LocalIsolate and thus a LocalHeap,
//   c) Said LocalHeap has been parked and
//   d) The given condition evaluates to true.
// Used, for example, when printing the graph with --trace-turbo with a
// previously parked LocalHeap.
class V8_NODISCARD UnparkedScopeIfNeeded {
 public:
  explicit UnparkedScopeIfNeeded(JSHeapBroker* broker,
                                 bool extra_condition = true) {
    if (broker != nullptr && extra_condition) {
      LocalIsolate* local_isolate = broker->local_isolate();
      if (local_isolate != nullptr && local_isolate->heap()->IsParked()) {
        unparked_scope.emplace(local_isolate->heap());
      }
    }
  }

 private:
  base::Optional<UnparkedScope> unparked_scope;
};

// Usage:
//
//  base::Optional<FooRef> ref = TryMakeRef(broker, o);
//  if (!ref.has_value()) return {};  // bailout
//
// or
//
//  FooRef ref = MakeRef(broker, o);
template <class T,
          typename = std::enable_if_t<std::is_convertible<T*, Object*>::value>>
base::Optional<typename ref_traits<T>::ref_type> TryMakeRef(
    JSHeapBroker* broker, T object, GetOrCreateDataFlags flags = {}) {
  ObjectData* data = broker->TryGetOrCreateData(object, flags);
  if (data == nullptr) {
    TRACE_BROKER_MISSING(broker, "ObjectData for " << Brief(object));
    return {};
  }
  return {typename ref_traits<T>::ref_type(broker, data)};
}

template <class T,
          typename = std::enable_if_t<std::is_convertible<T*, Object*>::value>>
base::Optional<typename ref_traits<T>::ref_type> TryMakeRef(
    JSHeapBroker* broker, Handle<T> object, GetOrCreateDataFlags flags = {}) {
  ObjectData* data = broker->TryGetOrCreateData(object, flags);
  if (data == nullptr) {
    TRACE_BROKER_MISSING(broker, "ObjectData for " << Brief(*object));
    return {};
  }
  return {typename ref_traits<T>::ref_type(broker, data)};
}

template <class T,
          typename = std::enable_if_t<std::is_convertible<T*, Object*>::value>>
typename ref_traits<T>::ref_type MakeRef(JSHeapBroker* broker, T object) {
  return TryMakeRef(broker, object).value();
}

template <class T,
          typename = std::enable_if_t<std::is_convertible<T*, Object*>::value>>
typename ref_traits<T>::ref_type MakeRef(JSHeapBroker* broker,
                                         Handle<T> object) {
  return TryMakeRef(broker, object).value();
}

template <class T,
          typename = std::enable_if_t<std::is_convertible<T*, Object*>::value>>
typename ref_traits<T>::ref_type MakeRefAssumeMemoryFence(JSHeapBroker* broker,
                                                          T object) {
  return TryMakeRef(broker, object, kAssumeMemoryFence).value();
}

template <class T,
          typename = std::enable_if_t<std::is_convertible<T*, Object*>::value>>
typename ref_traits<T>::ref_type MakeRefAssumeMemoryFence(JSHeapBroker* broker,
                                                          Handle<T> object) {
  return TryMakeRef(broker, object, kAssumeMemoryFence).value();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_HEAP_BROKER_H_
