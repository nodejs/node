// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_HEAP_REFS_H_
#define V8_COMPILER_HEAP_REFS_H_

#include "src/base/optional.h"
#include "src/ic/call-optimization.h"
#include "src/objects/elements-kind.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/instance-type.h"
#include "src/utils/boxed-float.h"

namespace v8 {

class CFunctionInfo;

namespace internal {

class BytecodeArray;
class CallHandlerInfo;
class FixedDoubleArray;
class FunctionTemplateInfo;
class HeapNumber;
class InternalizedString;
class JSBoundFunction;
class JSDataView;
class JSGlobalProxy;
class JSTypedArray;
class NativeContext;
class ScriptContextTable;
template <typename>
class Signature;

namespace wasm {
class ValueType;
struct WasmModule;
}  // namespace wasm

namespace compiler {

class CompilationDependencies;
struct FeedbackSource;
class JSHeapBroker;
class ObjectData;
class PerIsolateCompilerCache;
class PropertyAccessInfo;

// Whether we are loading a property or storing to a property.
// For a store during literal creation, do not walk up the prototype chain.
// For a define operation, we behave similarly to kStoreInLiteral, but with
// distinct semantics for private class fields (in which private field
// accesses must throw when storing a field which does not exist, or
// adding/defining a field which already exists).
enum class AccessMode { kLoad, kStore, kStoreInLiteral, kHas, kDefine };

inline bool IsAnyStore(AccessMode mode) {
  return mode == AccessMode::kStore || mode == AccessMode::kStoreInLiteral;
}

enum class OddballType : uint8_t {
  kNone,     // Not an Oddball.
  kBoolean,  // True or False.
  kUndefined,
  kNull,
  kHole,
  kUninitialized,
  kOther  // Oddball, but none of the above.
};

enum class RefSerializationKind {
  // Skips serialization.
  kNeverSerialized,
  // Can be serialized on demand from the background thread.
  kBackgroundSerialized,
};

// This list is sorted such that subtypes appear before their supertypes.
// DO NOT VIOLATE THIS PROPERTY!
#define HEAP_BROKER_OBJECT_LIST_BASE(BACKGROUND_SERIALIZED, NEVER_SERIALIZED) \
  /* Subtypes of JSObject */                                                  \
  BACKGROUND_SERIALIZED(JSArray)                                              \
  BACKGROUND_SERIALIZED(JSBoundFunction)                                      \
  BACKGROUND_SERIALIZED(JSDataView)                                           \
  BACKGROUND_SERIALIZED(JSFunction)                                           \
  BACKGROUND_SERIALIZED(JSGlobalObject)                                       \
  BACKGROUND_SERIALIZED(JSGlobalProxy)                                        \
  BACKGROUND_SERIALIZED(JSTypedArray)                                         \
  /* Subtypes of Context */                                                   \
  NEVER_SERIALIZED(NativeContext)                                             \
  /* Subtypes of FixedArray */                                                \
  NEVER_SERIALIZED(ObjectBoilerplateDescription)                              \
  BACKGROUND_SERIALIZED(ScriptContextTable)                                   \
  /* Subtypes of String */                                                    \
  NEVER_SERIALIZED(InternalizedString)                                        \
  /* Subtypes of FixedArrayBase */                                            \
  NEVER_SERIALIZED(BytecodeArray)                                             \
  BACKGROUND_SERIALIZED(FixedArray)                                           \
  NEVER_SERIALIZED(FixedDoubleArray)                                          \
  /* Subtypes of Name */                                                      \
  NEVER_SERIALIZED(String)                                                    \
  NEVER_SERIALIZED(Symbol)                                                    \
  /* Subtypes of JSReceiver */                                                \
  BACKGROUND_SERIALIZED(JSObject)                                             \
  /* Subtypes of HeapObject */                                                \
  NEVER_SERIALIZED(AccessorInfo)                                              \
  NEVER_SERIALIZED(AllocationSite)                                            \
  NEVER_SERIALIZED(ArrayBoilerplateDescription)                               \
  BACKGROUND_SERIALIZED(BigInt)                                               \
  NEVER_SERIALIZED(CallHandlerInfo)                                           \
  NEVER_SERIALIZED(Cell)                                                      \
  NEVER_SERIALIZED(Code)                                                      \
  NEVER_SERIALIZED(CodeDataContainer)                                         \
  NEVER_SERIALIZED(Context)                                                   \
  NEVER_SERIALIZED(DescriptorArray)                                           \
  NEVER_SERIALIZED(FeedbackCell)                                              \
  NEVER_SERIALIZED(FeedbackVector)                                            \
  BACKGROUND_SERIALIZED(FixedArrayBase)                                       \
  NEVER_SERIALIZED(FunctionTemplateInfo)                                      \
  NEVER_SERIALIZED(HeapNumber)                                                \
  BACKGROUND_SERIALIZED(JSReceiver)                                           \
  BACKGROUND_SERIALIZED(Map)                                                  \
  NEVER_SERIALIZED(Name)                                                      \
  BACKGROUND_SERIALIZED(PropertyCell)                                         \
  NEVER_SERIALIZED(RegExpBoilerplateDescription)                              \
  NEVER_SERIALIZED(ScopeInfo)                                                 \
  NEVER_SERIALIZED(SharedFunctionInfo)                                        \
  NEVER_SERIALIZED(SourceTextModule)                                          \
  NEVER_SERIALIZED(TemplateObjectDescription)                                 \
  /* Subtypes of Object */                                                    \
  BACKGROUND_SERIALIZED(HeapObject)

#define HEAP_BROKER_OBJECT_LIST(V) HEAP_BROKER_OBJECT_LIST_BASE(V, V)
#define IGNORE_CASE(...)
#define HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(V) \
  HEAP_BROKER_OBJECT_LIST_BASE(V, IGNORE_CASE)

#define FORWARD_DECL(Name) class Name##Ref;
HEAP_BROKER_OBJECT_LIST(FORWARD_DECL)
#undef FORWARD_DECL

class ObjectRef;

template <class T>
struct ref_traits;

#define FORWARD_DECL(Name) class Name##Data;
HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(FORWARD_DECL)
#undef FORWARD_DECL

#define BACKGROUND_SERIALIZED_REF_TRAITS(Name)                     \
  template <>                                                      \
  struct ref_traits<Name> {                                        \
    using ref_type = Name##Ref;                                    \
    using data_type = Name##Data;                                  \
    static constexpr RefSerializationKind ref_serialization_kind = \
        RefSerializationKind::kBackgroundSerialized;               \
  };

#define NEVER_SERIALIZED_REF_TRAITS(Name)                          \
  template <>                                                      \
  struct ref_traits<Name> {                                        \
    using ref_type = Name##Ref;                                    \
    using data_type = ObjectData;                                  \
    static constexpr RefSerializationKind ref_serialization_kind = \
        RefSerializationKind::kNeverSerialized;                    \
  };

HEAP_BROKER_OBJECT_LIST_BASE(BACKGROUND_SERIALIZED_REF_TRAITS,
                             NEVER_SERIALIZED_REF_TRAITS)
#undef NEVER_SERIALIZED_REF_TRAITS
#undef BACKGROUND_SERIALIZED_REF_TRAITS

template <>
struct ref_traits<Object> {
  using ref_type = ObjectRef;
  // Note: While a bit awkward, this artificial ref serialization kind value is
  // okay: smis are never-serialized, and we never create raw non-smi
  // ObjectRefs (they would at least be HeapObjectRefs instead).
  static constexpr RefSerializationKind ref_serialization_kind =
      RefSerializationKind::kNeverSerialized;
};

// A ref without the broker_ field, used when storage size is important.
template <class T>
class TinyRef {
 private:
  using RefType = typename ref_traits<T>::ref_type;

