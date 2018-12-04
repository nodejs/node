// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_HEAP_BROKER_H_
#define V8_COMPILER_JS_HEAP_BROKER_H_

#include "src/base/compiler-specific.h"
#include "src/base/optional.h"
#include "src/compiler/refs-map.h"
#include "src/globals.h"
#include "src/objects.h"
#include "src/objects/builtin-function-id.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

enum class OddballType : uint8_t {
  kNone,     // Not an Oddball.
  kBoolean,  // True or False.
  kUndefined,
  kNull,
  kHole,
  kUninitialized,
  kOther  // Oddball, but none of the above.
};

// This list is sorted such that subtypes appear before their supertypes.
// DO NOT VIOLATE THIS PROPERTY!
#define HEAP_BROKER_OBJECT_LIST(V) \
  /* Subtypes of JSObject */       \
  V(JSArray)                       \
  V(JSFunction)                    \
  V(JSGlobalProxy)                 \
  V(JSRegExp)                      \
  /* Subtypes of Context */        \
  V(NativeContext)                 \
  /* Subtypes of FixedArray */     \
  V(Context)                       \
  V(ScopeInfo)                     \
  V(ScriptContextTable)            \
  /* Subtypes of FixedArrayBase */ \
  V(BytecodeArray)                 \
  V(FixedArray)                    \
  V(FixedDoubleArray)              \
  /* Subtypes of Name */           \
  V(InternalizedString)            \
  V(String)                        \
  /* Subtypes of HeapObject */     \
  V(AllocationSite)                \
  V(Cell)                          \
  V(Code)                          \
  V(DescriptorArray)               \
  V(FeedbackVector)                \
  V(FixedArrayBase)                \
  V(HeapNumber)                    \
  V(JSObject)                      \
  V(Map)                           \
  V(Module)                        \
  V(MutableHeapNumber)             \
  V(Name)                          \
  V(PropertyCell)                  \
  V(SharedFunctionInfo)            \
  /* Subtypes of Object */         \
  V(HeapObject)

class CompilationDependencies;
class JSHeapBroker;
class ObjectData;
#define FORWARD_DECL(Name) class Name##Ref;
HEAP_BROKER_OBJECT_LIST(FORWARD_DECL)
#undef FORWARD_DECL

class ObjectRef {
 public:
  ObjectRef(JSHeapBroker* broker, Handle<Object> object);
  ObjectRef(JSHeapBroker* broker, ObjectData* data)
      : broker_(broker), data_(data) {
    CHECK_NOT_NULL(data_);
  }

  bool equals(const ObjectRef& other) const;

  Handle<Object> object() const;
  // TODO(neis): Remove eventually.
  template <typename T>
  Handle<T> object() const {
    AllowHandleDereference handle_dereference;
    return Handle<T>::cast(object());
  }

  bool IsSmi() const;
  int AsSmi() const;

#define HEAP_IS_METHOD_DECL(Name) bool Is##Name() const;
  HEAP_BROKER_OBJECT_LIST(HEAP_IS_METHOD_DECL)
#undef HEAP_IS_METHOD_DECL

#define HEAP_AS_METHOD_DECL(Name) Name##Ref As##Name() const;
  HEAP_BROKER_OBJECT_LIST(HEAP_AS_METHOD_DECL)
#undef HEAP_AS_METHOD_DECL

  bool BooleanValue() const;
  double OddballToNumber() const;

  Isolate* isolate() const;

 protected:
  JSHeapBroker* broker() const;
  ObjectData* data() const;

 private:
  JSHeapBroker* broker_;
  ObjectData* data_;
};

// Temporary class that carries information from a Map. We'd like to remove
// this class and use MapRef instead, but we can't as long as we support the
// kDisabled broker mode. That's because obtaining the MapRef via
// HeapObjectRef::map() requires a HandleScope when the broker is disabled.
// During OptimizeGraph we generally don't have a HandleScope, however. There
// are two places where we therefore use GetHeapObjectType() instead. Both that
// function and this class should eventually be removed.
class HeapObjectType {
 public:
  enum Flag : uint8_t { kUndetectable = 1 << 0, kCallable = 1 << 1 };

  typedef base::Flags<Flag> Flags;

  HeapObjectType(InstanceType instance_type, Flags flags,
                 OddballType oddball_type)
      : instance_type_(instance_type),
        oddball_type_(oddball_type),
        flags_(flags) {
    DCHECK_EQ(instance_type == ODDBALL_TYPE,
              oddball_type != OddballType::kNone);
  }

  OddballType oddball_type() const { return oddball_type_; }
  InstanceType instance_type() const { return instance_type_; }
  Flags flags() const { return flags_; }

  bool is_callable() const { return flags_ & kCallable; }
  bool is_undetectable() const { return flags_ & kUndetectable; }

 private:
  InstanceType const instance_type_;
  OddballType const oddball_type_;
  Flags const flags_;
};

