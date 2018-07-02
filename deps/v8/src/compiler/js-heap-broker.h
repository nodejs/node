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

#define HEAP_BROKER_DATA_LIST(V) \
  V(AllocationSite)              \
  V(Context)                     \
  V(FeedbackVector)              \
  V(FixedArray)                  \
  V(FixedArrayBase)              \
  V(FixedDoubleArray)            \
  V(HeapNumber)                  \
  V(HeapObject)                  \
  V(JSArray)                     \
  V(JSFunction)                  \
  V(JSObject)                    \
  V(JSRegExp)                    \
  V(MutableHeapNumber)           \
  V(Name)                        \
  V(NativeContext)               \
  V(ScriptContextTable)          \
  V(SharedFunctionInfo)          \
  V(Map)

#define HEAP_BROKER_KIND_LIST(V) \
  HEAP_BROKER_DATA_LIST(V)       \
  V(InternalizedString)          \
  V(String)

#define FORWARD_DECL(Name) class Name##Ref;
HEAP_BROKER_DATA_LIST(FORWARD_DECL)
#undef FORWARD_DECL

class JSHeapBroker;
class HeapObjectRef;

class ObjectRef {
 public:
  explicit ObjectRef(Handle<Object> object) : object_(object) {}

  template <typename T>
  Handle<T> object() const {
    AllowHandleDereference handle_dereference;
    return Handle<T>::cast(object_);
  }

  OddballType oddball_type(const JSHeapBroker* broker) const;

  bool IsSmi() const;
  int AsSmi() const;

#define HEAP_IS_METHOD_DECL(Name) bool Is##Name() const;
  HEAP_BROKER_KIND_LIST(HEAP_IS_METHOD_DECL)
#undef HEAP_IS_METHOD_DECL

#define HEAP_AS_METHOD_DECL(Name) Name##Ref As##Name() const;
  HEAP_BROKER_DATA_LIST(HEAP_AS_METHOD_DECL)
#undef HEAP_AS_METHOD_DECL

 private:
  Handle<Object> object_;
};

class HeapObjectRef : public ObjectRef {
 public:
  explicit HeapObjectRef(Handle<Object> object);
  HeapObjectType type(const JSHeapBroker* broker) const;

  MapRef map(const JSHeapBroker* broker) const;

 private:
  friend class JSHeapBroker;
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

class JSObjectRef : public HeapObjectRef {
 public:
  explicit JSObjectRef(Handle<Object> object) : HeapObjectRef(object) {}

  bool IsUnboxedDoubleField(FieldIndex index) const;
  double RawFastDoublePropertyAt(FieldIndex index) const;
  ObjectRef RawFastPropertyAt(const JSHeapBroker* broker,
                              FieldIndex index) const;

  FixedArrayBaseRef elements(const JSHeapBroker* broker) const;
  void EnsureElementsTenured(const JSHeapBroker* broker);
};

class JSFunctionRef : public JSObjectRef {
 public:
  explicit JSFunctionRef(Handle<Object> object);
  bool HasBuiltinFunctionId() const;
  BuiltinFunctionId GetBuiltinFunctionId() const;
};

class JSRegExpRef : public JSObjectRef {
 public:
  explicit JSRegExpRef(Handle<Object> object) : JSObjectRef(object) {}

  ObjectRef raw_properties_or_hash(const JSHeapBroker* broker) const;
  ObjectRef data(const JSHeapBroker* broker) const;
  ObjectRef source(const JSHeapBroker* broker) const;
  ObjectRef flags(const JSHeapBroker* broker) const;
  ObjectRef last_index(const JSHeapBroker* broker) const;
};

class HeapNumberRef : public HeapObjectRef {
 public:
  explicit HeapNumberRef(Handle<Object> object);
  double value() const;
};

class MutableHeapNumberRef : public HeapObjectRef {
 public:
  explicit MutableHeapNumberRef(Handle<Object> object);
  double value() const;
};

class ContextRef : public HeapObjectRef {
 public:
  explicit ContextRef(Handle<Object> object);
  base::Optional<ContextRef> previous(const JSHeapBroker* broker) const;
  ObjectRef get(const JSHeapBroker* broker, int index) const;
};

class NativeContextRef : public ContextRef {
 public:
  explicit NativeContextRef(Handle<Object> object);