 public:
  explicit TinyRef(const RefType& ref) : TinyRef(ref.data_) {}
  RefType AsRef(JSHeapBroker* broker) const;
  static base::Optional<RefType> AsOptionalRef(JSHeapBroker* broker,
                                               base::Optional<TinyRef<T>> ref) {
    if (!ref.has_value()) return {};
    return ref->AsRef(broker);
  }
  Handle<T> object() const;

 private:
  explicit TinyRef(ObjectData* data) : data_(data) { DCHECK_NOT_NULL(data); }
  ObjectData* const data_;
};

#define V(Name) using Name##TinyRef = TinyRef<Name>;
HEAP_BROKER_OBJECT_LIST(V)
#undef V

#ifdef V8_EXTERNAL_CODE_SPACE
using CodeTRef = CodeDataContainerRef;
using CodeTTinyRef = CodeDataContainerTinyRef;
#else
using CodeTRef = CodeRef;
using CodeTTinyRef = CodeTinyRef;
#endif

class V8_EXPORT_PRIVATE ObjectRef {
 public:
  ObjectRef(JSHeapBroker* broker, ObjectData* data, bool check_type = true)
      : data_(data), broker_(broker) {
    CHECK_NOT_NULL(data_);
  }

  Handle<Object> object() const;

  bool equals(const ObjectRef& other) const;

