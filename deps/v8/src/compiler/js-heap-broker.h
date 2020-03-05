// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_HEAP_BROKER_H_
#define V8_COMPILER_JS_HEAP_BROKER_H_

#include "src/base/compiler-specific.h"
#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/compiler/access-info.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/processed-feedback.h"
#include "src/compiler/refs-map.h"
#include "src/compiler/serializer-hints.h"
#include "src/handles/handles.h"
#include "src/interpreter/bytecode-array-accessor.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/function-kind.h"
#include "src/objects/objects.h"
#include "src/utils/address-map.h"
#include "src/utils/ostreams.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class BytecodeAnalysis;
class ObjectRef;
std::ostream& operator<<(std::ostream& os, const ObjectRef& ref);

#define TRACE_BROKER(broker, x)                                      \
  do {                                                               \
    if (broker->tracing_enabled() && FLAG_trace_heap_broker_verbose) \
      broker->Trace() << x << '\n';                                  \
  } while (false)

#define TRACE_BROKER_MEMORY(broker, x)                              \
  do {                                                              \
    if (broker->tracing_enabled() && FLAG_trace_heap_broker_memory) \
      broker->Trace() << x << std::endl;                            \
  } while (false)

#define TRACE_BROKER_MISSING(broker, x)                             \
  do {                                                              \
    if (broker->tracing_enabled())                                  \
      broker->Trace() << "Missing " << x << " (" << __FILE__ << ":" \
                      << __LINE__ << ")" << std::endl;              \
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

class V8_EXPORT_PRIVATE JSHeapBroker {
 public:
  JSHeapBroker(Isolate* isolate, Zone* broker_zone, bool tracing_enabled,
               bool is_concurrent_inlining);

  // The compilation target's native context. We need the setter because at
  // broker construction time we don't yet have the canonical handle.
  NativeContextRef target_native_context() const {
    return target_native_context_.value();
  }
  void SetTargetNativeContextRef(Handle<NativeContext> native_context);

  void InitializeAndStartSerializing(Handle<NativeContext> native_context);

  Isolate* isolate() const { return isolate_; }
  Zone* zone() const { return zone_; }
  bool tracing_enabled() const { return tracing_enabled_; }
  bool is_concurrent_inlining() const { return is_concurrent_inlining_; }

  enum BrokerMode { kDisabled, kSerializing, kSerialized, kRetired };
  BrokerMode mode() const { return mode_; }
  void StopSerializing();
  void Retire();
  bool SerializingAllowed() const;

#ifdef DEBUG
  void PrintRefsAnalysis() const;
#endif  // DEBUG

  // Retruns the handle from root index table for read only heap objects.
  Handle<Object> GetRootHandle(Object object);

  // Never returns nullptr.
  ObjectData* GetOrCreateData(Handle<Object>);
  // Like the previous but wraps argument in handle first (for convenience).
  ObjectData* GetOrCreateData(Object);

  // Check if {object} is any native context's %ArrayPrototype% or
  // %ObjectPrototype%.
  bool IsArrayOrObjectPrototype(const JSObjectRef& object) const;

  bool HasFeedback(FeedbackSource const& source) const;
  void SetFeedback(FeedbackSource const& source,
                   ProcessedFeedback const* feedback);
  ProcessedFeedback const& GetFeedback(FeedbackSource const& source) const;
  FeedbackSlotKind GetFeedbackSlotKind(FeedbackSource const& source) const;

  // TODO(neis): Move these into serializer when we're always in the background.
  ElementAccessFeedback const& ProcessFeedbackMapsForElementAccess(
      MapHandles const& maps, KeyedAccessMode const& keyed_mode,
      FeedbackSlotKind slot_kind);
  BytecodeAnalysis const& GetBytecodeAnalysis(
      Handle<BytecodeArray> bytecode_array, BailoutId osr_offset,
      bool analyze_liveness,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

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

  StringRef GetTypedArrayStringTag(ElementsKind kind);

  bool ShouldBeSerializedForCompilation(const SharedFunctionInfoRef& shared,
                                        const FeedbackVectorRef& feedback,
                                        const HintsVector& arguments) const;
  void SetSerializedForCompilation(const SharedFunctionInfoRef& shared,
                                   const FeedbackVectorRef& feedback,
                                   const HintsVector& arguments);
  bool IsSerializedForCompilation(const SharedFunctionInfoRef& shared,
                                  const FeedbackVectorRef& feedback) const;

  std::ostream& Trace() const;
  void IncrementTracingIndentation();
  void DecrementTracingIndentation();

  RootIndexMap const& root_index_map() { return root_index_map_; }

 private:
  friend class HeapObjectRef;
  friend class ObjectRef;
  friend class ObjectData;

  // Bottleneck FeedbackNexus access here, for storage in the broker
  // or on-the-fly usage elsewhere in the compiler.
  ForInHint ReadFeedbackForForIn(FeedbackSource const& source) const;
  CompareOperationHint ReadFeedbackForCompareOperation(
      FeedbackSource const& source) const;
  BinaryOperationHint ReadFeedbackForBinaryOperation(
      FeedbackSource const& source) const;

  ProcessedFeedback const& ReadFeedbackForCall(FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForGlobalAccess(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForInstanceOf(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForPropertyAccess(
      FeedbackSource const& source, AccessMode mode,
      base::Optional<NameRef> static_name);
  ProcessedFeedback const& ReadFeedbackForArrayOrObjectLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForRegExpLiteral(
      FeedbackSource const& source);
  ProcessedFeedback const& ReadFeedbackForTemplateObject(
      FeedbackSource const& source);

  void CollectArrayAndObjectPrototypes();
  void SerializeTypedArrayStringTags();

  PerIsolateCompilerCache* compiler_cache() const { return compiler_cache_; }

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
  mutable StdoutStream trace_out_;
  unsigned trace_indentation_ = 0;
  PerIsolateCompilerCache* compiler_cache_ = nullptr;
  ZoneUnorderedMap<FeedbackSource, ProcessedFeedback const*,
                   FeedbackSource::Hash, FeedbackSource::Equal>
      feedback_;
  ZoneUnorderedMap<ObjectData*, BytecodeAnalysis*> bytecode_analyses_;
  ZoneUnorderedMap<PropertyAccessTarget, PropertyAccessInfo,
                   PropertyAccessTarget::Hash, PropertyAccessTarget::Equal>
      property_access_infos_;

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

  static const size_t kMaxSerializedFunctionsCacheSize = 200;
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
