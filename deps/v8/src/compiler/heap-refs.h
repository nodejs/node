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

namespace v8 {
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
class JSRegExp;
class JSTypedArray;
class NativeContext;
class ScriptContextTable;
class VectorSlotPair;

namespace compiler {

// Whether we are loading a property or storing to a property.
// For a store during literal creation, do not walk up the prototype chain.
enum class AccessMode { kLoad, kStore, kStoreInLiteral, kHas };

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
  V(JSBoundFunction)               \
  V(JSDataView)                    \
  V(JSFunction)                    \
  V(JSGlobalProxy)                 \
  V(JSRegExp)                      \
  V(JSTypedArray)                  \
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
  V(Symbol)                        \
  /* Subtypes of HeapObject */     \
  V(AllocationSite)                \
  V(BigInt)                        \
  V(CallHandlerInfo)               \
  V(Cell)                          \
  V(Code)                          \
  V(DescriptorArray)               \
  V(FeedbackCell)                  \
  V(FeedbackVector)                \
  V(FixedArrayBase)                \
  V(FunctionTemplateInfo)          \
  V(HeapNumber)                    \
  V(JSObject)                      \
  V(Map)                           \
  V(MutableHeapNumber)             \
  V(Name)                          \
  V(PropertyCell)                  \
  V(SharedFunctionInfo)            \
  V(SourceTextModule)              \
  /* Subtypes of Object */         \
  V(HeapObject)

class CompilationDependencies;
class JSHeapBroker;
class ObjectData;
class PerIsolateCompilerCache;
class PropertyAccessInfo;
#define FORWARD_DECL(Name) class Name##Ref;
HEAP_BROKER_OBJECT_LIST(FORWARD_DECL)
#undef FORWARD_DECL

class V8_EXPORT_PRIVATE ObjectRef {
 public:
  ObjectRef(JSHeapBroker* broker, Handle<Object> object);
  ObjectRef(JSHeapBroker* broker, ObjectData* data)
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

  bool IsNullOrUndefined() const;

  bool BooleanValue() const;
  Maybe<double> OddballToNumber() const;

  // Return the element at key {index} if {index} is known to be an own data
  // property of the object that is non-writable and non-configurable.
  base::Optional<ObjectRef> GetOwnConstantElement(uint32_t index,
                                                  bool serialize = false) const;

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
  friend class JSGlobalProxyRef;
  friend class JSGlobalProxyData;
  friend class JSObjectData;
  friend class StringData;

  friend std::ostream& operator<<(std::ostream& os, const ObjectRef& ref);

  JSHeapBroker* broker_;
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

class HeapObjectRef : public ObjectRef {
 public:
  using ObjectRef::ObjectRef;
  Handle<HeapObject> object() const;

  MapRef map() const;

  // See the comment on the HeapObjectType class.
  HeapObjectType GetHeapObjectType() const;
};

class PropertyCellRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<PropertyCell> object() const;

  PropertyDetails property_details() const;

  void Serialize();
  ObjectRef value() const;
};

class JSObjectRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<JSObject> object() const;

  uint64_t RawFastDoublePropertyAsBitsAt(FieldIndex index) const;
  double RawFastDoublePropertyAt(FieldIndex index) const;
  ObjectRef RawFastPropertyAt(FieldIndex index) const;

  // Return the value of the property identified by the field {index}
  // if {index} is known to be an own data property of the object.
  base::Optional<ObjectRef> GetOwnProperty(Representation field_representation,
                                           FieldIndex index,
                                           bool serialize = false) const;

  FixedArrayBaseRef elements() const;
  void SerializeElements();
  void EnsureElementsTenured();
  ElementsKind GetElementsKind() const;

  void SerializeObjectCreateMap();
  base::Optional<MapRef> GetObjectCreateMap() const;
};

class JSDataViewRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
  Handle<JSDataView> object() const;

  size_t byte_length() const;
  size_t byte_offset() const;
};

class JSBoundFunctionRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
  Handle<JSBoundFunction> object() const;

  void Serialize();

  // The following are available only after calling Serialize().
  ObjectRef bound_target_function() const;
  ObjectRef bound_this() const;
  FixedArrayRef bound_arguments() const;
};