  bool IsSmi() const;
  int AsSmi() const;

#define HEAP_IS_METHOD_DECL(Name) bool Is##Name() const;
  HEAP_BROKER_OBJECT_LIST(HEAP_IS_METHOD_DECL)
#undef HEAP_IS_METHOD_DECL

#define HEAP_AS_METHOD_DECL(Name) Name##Ref As##Name() const;
  HEAP_BROKER_OBJECT_LIST(HEAP_AS_METHOD_DECL)
#undef HEAP_AS_METHOD_DECL

  bool IsNull() const;
  bool IsNullOrUndefined() const;
  bool IsTheHole() const;

  base::Optional<bool> TryGetBooleanValue() const;
  Maybe<double> OddballToNumber() const;

  bool should_access_heap() const;

  Isolate* isolate() const;

  struct Hash {
    size_t operator()(const ObjectRef& ref) const {
      return base::hash_combine(ref.object().address());
    }
  };
  struct Equal {
    bool operator()(const ObjectRef& lhs, const ObjectRef& rhs) const {
      return lhs.equals(rhs);
    }
  };

 protected:
  JSHeapBroker* broker() const;
  ObjectData* data() const;
  ObjectData* data_;  // Should be used only by object() getters.

 private:
  friend class FunctionTemplateInfoRef;
  friend class JSArrayData;
  friend class JSFunctionData;
  friend class JSGlobalObjectData;
  friend class JSGlobalObjectRef;
  friend class JSHeapBroker;
  friend class JSObjectData;
  friend class StringData;
  template <class T>
  friend class TinyRef;

  friend std::ostream& operator<<(std::ostream& os, const ObjectRef& ref);

  JSHeapBroker* broker_;
};

template <class T>
using ZoneRefUnorderedSet =
    ZoneUnorderedSet<T, ObjectRef::Hash, ObjectRef::Equal>;

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

  using Flags = base::Flags<Flag>;

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

// Constructors are carefully defined such that we do a type check on
// the outermost Ref class in the inheritance chain only.
#define DEFINE_REF_CONSTRUCTOR(Name, Base)                                  \
  Name##Ref(JSHeapBroker* broker, ObjectData* data, bool check_type = true) \
      : Base(broker, data, false) {                                         \
    if (check_type) {                                                       \
      CHECK(Is##Name());                                                    \
    }                                                                       \
  }

class HeapObjectRef : public ObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(HeapObject, ObjectRef)

  Handle<HeapObject> object() const;

  MapRef map() const;

  // Only for use in special situations where we need to read the object's
  // current map (instead of returning the cached map). Use with care.
  base::Optional<MapRef> map_direct_read() const;

  // See the comment on the HeapObjectType class.
  HeapObjectType GetHeapObjectType() const;
};

class PropertyCellRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(PropertyCell, HeapObjectRef)

  Handle<PropertyCell> object() const;

  V8_WARN_UNUSED_RESULT bool Cache() const;
  void CacheAsProtector() const {
    bool cached = Cache();
    // A protector always holds a Smi value and its cell type never changes, so
    // Cache can't fail.
    CHECK(cached);
  }

  PropertyDetails property_details() const;
  ObjectRef value() const;
};

class JSReceiverRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSReceiver, HeapObjectRef)

  Handle<JSReceiver> object() const;
};