class HeapObjectRef : public ObjectRef {
 public:
  using ObjectRef::ObjectRef;

  MapRef map() const;

  // See the comment on the HeapObjectType class.
  HeapObjectType GetHeapObjectType() const;
};

class PropertyCellRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  PropertyDetails property_details() const;
  ObjectRef value() const;
};

class JSObjectRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  double RawFastDoublePropertyAt(FieldIndex index) const;
  ObjectRef RawFastPropertyAt(FieldIndex index) const;

  FixedArrayBaseRef elements() const;
  void EnsureElementsTenured();
  ElementsKind GetElementsKind() const;

  void SerializeObjectCreateMap();
  base::Optional<MapRef> GetObjectCreateMap() const;
};

class JSFunctionRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;

  bool has_initial_map() const;
  bool has_prototype() const;
  bool PrototypeRequiresRuntimeLookup() const;

  void Serialize();

  // The following are available only after calling Serialize().
  ObjectRef prototype() const;
  MapRef initial_map() const;
  JSGlobalProxyRef global_proxy() const;
  SharedFunctionInfoRef shared() const;
  int InitialMapInstanceSizeWithMinSlack() const;
};

class JSRegExpRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;

  ObjectRef raw_properties_or_hash() const;
  ObjectRef data() const;
  ObjectRef source() const;
  ObjectRef flags() const;
  ObjectRef last_index() const;
};

class HeapNumberRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  double value() const;
};

class MutableHeapNumberRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  double value() const;
};

class ContextRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  void Serialize();

  ContextRef previous() const;
  ObjectRef get(int index) const;
};

#define BROKER_COMPULSORY_NATIVE_CONTEXT_FIELDS(V) \
  V(JSFunction, array_function)                    \
  V(JSFunction, object_function)                   \
  V(JSFunction, promise_function)                  \
  V(Map, fast_aliased_arguments_map)               \
  V(Map, initial_array_iterator_map)               \
  V(Map, initial_string_iterator_map)              \
  V(Map, iterator_result_map)                      \
  V(Map, js_array_holey_double_elements_map)       \
  V(Map, js_array_holey_elements_map)              \
  V(Map, js_array_holey_smi_elements_map)          \
  V(Map, js_array_packed_double_elements_map)      \
  V(Map, js_array_packed_elements_map)             \
  V(Map, js_array_packed_smi_elements_map)         \
  V(Map, sloppy_arguments_map)                     \
  V(Map, slow_object_with_null_prototype_map)      \
  V(Map, strict_arguments_map)                     \
  V(ScriptContextTable, script_context_table)

// Those are set by Bootstrapper::ExportFromRuntime, which may not yet have
// happened when Turbofan is invoked via --always-opt.
#define BROKER_OPTIONAL_NATIVE_CONTEXT_FIELDS(V) \
  V(Map, map_key_iterator_map)                   \
  V(Map, map_key_value_iterator_map)             \
  V(Map, map_value_iterator_map)                 \
  V(Map, set_key_value_iterator_map)             \
  V(Map, set_value_iterator_map)

#define BROKER_NATIVE_CONTEXT_FIELDS(V)      \
  BROKER_COMPULSORY_NATIVE_CONTEXT_FIELDS(V) \
  BROKER_OPTIONAL_NATIVE_CONTEXT_FIELDS(V)

class NativeContextRef : public ContextRef {
 public:
  using ContextRef::ContextRef;
  void Serialize();

#define DECL_ACCESSOR(type, name) type##Ref name() const;
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  MapRef GetFunctionMapFromIndex(int index) const;
  MapRef GetInitialJSArrayMap(ElementsKind kind) const;
};

class NameRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
};

class ScriptContextTableRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  struct LookupResult {
    ContextRef context;
    bool immutable;
    int index;
  };

  base::Optional<LookupResult> lookup(const NameRef& name) const;
};

class DescriptorArrayRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
};

class FeedbackVectorRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  ObjectRef get(FeedbackSlot slot) const;

  void SerializeSlots();
};

class AllocationSiteRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  bool PointsToLiteral() const;
  PretenureFlag GetPretenureMode() const;
  ObjectRef nested_site() const;

  // {IsFastLiteral} determines whether the given array or object literal
  // boilerplate satisfies all limits to be considered for fast deep-copying
  // and computes the total size of all objects that are part of the graph.
  //
  // If PointsToLiteral() is false, then IsFastLiteral() is also false.
  bool IsFastLiteral() const;
  // We only serialize boilerplate if IsFastLiteral is true.
  base::Optional<JSObjectRef> boilerplate() const;

  ElementsKind GetElementsKind() const;
  bool CanInlineCall() const;
};

class MapRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  int instance_size() const;
  InstanceType instance_type() const;
  int GetInObjectProperties() const;
  int GetInObjectPropertiesStartInWords() const;
  int NumberOfOwnDescriptors() const;
  int GetInObjectPropertyOffset(int index) const;
  ElementsKind elements_kind() const;
  bool is_stable() const;
  bool is_constructor() const;
  bool has_prototype_slot() const;
  bool is_deprecated() const;
  bool CanBeDeprecated() const;
  bool CanTransition() const;
  bool IsInobjectSlackTrackingInProgress() const;
  bool is_dictionary_map() const;
  bool IsFixedCowArrayMap() const;
  bool is_undetectable() const;
  bool is_callable() const;

  ObjectRef constructor_or_backpointer() const;
  ObjectRef prototype() const;

  OddballType oddball_type() const;

  base::Optional<MapRef> AsElementsKind(ElementsKind kind) const;

  // Concerning the underlying instance_descriptors:
  void SerializeOwnDescriptors();
  MapRef FindFieldOwner(int descriptor_index) const;
  PropertyDetails GetPropertyDetails(int descriptor_index) const;
  NameRef GetPropertyKey(int descriptor_index) const;
  FieldIndex GetFieldIndexFor(int descriptor_index) const;
  ObjectRef GetFieldType(int descriptor_index) const;
  bool IsUnboxedDoubleField(int descriptor_index) const;
};

class FixedArrayBaseRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  int length() const;
};

class FixedArrayRef : public FixedArrayBaseRef {
 public:
  using FixedArrayBaseRef::FixedArrayBaseRef;

  ObjectRef get(int i) const;
};

class FixedDoubleArrayRef : public FixedArrayBaseRef {
 public:
  using FixedArrayBaseRef::FixedArrayBaseRef;

  double get_scalar(int i) const;
  bool is_the_hole(int i) const;
};

class BytecodeArrayRef : public FixedArrayBaseRef {
 public:
  using FixedArrayBaseRef::FixedArrayBaseRef;

  int register_count() const;
};

class JSArrayRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;

  ObjectRef length() const;
};

class ScopeInfoRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  int ContextLength() const;
};

#define BROKER_SFI_FIELDS(V)                \
  V(int, internal_formal_parameter_count)   \
  V(bool, has_duplicate_parameters)         \
  V(int, function_map_index)                \
  V(FunctionKind, kind)                     \
  V(LanguageMode, language_mode)            \
  V(bool, native)                           \
  V(bool, HasBreakInfo)                     \
  V(bool, HasBuiltinFunctionId)             \
  V(bool, HasBuiltinId)                     \
  V(BuiltinFunctionId, builtin_function_id) \
  V(bool, construct_as_builtin)             \
  V(bool, HasBytecodeArray)

class SharedFunctionInfoRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  int builtin_id() const;
  BytecodeArrayRef GetBytecodeArray() const;
#define DECL_ACCESSOR(type, name) type name() const;
  BROKER_SFI_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR
};

class StringRef : public NameRef {
 public:
  using NameRef::NameRef;

  int length() const;
  uint16_t GetFirstChar();
  base::Optional<double> ToNumber();
  bool IsSeqString() const;
  bool IsExternalString() const;
};

class ModuleRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  void Serialize();

  CellRef GetCell(int cell_index) const;
};

class CellRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
};

class JSGlobalProxyRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
};

class CodeRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
};

class InternalizedStringRef : public StringRef {
 public:
  using StringRef::StringRef;
};

class PerIsolateCompilerCache;

class V8_EXPORT_PRIVATE JSHeapBroker : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  JSHeapBroker(Isolate* isolate, Zone* broker_zone);
  void SetNativeContextRef();
  void SerializeStandardObjects();

  Isolate* isolate() const { return isolate_; }
  Zone* zone() const { return current_zone_; }
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
  ObjectData* GetOrCreateData(Object*);

  void Trace(const char* format, ...) const;
  void IncrementTracingIndentation();
  void DecrementTracingIndentation();

 private:
  friend class HeapObjectRef;
  friend class ObjectRef;
  friend class ObjectData;

  void SerializeShareableObjects();

  Isolate* const isolate_;
  Zone* const broker_zone_;
  Zone* current_zone_;
  base::Optional<NativeContextRef> native_context_;
  RefsMap* refs_;

  BrokerMode mode_ = kDisabled;
  unsigned tracing_indentation_ = 0;
  PerIsolateCompilerCache* compiler_cache_;

  static const size_t kMinimalRefsBucketCount = 8;     // must be power of 2
  static const size_t kInitialRefsBucketCount = 1024;  // must be power of 2
};

#define ASSIGN_RETURN_NO_CHANGE_IF_DATA_MISSING(something_var,          \
                                                optionally_something)   \
  auto optionally_something_ = optionally_something;                    \
  if (!optionally_something_)                                           \
    return NoChangeBecauseOfMissingData(js_heap_broker(), __FUNCTION__, \
                                        __LINE__);                      \
  something_var = *optionally_something_;

class Reduction;
Reduction NoChangeBecauseOfMissingData(JSHeapBroker* broker,
                                       const char* function, int line);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_HEAP_BROKER_H_