class V8_EXPORT_PRIVATE JSFunctionRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
  Handle<JSFunction> object() const;

  bool has_feedback_vector() const;
  bool has_initial_map() const;
  bool has_prototype() const;
  bool PrototypeRequiresRuntimeLookup() const;

  void Serialize();
  bool serialized() const;

  // The following are available only after calling Serialize().
  ObjectRef prototype() const;
  MapRef initial_map() const;
  ContextRef context() const;
  NativeContextRef native_context() const;
  SharedFunctionInfoRef shared() const;
  FeedbackVectorRef feedback_vector() const;
  int InitialMapInstanceSizeWithMinSlack() const;

  bool IsSerializedForCompilation() const;
};

class JSRegExpRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
  Handle<JSRegExp> object() const;

  ObjectRef raw_properties_or_hash() const;
  ObjectRef data() const;
  ObjectRef source() const;
  ObjectRef flags() const;
  ObjectRef last_index() const;
};

class HeapNumberRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<HeapNumber> object() const;

  double value() const;
};

class MutableHeapNumberRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<MutableHeapNumber> object() const;

  double value() const;
};

class ContextRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<Context> object() const;

  // {previous} decrements {depth} by 1 for each previous link successfully
  // followed. If {depth} != 0 on function return, then it only got
  // partway to the desired depth. If {serialize} is true, then
  // {previous} will cache its findings.
  ContextRef previous(size_t* depth, bool serialize = false) const;

  // Only returns a value if the index is valid for this ContextRef.
  base::Optional<ObjectRef> get(int index, bool serialize = false) const;

  // We only serialize the ScopeInfo if certain Promise
  // builtins are called.
  void SerializeScopeInfo();
  base::Optional<ScopeInfoRef> scope_info() const;
};

#define BROKER_COMPULSORY_NATIVE_CONTEXT_FIELDS(V)                    \
  V(JSFunction, array_function)                                       \
  V(JSFunction, boolean_function)                                     \
  V(JSFunction, bigint_function)                                      \
  V(JSFunction, number_function)                                      \
  V(JSFunction, object_function)                                      \
  V(JSFunction, promise_function)                                     \
  V(JSFunction, promise_then)                                         \
  V(JSFunction, string_function)                                      \
  V(JSFunction, symbol_function)                                      \
  V(JSGlobalProxy, global_proxy_object)                               \
  V(JSObject, promise_prototype)                                      \
  V(Map, bound_function_with_constructor_map)                         \
  V(Map, bound_function_without_constructor_map)                      \
  V(Map, fast_aliased_arguments_map)                                  \
  V(Map, initial_array_iterator_map)                                  \
  V(Map, initial_string_iterator_map)                                 \
  V(Map, iterator_result_map)                                         \
  V(Map, js_array_holey_double_elements_map)                          \
  V(Map, js_array_holey_elements_map)                                 \
  V(Map, js_array_holey_smi_elements_map)                             \
  V(Map, js_array_packed_double_elements_map)                         \
  V(Map, js_array_packed_elements_map)                                \
  V(Map, js_array_packed_smi_elements_map)                            \
  V(Map, sloppy_arguments_map)                                        \
  V(Map, slow_object_with_null_prototype_map)                         \
  V(Map, strict_arguments_map)                                        \
  V(ScriptContextTable, script_context_table)                         \
  V(SharedFunctionInfo, promise_capability_default_reject_shared_fun) \
  V(SharedFunctionInfo, promise_catch_finally_shared_fun)             \
  V(SharedFunctionInfo, promise_then_finally_shared_fun)              \
  V(SharedFunctionInfo, promise_capability_default_resolve_shared_fun)

// Those are set by Bootstrapper::ExportFromRuntime, which may not yet have
// happened when Turbofan is invoked via --always-opt.
#define BROKER_OPTIONAL_NATIVE_CONTEXT_FIELDS(V) \
  V(Map, async_function_object_map)              \
  V(Map, map_key_iterator_map)                   \
  V(Map, map_key_value_iterator_map)             \
  V(Map, map_value_iterator_map)                 \
  V(JSFunction, regexp_exec_function)            \
  V(Map, set_key_value_iterator_map)             \
  V(Map, set_value_iterator_map)