class JSObjectRef : public JSReceiverRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSObject, JSReceiverRef)

  Handle<JSObject> object() const;

  base::Optional<ObjectRef> raw_properties_or_hash() const;

  // Usable only for in-object properties. Only use this if the underlying
  // value can be an uninitialized-sentinel, or if HeapNumber construction must
  // be avoided for some reason. Otherwise, use the higher-level
  // GetOwnFastDataProperty.
  base::Optional<ObjectRef> RawInobjectPropertyAt(FieldIndex index) const;

  // Return the element at key {index} if {index} is known to be an own data
  // property of the object that is non-writable and non-configurable. If
  // {dependencies} is non-null, a dependency will be taken to protect
  // against inconsistency due to weak memory concurrency.
  base::Optional<ObjectRef> GetOwnConstantElement(
      const FixedArrayBaseRef& elements_ref, uint32_t index,
      CompilationDependencies* dependencies) const;
  // The direct-read implementation of the above, extracted into a helper since
  // it's also called from compilation-dependency validation. This helper is
  // guaranteed to not create new Ref instances.
  base::Optional<Object> GetOwnConstantElementFromHeap(
      FixedArrayBase elements, ElementsKind elements_kind,
      uint32_t index) const;

  // Return the value of the property identified by the field {index}
  // if {index} is known to be an own data property of the object.
  // If {dependencies} is non-null, and a property was successfully read,
  // then the function will take a dependency to check the value of the
  // property at code finalization time.
  base::Optional<ObjectRef> GetOwnFastDataProperty(
      Representation field_representation, FieldIndex index,
      CompilationDependencies* dependencies) const;

  // Return the value of the dictionary property at {index} in the dictionary
  // if {index} is known to be an own data property of the object.
  base::Optional<ObjectRef> GetOwnDictionaryProperty(
      InternalIndex index, CompilationDependencies* dependencies) const;

  // When concurrent inlining is enabled, reads the elements through a direct
  // relaxed read. This is to ease the transition to unserialized (or
  // background-serialized) elements.
  base::Optional<FixedArrayBaseRef> elements(RelaxedLoadTag) const;
  bool IsElementsTenured(const FixedArrayBaseRef& elements);

  base::Optional<MapRef> GetObjectCreateMap() const;
};

class JSDataViewRef : public JSObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSDataView, JSObjectRef)

  Handle<JSDataView> object() const;

  size_t byte_length() const;
};

class JSBoundFunctionRef : public JSObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSBoundFunction, JSObjectRef)

  Handle<JSBoundFunction> object() const;

  JSReceiverRef bound_target_function() const;
  ObjectRef bound_this() const;
  FixedArrayRef bound_arguments() const;
};

class V8_EXPORT_PRIVATE JSFunctionRef : public JSObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSFunction, JSObjectRef)

  Handle<JSFunction> object() const;

  // Returns true, iff the serialized JSFunctionData contents are consistent
  // with the state of the underlying JSFunction object. Must be called from
  // the main thread.
  bool IsConsistentWithHeapState() const;

  ContextRef context() const;
  NativeContextRef native_context() const;
  SharedFunctionInfoRef shared() const;
  CodeRef code() const;

  bool has_initial_map(CompilationDependencies* dependencies) const;
  bool PrototypeRequiresRuntimeLookup(
      CompilationDependencies* dependencies) const;
  bool has_instance_prototype(CompilationDependencies* dependencies) const;
  ObjectRef instance_prototype(CompilationDependencies* dependencies) const;
  MapRef initial_map(CompilationDependencies* dependencies) const;
  int InitialMapInstanceSizeWithMinSlack(
      CompilationDependencies* dependencies) const;
  FeedbackCellRef raw_feedback_cell(
      CompilationDependencies* dependencies) const;
  base::Optional<FeedbackVectorRef> feedback_vector(
      CompilationDependencies* dependencies) const;
};

class RegExpBoilerplateDescriptionRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(RegExpBoilerplateDescription, HeapObjectRef)

  Handle<RegExpBoilerplateDescription> object() const;

  FixedArrayRef data() const;
  StringRef source() const;
  int flags() const;
};

// HeapNumberRef is only created for immutable HeapNumbers. Mutable
// HeapNumbers (those owned by in-object or backing store fields with
// representation type Double are not exposed to the compiler through
// HeapNumberRef. Instead, we read their value, and protect that read
// with a field-constness Dependency.
class HeapNumberRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(HeapNumber, HeapObjectRef)

  Handle<HeapNumber> object() const;

  double value() const;
  uint64_t value_as_bits() const;
};

class ContextRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(Context, HeapObjectRef)

  Handle<Context> object() const;

  // {previous} decrements {depth} by 1 for each previous link successfully
  // followed. If {depth} != 0 on function return, then it only got partway to
  // the desired depth.
  ContextRef previous(size_t* depth) const;

  // Only returns a value if the index is valid for this ContextRef.
  base::Optional<ObjectRef> get(int index) const;
};

#define BROKER_NATIVE_CONTEXT_FIELDS(V)          \
  V(JSFunction, array_function)                  \
  V(JSFunction, bigint_function)                 \
  V(JSFunction, boolean_function)                \
  V(JSFunction, function_prototype_apply)        \
  V(JSFunction, number_function)                 \
  V(JSFunction, object_function)                 \
  V(JSFunction, promise_function)                \
  V(JSFunction, promise_then)                    \
  V(JSFunction, regexp_exec_function)            \
  V(JSFunction, regexp_function)                 \
  V(JSFunction, string_function)                 \
  V(JSFunction, symbol_function)                 \
  V(JSGlobalObject, global_object)               \
  V(JSGlobalProxy, global_proxy_object)          \
  V(JSObject, initial_array_prototype)           \
  V(JSObject, promise_prototype)                 \
  V(Map, async_function_object_map)              \
  V(Map, block_context_map)                      \
  V(Map, bound_function_with_constructor_map)    \
  V(Map, bound_function_without_constructor_map) \
  V(Map, catch_context_map)                      \
  V(Map, eval_context_map)                       \
  V(Map, fast_aliased_arguments_map)             \
  V(Map, function_context_map)                   \
  V(Map, initial_array_iterator_map)             \
  V(Map, initial_string_iterator_map)            \
  V(Map, iterator_result_map)                    \
  V(Map, js_array_holey_double_elements_map)     \
  V(Map, js_array_holey_elements_map)            \
  V(Map, js_array_holey_smi_elements_map)        \
  V(Map, js_array_packed_double_elements_map)    \
  V(Map, js_array_packed_elements_map)           \
  V(Map, js_array_packed_smi_elements_map)       \
  V(Map, map_key_iterator_map)                   \
  V(Map, map_key_value_iterator_map)             \
  V(Map, map_value_iterator_map)                 \
  V(Map, set_key_value_iterator_map)             \
  V(Map, set_value_iterator_map)                 \
  V(Map, sloppy_arguments_map)                   \
  V(Map, slow_object_with_null_prototype_map)    \
  V(Map, strict_arguments_map)                   \
  V(Map, with_context_map)                       \
  V(ScriptContextTable, script_context_table)

class NativeContextRef : public ContextRef {
 public:
  DEFINE_REF_CONSTRUCTOR(NativeContext, ContextRef)

  Handle<NativeContext> object() const;

#define DECL_ACCESSOR(type, name) type##Ref name() const;
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  ScopeInfoRef scope_info() const;
  MapRef GetFunctionMapFromIndex(int index) const;
  MapRef GetInitialJSArrayMap(ElementsKind kind) const;
  base::Optional<JSFunctionRef> GetConstructorFunction(const MapRef& map) const;
  bool GlobalIsDetached() const;
};

class NameRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(Name, HeapObjectRef)

  Handle<Name> object() const;

  bool IsUniqueName() const;
};

class DescriptorArrayRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(DescriptorArray, HeapObjectRef)

  Handle<DescriptorArray> object() const;

  PropertyDetails GetPropertyDetails(InternalIndex descriptor_index) const;
  NameRef GetPropertyKey(InternalIndex descriptor_index) const;
  base::Optional<ObjectRef> GetStrongValue(
      InternalIndex descriptor_index) const;
};

class FeedbackCellRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(FeedbackCell, HeapObjectRef)

  Handle<FeedbackCell> object() const;

  ObjectRef value() const;

  // Convenience wrappers around {value()}:
  base::Optional<FeedbackVectorRef> feedback_vector() const;
  base::Optional<SharedFunctionInfoRef> shared_function_info() const;
};

class FeedbackVectorRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(FeedbackVector, HeapObjectRef)

  Handle<FeedbackVector> object() const;

  SharedFunctionInfoRef shared_function_info() const;

  FeedbackCellRef GetClosureFeedbackCell(int index) const;
};

class CallHandlerInfoRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(CallHandlerInfo, HeapObjectRef)

  Handle<CallHandlerInfo> object() const;

  Address callback() const;
  ObjectRef data() const;
};

class AccessorInfoRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(AccessorInfo, HeapObjectRef)

  Handle<AccessorInfo> object() const;
};

class AllocationSiteRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(AllocationSite, HeapObjectRef)

  Handle<AllocationSite> object() const;

  bool PointsToLiteral() const;
  AllocationType GetAllocationType() const;
  ObjectRef nested_site() const;

  base::Optional<JSObjectRef> boilerplate() const;
  ElementsKind GetElementsKind() const;
  bool CanInlineCall() const;
};

class BigIntRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(BigInt, HeapObjectRef)

  Handle<BigInt> object() const;

  uint64_t AsUint64() const;
};

class V8_EXPORT_PRIVATE MapRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(Map, HeapObjectRef)

  Handle<Map> object() const;

  int instance_size() const;
  InstanceType instance_type() const;
  int GetInObjectProperties() const;
  int GetInObjectPropertiesStartInWords() const;
  int NumberOfOwnDescriptors() const;
  int GetInObjectPropertyOffset(int index) const;
  int constructor_function_index() const;
  int NextFreePropertyIndex() const;
  int UnusedPropertyFields() const;
  ElementsKind elements_kind() const;
  bool is_stable() const;
  bool is_constructor() const;
  bool has_prototype_slot() const;
  bool is_access_check_needed() const;
  bool is_deprecated() const;
  bool CanBeDeprecated() const;
  bool CanTransition() const;
  bool IsInobjectSlackTrackingInProgress() const;
  bool is_dictionary_map() const;
  bool IsFixedCowArrayMap() const;
  bool IsPrimitiveMap() const;
  bool is_undetectable() const;
  bool is_callable() const;
  bool has_indexed_interceptor() const;
  bool is_migration_target() const;
  bool supports_fast_array_iteration() const;
  bool supports_fast_array_resize() const;
  bool is_abandoned_prototype_map() const;

  OddballType oddball_type() const;

  bool CanInlineElementAccess() const;

  // Note: Only returns a value if the requested elements kind matches the
  // current kind, or if the current map is an unmodified JSArray initial map.
  base::Optional<MapRef> AsElementsKind(ElementsKind kind) const;

#define DEF_TESTER(Type, ...) bool Is##Type##Map() const;
  INSTANCE_TYPE_CHECKERS(DEF_TESTER)
#undef DEF_TESTER

  HeapObjectRef GetBackPointer() const;

  HeapObjectRef prototype() const;

  bool HasOnlyStablePrototypesWithFastElements(
      ZoneVector<MapRef>* prototype_maps);

  // Concerning the underlying instance_descriptors:
  DescriptorArrayRef instance_descriptors() const;
  MapRef FindFieldOwner(InternalIndex descriptor_index) const;
  PropertyDetails GetPropertyDetails(InternalIndex descriptor_index) const;
  NameRef GetPropertyKey(InternalIndex descriptor_index) const;
  FieldIndex GetFieldIndexFor(InternalIndex descriptor_index) const;
  base::Optional<ObjectRef> GetStrongValue(
      InternalIndex descriptor_number) const;

  MapRef FindRootMap() const;
  ObjectRef GetConstructor() const;
};

struct HolderLookupResult {
  HolderLookupResult(CallOptimization::HolderLookup lookup_ =
                         CallOptimization::kHolderNotFound,
                     base::Optional<JSObjectRef> holder_ = base::nullopt)
      : lookup(lookup_), holder(holder_) {}
  CallOptimization::HolderLookup lookup;
  base::Optional<JSObjectRef> holder;
};

class FunctionTemplateInfoRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(FunctionTemplateInfo, HeapObjectRef)

  Handle<FunctionTemplateInfo> object() const;

  bool is_signature_undefined() const;
  bool accept_any_receiver() const;
  base::Optional<CallHandlerInfoRef> call_code() const;
  ZoneVector<Address> c_functions() const;
  ZoneVector<const CFunctionInfo*> c_signatures() const;
  HolderLookupResult LookupHolderOfExpectedType(MapRef receiver_map);
};

class FixedArrayBaseRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(FixedArrayBase, HeapObjectRef)

  Handle<FixedArrayBase> object() const;

  int length() const;
};

class ArrayBoilerplateDescriptionRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<ArrayBoilerplateDescription> object() const;

  int constants_elements_length() const;
};

class FixedArrayRef : public FixedArrayBaseRef {
 public:
  DEFINE_REF_CONSTRUCTOR(FixedArray, FixedArrayBaseRef)

  Handle<FixedArray> object() const;

  base::Optional<ObjectRef> TryGet(int i) const;
};

class FixedDoubleArrayRef : public FixedArrayBaseRef {
 public:
  DEFINE_REF_CONSTRUCTOR(FixedDoubleArray, FixedArrayBaseRef)

  Handle<FixedDoubleArray> object() const;

  // Due to 64-bit unaligned reads, only usable for
  // immutable-after-initialization FixedDoubleArrays protected by
  // acquire-release semantics (such as boilerplate elements).
  Float64 GetFromImmutableFixedDoubleArray(int i) const;
};

class BytecodeArrayRef : public FixedArrayBaseRef {
 public:
  DEFINE_REF_CONSTRUCTOR(BytecodeArray, FixedArrayBaseRef)

  Handle<BytecodeArray> object() const;

  // NOTE: Concurrent reads of the actual bytecodes as well as the constant pool
  // (both immutable) do not go through BytecodeArrayRef but are performed
  // directly through the handle by BytecodeArrayIterator.

  int register_count() const;
  int parameter_count() const;
  interpreter::Register incoming_new_target_or_generator_register() const;

  Handle<ByteArray> SourcePositionTable() const;

  // Exception handler table.
  Address handler_table_address() const;
  int handler_table_size() const;
};

class ScriptContextTableRef : public FixedArrayRef {
 public:
  DEFINE_REF_CONSTRUCTOR(ScriptContextTable, FixedArrayRef)

  Handle<ScriptContextTable> object() const;
};

class ObjectBoilerplateDescriptionRef : public FixedArrayRef {
 public:
  DEFINE_REF_CONSTRUCTOR(ObjectBoilerplateDescription, FixedArrayRef)

  Handle<ObjectBoilerplateDescription> object() const;

  int size() const;
};

class JSArrayRef : public JSObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSArray, JSObjectRef)

  Handle<JSArray> object() const;

  // The `length` property of boilerplate JSArray objects. Boilerplates are
  // immutable after initialization. Must not be used for non-boilerplate
  // JSArrays.
  ObjectRef GetBoilerplateLength() const;

  // Return the element at key {index} if the array has a copy-on-write elements
  // storage and {index} is known to be an own data property.
  // Note the value returned by this function is only valid if we ensure at
  // runtime that the backing store has not changed.
  base::Optional<ObjectRef> GetOwnCowElement(FixedArrayBaseRef elements_ref,
                                             uint32_t index) const;

  // The `JSArray::length` property; not safe to use in general, but can be
  // used in some special cases that guarantee a valid `length` value despite
  // concurrent reads. The result needs to be optional in case the
  // return value was created too recently to pass the gc predicate.
  base::Optional<ObjectRef> length_unsafe() const;
};

class ScopeInfoRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(ScopeInfo, HeapObjectRef)

  Handle<ScopeInfo> object() const;

  int ContextLength() const;
  bool HasOuterScopeInfo() const;
  bool HasContextExtensionSlot() const;

  ScopeInfoRef OuterScopeInfo() const;
};

#define BROKER_SFI_FIELDS(V)                               \
  V(int, internal_formal_parameter_count_without_receiver) \
  V(bool, IsDontAdaptArguments)                            \
  V(bool, has_simple_parameters)                           \
  V(bool, has_duplicate_parameters)                        \
  V(int, function_map_index)                               \
  V(FunctionKind, kind)                                    \
  V(LanguageMode, language_mode)                           \
  V(bool, native)                                          \
  V(bool, HasBreakInfo)                                    \
  V(bool, HasBuiltinId)                                    \
  V(bool, construct_as_builtin)                            \
  V(bool, HasBytecodeArray)                                \
  V(int, StartPosition)                                    \
  V(bool, is_compiled)                                     \
  V(bool, IsUserJavaScript)                                \
  IF_WASM(V, const wasm::WasmModule*, wasm_module)         \
  IF_WASM(V, const wasm::FunctionSig*, wasm_function_signature)

