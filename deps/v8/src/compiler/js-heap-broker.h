// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_HEAP_BROKER_H_
#define V8_COMPILER_JS_HEAP_BROKER_H_

#include "src/base/compiler-specific.h"
#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/compiler/access-info.h"
#include "src/compiler/refs-map.h"
#include "src/handles/handles.h"
#include "src/interpreter/bytecode-array-accessor.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/function-kind.h"
#include "src/objects/objects.h"
#include "src/utils/ostreams.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class BytecodeAnalysis;
class ObjectRef;
std::ostream& operator<<(std::ostream& os, const ObjectRef& ref);

struct FeedbackSource {
  FeedbackSource(Handle<FeedbackVector> vector_, FeedbackSlot slot_)
      : vector(vector_), slot(slot_) {}
  explicit FeedbackSource(FeedbackNexus const& nexus);
  explicit FeedbackSource(VectorSlotPair const& pair);

  Handle<FeedbackVector> const vector;
  FeedbackSlot const slot;

  struct Hash {
    size_t operator()(FeedbackSource const& source) const {
      return base::hash_combine(source.vector.address(), source.slot);
    }
  };

  struct Equal {
    bool operator()(FeedbackSource const& lhs,
                    FeedbackSource const& rhs) const {
      return lhs.vector.equals(rhs.vector) && lhs.slot == rhs.slot;
    }
  };
};

#define TRACE_BROKER(broker, x)                                      \
  do {                                                               \
    if (broker->tracing_enabled() && FLAG_trace_heap_broker_verbose) \
      broker->Trace() << x << '\n';                                  \
  } while (false)

#define TRACE_BROKER_MISSING(broker, x)                             \
  do {                                                              \
    if (broker->tracing_enabled())                                  \
      broker->Trace() << __FUNCTION__ << ": missing " << x << '\n'; \
  } while (false)

class V8_EXPORT_PRIVATE JSHeapBroker {
 public:
  JSHeapBroker(Isolate* isolate, Zone* broker_zone, bool tracing_enabled);

  void SetNativeContextRef();
  void SerializeStandardObjects();

  Isolate* isolate() const { return isolate_; }
  Zone* zone() const { return current_zone_; }
  bool tracing_enabled() const { return tracing_enabled_; }
  NativeContextRef native_context() const { return native_context_.value(); }
  PerIsolateCompilerCache* compiler_cache() const { return compiler_cache_; }

  enum BrokerMode { kDisabled, kSerializing, kSerialized, kRetired };
  BrokerMode mode() const { return mode_; }
  void StartSerializing();
  void StopSerializing();
  void Retire();
  bool SerializingAllowed() const;

  // Returns nullptr iff handle unknown.
  ObjectData* GetData(Handle<Object>) const;
  // Never returns nullptr.
  ObjectData* GetOrCreateData(Handle<Object>);
  // Like the previous but wraps argument in handle first (for convenience).
  ObjectData* GetOrCreateData(Object);

  // Check if {object} is any native context's %ArrayPrototype% or
  // %ObjectPrototype%.
  bool IsArrayOrObjectPrototype(const JSObjectRef& object) const;

  bool HasFeedback(FeedbackSource const& source) const;
  // The processed {feedback} can be {nullptr}, indicating that the original
  // feedback didn't contain information relevant for Turbofan.
  void SetFeedback(FeedbackSource const& source,
                   ProcessedFeedback const* feedback);
  ProcessedFeedback const* GetFeedback(FeedbackSource const& source) const;

  // Convenience wrappers around GetFeedback.
  GlobalAccessFeedback const* GetGlobalAccessFeedback(
      FeedbackSource const& source) const;

  // TODO(neis): Move these into serializer when we're always in the background.
  ElementAccessFeedback const* ProcessFeedbackMapsForElementAccess(
      MapHandles const& maps, KeyedAccessMode const& keyed_mode);
  GlobalAccessFeedback const* ProcessFeedbackForGlobalAccess(
      FeedbackSource const& source);

  BytecodeAnalysis const& GetBytecodeAnalysis(
      Handle<BytecodeArray> bytecode_array, BailoutId osr_offset,
      bool analyze_liveness, bool serialize);

  base::Optional<NameRef> GetNameFeedback(FeedbackNexus const& nexus);

  // If there is no result stored for {map}, we return an Invalid
  // PropertyAccessInfo.
  PropertyAccessInfo GetAccessInfoForLoadingThen(MapRef map);
  void CreateAccessInfoForLoadingThen(MapRef map,
                                      CompilationDependencies* dependencies);
  PropertyAccessInfo GetAccessInfoForLoadingExec(MapRef map);
  PropertyAccessInfo const& CreateAccessInfoForLoadingExec(
      MapRef map, CompilationDependencies* dependencies);

  std::ostream& Trace();
  void IncrementTracingIndentation();
  void DecrementTracingIndentation();

 private:
  friend class HeapObjectRef;
  friend class ObjectRef;
  friend class ObjectData;

  void SerializeShareableObjects();
  void CollectArrayAndObjectPrototypes();

  Isolate* const isolate_;
  Zone* const broker_zone_;
  Zone* current_zone_;
  base::Optional<NativeContextRef> native_context_;
  RefsMap* refs_;
  ZoneUnorderedSet<Handle<JSObject>, Handle<JSObject>::hash,
                   Handle<JSObject>::equal_to>
      array_and_object_prototypes_;
  BrokerMode mode_ = kDisabled;
  bool const tracing_enabled_;
  StdoutStream trace_out_;
  unsigned trace_indentation_ = 0;
  PerIsolateCompilerCache* compiler_cache_;
  ZoneUnorderedMap<FeedbackSource, ProcessedFeedback const*,
                   FeedbackSource::Hash, FeedbackSource::Equal>
      feedback_;
  ZoneUnorderedMap<ObjectData*, BytecodeAnalysis*> bytecode_analyses_;
  typedef ZoneUnorderedMap<MapRef, PropertyAccessInfo, ObjectRef::Hash,
                           ObjectRef::Equal>
      MapToAccessInfos;
  MapToAccessInfos ais_for_loading_then_;
  MapToAccessInfos ais_for_loading_exec_;

  static const size_t kMinimalRefsBucketCount = 8;     // must be power of 2
  static const size_t kInitialRefsBucketCount = 1024;  // must be power of 2
};

class TraceScope {
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

class OffHeapBytecodeArray final : public interpreter::AbstractBytecodeArray {
 public:
  explicit OffHeapBytecodeArray(BytecodeArrayRef bytecode_array);

  int length() const override;
  int parameter_count() const override;
  uint8_t get(int index) const override;
  void set(int index, uint8_t value) override;
  Address GetFirstBytecodeAddress() const override;
  Handle<Object> GetConstantAtIndex(int index, Isolate* isolate) const override;
  bool IsConstantAtIndexSmi(int index) const override;
  Smi GetConstantAtIndexAsSmi(int index) const override;

 private:
  BytecodeArrayRef array_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_HEAP_BROKER_H_
