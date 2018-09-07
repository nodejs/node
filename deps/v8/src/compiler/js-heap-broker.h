// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_HEAP_BROKER_H_
#define V8_COMPILER_JS_HEAP_BROKER_H_

#include "src/base/compiler-specific.h"
#include "src/base/optional.h"
#include "src/globals.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace compiler {

class DisallowHeapAccess {
  DisallowHeapAllocation no_heap_allocation_;
  DisallowHandleAllocation no_handle_allocation_;
  DisallowHandleDereference no_handle_dereference_;
  DisallowCodeDependencyChange no_dependency_change_;
};

enum class OddballType : uint8_t {
  kNone,     // Not an Oddball.
  kBoolean,  // True or False.
  kUndefined,
  kNull,
  kHole,
  kUninitialized,
  kOther  // Oddball, but none of the above.
};

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

#define HEAP_BROKER_OBJECT_LIST(V) \
  V(AllocationSite)                \
  V(Cell)                          \
  V(Code)                          \
  V(Context)                       \
  V(FeedbackVector)                \
  V(FixedArray)                    \
  V(FixedArrayBase)                \
  V(FixedDoubleArray)              \
  V(HeapNumber)                    \
  V(HeapObject)                    \
  V(InternalizedString)            \
  V(JSArray)                       \
  V(JSFunction)                    \
  V(JSGlobalProxy)                 \
  V(JSObject)                      \
  V(JSRegExp)                      \
  V(Map)                           \
  V(Module)                        \
  V(MutableHeapNumber)             \
  V(Name)                          \
  V(NativeContext)                 \
  V(PropertyCell)                  \
  V(ScopeInfo)                     \
  V(ScriptContextTable)            \
  V(SharedFunctionInfo)            \
  V(String)

class CompilationDependencies;
class JSHeapBroker;
#define FORWARD_DECL(Name) class Name##Ref;
HEAP_BROKER_OBJECT_LIST(FORWARD_DECL)
#undef FORWARD_DECL

class ObjectRef {
 public:
  explicit ObjectRef(const JSHeapBroker* broker, Handle<Object> object)
      : broker_(broker), object_(object) {}

  template <typename T>
  Handle<T> object() const {
    AllowHandleDereference handle_dereference;
    return Handle<T>::cast(object_);
  }

  OddballType oddball_type() const;

  bool IsSmi() const;
  int AsSmi() const;

  bool equals(const ObjectRef& other) const;

#define HEAP_IS_METHOD_DECL(Name) bool Is##Name() const;
  HEAP_BROKER_OBJECT_LIST(HEAP_IS_METHOD_DECL)
#undef HEAP_IS_METHOD_DECL

#define HEAP_AS_METHOD_DECL(Name) Name##Ref As##Name() const;
  HEAP_BROKER_OBJECT_LIST(HEAP_AS_METHOD_DECL)
#undef HEAP_AS_METHOD_DECL

  StringRef TypeOf() const;
  bool BooleanValue();
  double OddballToNumber() const;

 protected:
  const JSHeapBroker* broker() const { return broker_; }

 private:
  const JSHeapBroker* broker_;
  Handle<Object> object_;
};

class FieldTypeRef : public ObjectRef {
 public:
  using ObjectRef::ObjectRef;
};

class HeapObjectRef : public ObjectRef {
 public:
  using ObjectRef::ObjectRef;

  HeapObjectType type() const;
  MapRef map() const;
  base::Optional<MapRef> TryGetObjectCreateMap() const;
  bool IsSeqString() const;
  bool IsExternalString() const;
};

class PropertyCellRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  ObjectRef value() const;
  PropertyDetails property_details() const;
};

class JSObjectRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  bool IsUnboxedDoubleField(FieldIndex index) const;
  double RawFastDoublePropertyAt(FieldIndex index) const;
  ObjectRef RawFastPropertyAt(FieldIndex index) const;

  FixedArrayBaseRef elements() const;
  void EnsureElementsTenured();
  ElementsKind GetElementsKind();
};

struct SlackTrackingResult {
  SlackTrackingResult(int instance_sizex, int inobject_property_countx)
      : instance_size(instance_sizex),
        inobject_property_count(inobject_property_countx) {}
  int instance_size;
  int inobject_property_count;
};

class JSFunctionRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;

  bool HasBuiltinFunctionId() const;
  BuiltinFunctionId GetBuiltinFunctionId() const;
  bool IsConstructor() const;
  bool has_initial_map() const;
  MapRef initial_map() const;
  JSGlobalProxyRef global_proxy() const;
  SlackTrackingResult FinishSlackTracking() const;
  SharedFunctionInfoRef shared() const;
  void EnsureHasInitialMap() const;
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

  base::Optional<ContextRef> previous() const;
  ObjectRef get(int index) const;
};

class NativeContextRef : public ContextRef {
 public:
  using ContextRef::ContextRef;

  ScriptContextTableRef script_context_table() const;

  MapRef fast_aliased_arguments_map() const;
  MapRef sloppy_arguments_map() const;
  MapRef strict_arguments_map() const;
  MapRef js_array_fast_elements_map_index() const;
  MapRef initial_array_iterator_map() const;
  MapRef set_value_iterator_map() const;
  MapRef set_key_value_iterator_map() const;
  MapRef map_key_iterator_map() const;
  MapRef map_value_iterator_map() const;
  MapRef map_key_value_iterator_map() const;
  MapRef iterator_result_map() const;
  MapRef string_iterator_map() const;
  MapRef promise_function_initial_map() const;

  MapRef GetFunctionMapFromIndex(int index) const;
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

class FeedbackVectorRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  ObjectRef get(FeedbackSlot slot) const;
};

class AllocationSiteRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  JSObjectRef boilerplate() const;
  PretenureFlag GetPretenureMode() const;
  bool IsFastLiteral() const;
  ObjectRef nested_site() const;
  bool PointsToLiteral() const;
  ElementsKind GetElementsKind() const;
};

class MapRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  int instance_size() const;
  InstanceType instance_type() const;
  int GetInObjectProperties() const;
  int NumberOfOwnDescriptors() const;
  PropertyDetails GetPropertyDetails(int i) const;
  NameRef GetPropertyKey(int i) const;
  FieldIndex GetFieldIndexFor(int i) const;
  int GetInObjectPropertyOffset(int index) const;
  ElementsKind elements_kind() const;
  ObjectRef constructor_or_backpointer() const;
  bool is_stable() const;
  bool has_prototype_slot() const;
  bool is_deprecated() const;
  bool CanBeDeprecated() const;
  bool CanTransition() const;
  bool IsInobjectSlackTrackingInProgress() const;
  MapRef FindFieldOwner(int descriptor) const;
  bool is_dictionary_map() const;
  bool IsJSArrayMap() const;
  bool IsFixedCowArrayMap() const;

  // Concerning the underlying instance_descriptors:
  FieldTypeRef GetFieldType(int descriptor) const;
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
  bool is_the_hole(int i) const;
};

class FixedDoubleArrayRef : public FixedArrayBaseRef {
 public:
  using FixedArrayBaseRef::FixedArrayBaseRef;

  double get_scalar(int i) const;
  bool is_the_hole(int i) const;
};

class JSArrayRef : public JSObjectRef {
 public:
  using JSObjectRef::JSObjectRef;

  ElementsKind GetElementsKind() const;
  ObjectRef length() const;
};

class ScopeInfoRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  int ContextLength() const;
};

class SharedFunctionInfoRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  int internal_formal_parameter_count() const;
  bool has_duplicate_parameters() const;
  int function_map_index() const;
  FunctionKind kind() const;
  LanguageMode language_mode();
  bool native() const;
  bool HasBreakInfo() const;
  bool HasBuiltinId() const;
  int builtin_id() const;
  bool construct_as_builtin() const;
  bool HasBytecodeArray() const;
  int GetBytecodeArrayRegisterCount() const;
};

class StringRef : public NameRef {
 public:
  using NameRef::NameRef;

  int length() const;
  uint16_t GetFirstChar();
  double ToNumber();
};

class ModuleRef : public HeapObjectRef {
 public:
  using HeapObjectRef::HeapObjectRef;

  CellRef GetCell(int cell_index);
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

class V8_EXPORT_PRIVATE JSHeapBroker : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  JSHeapBroker(Isolate* isolate);

  HeapObjectType HeapObjectTypeFromMap(Handle<Map> map) const {
    AllowHandleDereference handle_dereference;
    return HeapObjectTypeFromMap(*map);
  }

  static base::Optional<int> TryGetSmi(Handle<Object> object);

  Isolate* isolate() const { return isolate_; }

 private:
  friend class HeapObjectRef;
  HeapObjectType HeapObjectTypeFromMap(Map* map) const;

  Isolate* const isolate_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_HEAP_BROKER_H_