#define BROKER_NATIVE_CONTEXT_FIELDS(V)      \
  BROKER_COMPULSORY_NATIVE_CONTEXT_FIELDS(V) \
  BROKER_OPTIONAL_NATIVE_CONTEXT_FIELDS(V)

class NativeContextRef : public ContextRef {
 public:
  using ContextRef::ContextRef;
  Handle<NativeContext> object() const;

  void Serialize();

#define DECL_ACCESSOR(type, name) type##Ref name() const;
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  ScopeInfoRef scope_info() const;
  MapRef GetFunctionMapFromIndex(int index) const;
  MapRef GetInitialJSArrayMap(ElementsKind kind) const;
  base::Optional<JSFunctionRef> GetConstructorFunction(const MapRef& map) const;
};

class NameRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<Name> object() const;

  bool IsUniqueName() const;
};

class ScriptContextTableRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<ScriptContextTable> object() const;

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
  Handle<DescriptorArray> object() const;
};

class FeedbackCellRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<FeedbackCell> object() const;

  HeapObjectRef value() const;
};

class FeedbackVectorRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<FeedbackVector> object() const;

  ObjectRef get(FeedbackSlot slot) const;

  void SerializeSlots();
};

class CallHandlerInfoRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<CallHandlerInfo> object() const;

  Address callback() const;

  void Serialize();
  ObjectRef data() const;
};

class AllocationSiteRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<AllocationSite> object() const;

  bool PointsToLiteral() const;
  AllocationType GetAllocationType() const;
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

class BigIntRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<BigInt> object() const;

  uint64_t AsUint64() const;
};

class V8_EXPORT_PRIVATE MapRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
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
  bool is_extensible() const;
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
  bool IsMapOfCurrentGlobalProxy() const;

  OddballType oddball_type() const;

#define DEF_TESTER(Type, ...) bool Is##Type##Map() const;
  INSTANCE_TYPE_CHECKERS(DEF_TESTER)
#undef DEF_TESTER

  void SerializeBackPointer();
  HeapObjectRef GetBackPointer() const;

  void SerializePrototype();
  bool serialized_prototype() const;
  HeapObjectRef prototype() const;

  void SerializeForElementLoad();

  void SerializeForElementStore();
  bool HasOnlyStablePrototypesWithFastElements(
      ZoneVector<MapRef>* prototype_maps);

  // Concerning the underlying instance_descriptors:
  void SerializeOwnDescriptors();
  void SerializeOwnDescriptor(int descriptor_index);
  MapRef FindFieldOwner(int descriptor_index) const;
  PropertyDetails GetPropertyDetails(int descriptor_index) const;
  NameRef GetPropertyKey(int descriptor_index) const;
  FieldIndex GetFieldIndexFor(int descriptor_index) const;
  ObjectRef GetFieldType(int descriptor_index) const;
  bool IsUnboxedDoubleField(int descriptor_index) const;

  // Available after calling JSFunctionRef::Serialize on a function that has
  // this map as initial map.
  ObjectRef GetConstructor() const;
  base::Optional<MapRef> AsElementsKind(ElementsKind kind) const;
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
  using HeapObjectRef::HeapObjectRef;
  Handle<FunctionTemplateInfo> object() const;

  bool is_signature_undefined() const;
  bool accept_any_receiver() const;
  // The following returns true if the CallHandlerInfo is present.
  bool has_call_code() const;

  void SerializeCallCode();
  base::Optional<CallHandlerInfoRef> call_code() const;

  HolderLookupResult LookupHolderOfExpectedType(MapRef receiver_map,
                                                bool serialize);
};

class FixedArrayBaseRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<FixedArrayBase> object() const;

  int length() const;
};

class FixedArrayRef : public FixedArrayBaseRef {
 public:
  using FixedArrayBaseRef::FixedArrayBaseRef;
  Handle<FixedArray> object() const;

  ObjectRef get(int i) const;
};