  ScriptContextTableRef script_context_table(const JSHeapBroker* broker) const;

  MapRef fast_aliased_arguments_map(const JSHeapBroker* broker) const;
  MapRef sloppy_arguments_map(const JSHeapBroker* broker) const;
  MapRef strict_arguments_map(const JSHeapBroker* broker) const;
  MapRef js_array_fast_elements_map_index(const JSHeapBroker* broker) const;
};

class NameRef : public HeapObjectRef {
 public:
  explicit NameRef(Handle<Object> object);
};

class ScriptContextTableRef : public HeapObjectRef {
 public:
  explicit ScriptContextTableRef(Handle<Object> object);

  struct LookupResult {
    ContextRef context;
    bool immutable;
    int index;
  };

  base::Optional<LookupResult> lookup(const NameRef& name) const;
};

class FeedbackVectorRef : public HeapObjectRef {
 public:
  explicit FeedbackVectorRef(Handle<Object> object) : HeapObjectRef(object) {}
  ObjectRef get(const JSHeapBroker* broker, FeedbackSlot slot) const;
};

class AllocationSiteRef : public HeapObjectRef {
 public:
  explicit AllocationSiteRef(Handle<HeapObject> object)
      : HeapObjectRef(object) {}

  JSObjectRef boilerplate(const JSHeapBroker* broker) const;
  PretenureFlag GetPretenureMode() const;

  bool IsFastLiteral(const JSHeapBroker* broker);
};

class MapRef : public HeapObjectRef {
 public:
  explicit MapRef(Handle<HeapObject> object) : HeapObjectRef(object) {}

  int instance_size() const;
  InstanceType instance_type() const;
  int GetInObjectProperties() const;
  int NumberOfOwnDescriptors() const;
  PropertyDetails GetPropertyDetails(int i) const;
  NameRef GetPropertyKey(const JSHeapBroker* broker, int i) const;
  FieldIndex GetFieldIndexFor(int i) const;

  bool IsJSArrayMap() const;
  bool IsFixedCowArrayMap(const JSHeapBroker* broker) const;
};

class FixedArrayBaseRef : public HeapObjectRef {
 public:
  explicit FixedArrayBaseRef(Handle<HeapObject> object)
      : HeapObjectRef(object) {}

  int length() const;
};

class FixedArrayRef : public FixedArrayBaseRef {
 public:
  explicit FixedArrayRef(Handle<HeapObject> object)
      : FixedArrayBaseRef(object) {}

  bool is_the_hole(const JSHeapBroker* broker, int i) const;
  ObjectRef get(const JSHeapBroker* broker, int i) const;
};

class FixedDoubleArrayRef : public FixedArrayBaseRef {
 public:
  explicit FixedDoubleArrayRef(Handle<HeapObject> object)
      : FixedArrayBaseRef(object) {}

  bool is_the_hole(int i) const;
  double get_scalar(int i) const;
};

class JSArrayRef : public JSObjectRef {
 public:
  explicit JSArrayRef(Handle<Object> object) : JSObjectRef(object) {}

  ElementsKind GetElementsKind() const;
  ObjectRef length(const JSHeapBroker* broker) const;
};

class SharedFunctionInfoRef : public HeapObjectRef {
 public:
  explicit SharedFunctionInfoRef(Handle<Object> object)
      : HeapObjectRef(object) {}

  int internal_formal_parameter_count() const;
  bool has_duplicate_parameters() const;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_HEAP_BROKER_H_