class V8_EXPORT_PRIVATE SharedFunctionInfoRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(SharedFunctionInfo, HeapObjectRef)

  Handle<SharedFunctionInfo> object() const;

  Builtin builtin_id() const;
  int context_header_size() const;
  int context_parameters_start() const;
  BytecodeArrayRef GetBytecodeArray() const;
  SharedFunctionInfo::Inlineability GetInlineability() const;
  base::Optional<FunctionTemplateInfoRef> function_template_info() const;
  ScopeInfoRef scope_info() const;

#define DECL_ACCESSOR(type, name) type name() const;
  BROKER_SFI_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  bool IsInlineable() const {
    return GetInlineability() == SharedFunctionInfo::kIsInlineable;
  }
};

class StringRef : public NameRef {
 public:
  DEFINE_REF_CONSTRUCTOR(String, NameRef)

  Handle<String> object() const;

  // With concurrent inlining on, we return base::nullopt due to not being able
  // to use LookupIterator in a thread-safe way.
  base::Optional<ObjectRef> GetCharAsStringOrUndefined(uint32_t index) const;

  // When concurrently accessing non-read-only non-supported strings, we return
  // base::nullopt for these methods.
  base::Optional<Handle<String>> ObjectIfContentAccessible();
  base::Optional<int> length() const;
  base::Optional<uint16_t> GetFirstChar() const;
  base::Optional<uint16_t> GetChar(int index) const;
  base::Optional<double> ToNumber();

  bool IsSeqString() const;
  bool IsExternalString() const;

 private:
  // With concurrent inlining on, we currently support reading directly
  // internalized strings, and thin strings (which are pointers to internalized
  // strings).
  bool SupportedStringKind() const;
};

class SymbolRef : public NameRef {
 public:
  DEFINE_REF_CONSTRUCTOR(Symbol, NameRef)

  Handle<Symbol> object() const;
};

class JSTypedArrayRef : public JSObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSTypedArray, JSObjectRef)

  Handle<JSTypedArray> object() const;

  bool is_on_heap() const;
  size_t length() const;
  void* data_ptr() const;
  HeapObjectRef buffer() const;
};

class SourceTextModuleRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(SourceTextModule, HeapObjectRef)

  Handle<SourceTextModule> object() const;

  base::Optional<CellRef> GetCell(int cell_index) const;
  base::Optional<ObjectRef> import_meta() const;
};

class TemplateObjectDescriptionRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(TemplateObjectDescription, HeapObjectRef)

  Handle<TemplateObjectDescription> object() const;
};

class CellRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(Cell, HeapObjectRef)

  Handle<Cell> object() const;
};

class JSGlobalObjectRef : public JSObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSGlobalObject, JSObjectRef)

  Handle<JSGlobalObject> object() const;

  bool IsDetachedFrom(JSGlobalProxyRef const& proxy) const;

  // Can be called even when there is no property cell for the given name.
  base::Optional<PropertyCellRef> GetPropertyCell(NameRef const& name) const;
};

class JSGlobalProxyRef : public JSObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(JSGlobalProxy, JSObjectRef)

  Handle<JSGlobalProxy> object() const;
};

class CodeRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(Code, HeapObjectRef)

  Handle<Code> object() const;

  unsigned GetInlinedBytecodeSize() const;
};

// CodeDataContainerRef doesn't appear to be used directly, but it is used via
// CodeTRef when V8_EXTERNAL_CODE_SPACE is enabled.
class CodeDataContainerRef : public HeapObjectRef {
 public:
  DEFINE_REF_CONSTRUCTOR(CodeDataContainer, HeapObjectRef)

  Handle<CodeDataContainer> object() const;
};

class InternalizedStringRef : public StringRef {
 public:
  DEFINE_REF_CONSTRUCTOR(InternalizedString, StringRef)

  Handle<InternalizedString> object() const;
};

#undef DEFINE_REF_CONSTRUCTOR

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_HEAP_REFS_H_