class FixedDoubleArrayRef : public FixedArrayBaseRef {
 public:
  using FixedArrayBaseRef::FixedArrayBaseRef;
  Handle<FixedDoubleArray> object() const;

  double get_scalar(int i) const;
  bool is_the_hole(int i) const;
};

class BytecodeArrayRef : public FixedArrayBaseRef {
 public:
  using FixedArrayBaseRef::FixedArrayBaseRef;
  Handle<BytecodeArray> object() const;

  int register_count() const;
  int parameter_count() const;
  interpreter::Register incoming_new_target_or_generator_register() const;

  // Bytecode access methods.
  uint8_t get(int index) const;
  Address GetFirstBytecodeAddress() const;

  // Source position table.
  const byte* source_positions_address() const;
  int source_positions_size() const;

  // Constant pool access.
  Handle<Object> GetConstantAtIndex(int index) const;
  bool IsConstantAtIndexSmi(int index) const;
  Smi GetConstantAtIndexAsSmi(int index) const;

  // Exception handler table.
  Address handler_table_address() const;
  int handler_table_size() const;

  bool IsSerializedForCompilation() const;
  void SerializeForCompilation();
};

class JSArrayRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
  Handle<JSArray> object() const;

  ObjectRef length() const;

  // Return the element at key {index} if the array has a copy-on-write elements
  // storage and {index} is known to be an own data property.
  base::Optional<ObjectRef> GetOwnCowElement(uint32_t index,
                                             bool serialize = false) const;
};

class ScopeInfoRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<ScopeInfo> object() const;

  int ContextLength() const;
};

#define BROKER_SFI_FIELDS(V)                 \
  V(int, internal_formal_parameter_count)    \
  V(bool, has_duplicate_parameters)          \
  V(int, function_map_index)                 \
  V(FunctionKind, kind)                      \
  V(LanguageMode, language_mode)             \
  V(bool, native)                            \
  V(bool, HasBreakInfo)                      \
  V(bool, HasBuiltinId)                      \
  V(bool, construct_as_builtin)              \
  V(bool, HasBytecodeArray)                  \
  V(bool, is_safe_to_skip_arguments_adaptor) \
  V(bool, IsInlineable)                      \
  V(int, StartPosition)                      \
  V(bool, is_compiled)

class V8_EXPORT_PRIVATE SharedFunctionInfoRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<SharedFunctionInfo> object() const;

  int builtin_id() const;
  BytecodeArrayRef GetBytecodeArray() const;

#define DECL_ACCESSOR(type, name) type name() const;
  BROKER_SFI_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  bool IsSerializedForCompilation(FeedbackVectorRef feedback) const;
  void SetSerializedForCompilation(FeedbackVectorRef feedback);

  // Template objects may not be created at compilation time. This method
  // wraps the retrieval of the template object and creates it if
  // necessary.
  JSArrayRef GetTemplateObject(ObjectRef description, FeedbackVectorRef vector,
                               FeedbackSlot slot, bool serialize = false);

  void SerializeFunctionTemplateInfo();
  base::Optional<FunctionTemplateInfoRef> function_template_info() const;
};

class StringRef : public NameRef {
 public:
  using NameRef::NameRef;
  Handle<String> object() const;

  int length() const;
  uint16_t GetFirstChar();
  base::Optional<double> ToNumber();
  bool IsSeqString() const;
  bool IsExternalString() const;
};

class SymbolRef : public NameRef {
 public:
  using NameRef::NameRef;
  Handle<Symbol> object() const;
};

class JSTypedArrayRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
  Handle<JSTypedArray> object() const;

  bool is_on_heap() const;
  size_t length() const;
  void* external_pointer() const;

  void Serialize();
  bool serialized() const;

  HeapObjectRef buffer() const;
};

class SourceTextModuleRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<SourceTextModule> object() const;

  void Serialize();

  CellRef GetCell(int cell_index) const;
};

class CellRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<Cell> object() const;

  ObjectRef value() const;
};

class JSGlobalProxyRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;
  Handle<JSGlobalProxy> object() const;

  // If {serialize} is false:
  //   If the property is known to exist as a property cell (on the global
  //   object), return that property cell. Otherwise (not known to exist as a
  //   property cell or known not to exist as a property cell) return nothing.
  // If {serialize} is true:
  //   Like above but potentially access the heap and serialize the necessary
  //   information.
  base::Optional<PropertyCellRef> GetPropertyCell(NameRef const& name,
                                                  bool serialize = false) const;
};

class CodeRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;
  Handle<Code> object() const;
};

class InternalizedStringRef : public StringRef {
 public:
  using StringRef::StringRef;
  Handle<InternalizedString> object() const;
};

class ElementAccessFeedback;
class NamedAccessFeedback;

class ProcessedFeedback : public ZoneObject {
 public:
  enum Kind { kInsufficient, kGlobalAccess, kNamedAccess, kElementAccess };
  Kind kind() const { return kind_; }

  ElementAccessFeedback const* AsElementAccess() const;
  NamedAccessFeedback const* AsNamedAccess() const;

 protected:
  explicit ProcessedFeedback(Kind kind) : kind_(kind) {}

 private:
  Kind const kind_;
};

class InsufficientFeedback final : public ProcessedFeedback {
 public:
  InsufficientFeedback();
};

class GlobalAccessFeedback : public ProcessedFeedback {
 public:
  explicit GlobalAccessFeedback(PropertyCellRef cell);
  GlobalAccessFeedback(ContextRef script_context, int slot_index,
                       bool immutable);

  bool IsPropertyCell() const;
  PropertyCellRef property_cell() const;

  bool IsScriptContextSlot() const { return !IsPropertyCell(); }
  ContextRef script_context() const;
  int slot_index() const;
  bool immutable() const;

  base::Optional<ObjectRef> GetConstantHint() const;

 private:
  ObjectRef const cell_or_context_;
  int const index_and_immutable_;
};

class KeyedAccessMode {
 public:
  static KeyedAccessMode FromNexus(FeedbackNexus const& nexus);

  AccessMode access_mode() const;
  bool IsLoad() const;
  bool IsStore() const;
  KeyedAccessLoadMode load_mode() const;
  KeyedAccessStoreMode store_mode() const;

 private:
  AccessMode const access_mode_;
  union LoadStoreMode {
    LoadStoreMode(KeyedAccessLoadMode load_mode);
    LoadStoreMode(KeyedAccessStoreMode store_mode);
    KeyedAccessLoadMode load_mode;
    KeyedAccessStoreMode store_mode;
  } const load_store_mode_;

  KeyedAccessMode(AccessMode access_mode, KeyedAccessLoadMode load_mode);
  KeyedAccessMode(AccessMode access_mode, KeyedAccessStoreMode store_mode);
};

class ElementAccessFeedback : public ProcessedFeedback {
 public:
  ElementAccessFeedback(Zone* zone, KeyedAccessMode const& keyed_mode);

  // No transition sources appear in {receiver_maps}.
  // All transition targets appear in {receiver_maps}.
  ZoneVector<Handle<Map>> receiver_maps;
  ZoneVector<std::pair<Handle<Map>, Handle<Map>>> transitions;

  KeyedAccessMode const keyed_mode;

  class MapIterator {
   public:
    bool done() const;
    void advance();
    MapRef current() const;

   private:
    friend class ElementAccessFeedback;

    explicit MapIterator(ElementAccessFeedback const& processed,
                         JSHeapBroker* broker);

    ElementAccessFeedback const& processed_;
    JSHeapBroker* const broker_;
    size_t index_ = 0;
  };

  // Iterator over all maps: first {receiver_maps}, then transition sources.
  MapIterator all_maps(JSHeapBroker* broker) const;
};

class NamedAccessFeedback : public ProcessedFeedback {
 public:
  NamedAccessFeedback(NameRef const& name,
                      ZoneVector<PropertyAccessInfo> const& access_infos);

  NameRef const& name() const { return name_; }
  ZoneVector<PropertyAccessInfo> const& access_infos() const {
    return access_infos_;
  }

 private:
  NameRef const name_;
  ZoneVector<PropertyAccessInfo> const access_infos_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_HEAP_REFS_H_
