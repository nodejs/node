// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATES_H_
#define V8_OBJECTS_TEMPLATES_H_

#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {

class CFunctionInfo;

namespace internal {

#include "torque-generated/src/objects/templates-tq.inc"

class TemplateInfo : public TorqueGeneratedTemplateInfo<TemplateInfo, Struct> {
 public:
  NEVER_READ_ONLY_SPACE

  static const int kFastTemplateInstantiationsCacheSize = 1 * KB;

  // While we could grow the slow cache until we run out of memory, we put
  // a limit on it anyway to not crash for embedders that re-create templates
  // instead of caching them.
  static const int kSlowTemplateInstantiationsCacheSize = 1 * MB;

  // If the serial number is set to kDoNotCache, then we should never cache this
  // TemplateInfo.
  static const int kDoNotCache = -1;
  // If the serial number is set to kUncached, it means that this TemplateInfo
  // has not been cached yet but it can be.
  static const int kUncached = -2;

  inline bool should_cache() const;
  inline bool is_cached() const;

  TQ_OBJECT_CONSTRUCTORS(TemplateInfo)
};

// Contains data members that are rarely set on a FunctionTemplateInfo.
class FunctionTemplateRareData
    : public TorqueGeneratedFunctionTemplateRareData<FunctionTemplateRareData,
                                                     Struct> {
 public:
  DECL_VERIFIER(FunctionTemplateRareData)
  TQ_OBJECT_CONSTRUCTORS(FunctionTemplateRareData)
};

// See the api-exposed FunctionTemplate for more information.
class FunctionTemplateInfo
    : public TorqueGeneratedFunctionTemplateInfo<FunctionTemplateInfo,
                                                 TemplateInfo> {
 public:
#define DECL_RARE_ACCESSORS(Name, CamelName, Type)                           \
  DECL_GETTER(Get##CamelName, Type)                                          \
  static inline void Set##CamelName(                                         \
      Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info, \
      Handle<Type> Name);

  // ObjectTemplateInfo or Undefined, used for the prototype property of the
  // resulting JSFunction instance of this FunctionTemplate.
  DECL_RARE_ACCESSORS(prototype_template, PrototypeTemplate, HeapObject)

  // In the case the prototype_template is Undefined we use the
  // prototype_provider_template to retrieve the instance prototype. Either
  // contains an FunctionTemplateInfo or Undefined.
  DECL_RARE_ACCESSORS(prototype_provider_template, PrototypeProviderTemplate,
                      HeapObject)

  // Used to create prototype chains. The parent_template's prototype is set as
  // __proto__ of this FunctionTemplate's instance prototype. Is either a
  // FunctionTemplateInfo or Undefined.
  DECL_RARE_ACCESSORS(parent_template, ParentTemplate, HeapObject)

  // Returns an InterceptorInfo or Undefined for named properties.
  DECL_RARE_ACCESSORS(named_property_handler, NamedPropertyHandler, HeapObject)
  // Returns an InterceptorInfo or Undefined for indexed properties/elements.
  DECL_RARE_ACCESSORS(indexed_property_handler, IndexedPropertyHandler,
                      HeapObject)

  // An ObjectTemplateInfo that is used when instantiating the JSFunction
  // associated with this FunctionTemplateInfo. Contains either an
  // ObjectTemplateInfo or Undefined. A default instance_template is assigned
  // upon first instantiation if it's Undefined.
  DECL_RARE_ACCESSORS(instance_template, InstanceTemplate, HeapObject)

  // Either a CallHandlerInfo or Undefined. If an instance_call_handler is
  // provided the instances created from the associated JSFunction are marked as
  // callable.
  DECL_RARE_ACCESSORS(instance_call_handler, InstanceCallHandler, HeapObject)

  DECL_RARE_ACCESSORS(access_check_info, AccessCheckInfo, HeapObject)

  DECL_RARE_ACCESSORS(c_function_overloads, CFunctionOverloads, FixedArray)
#undef DECL_RARE_ACCESSORS

  // Begin flag bits ---------------------
  DECL_BOOLEAN_ACCESSORS(undetectable)

  // If set, object instances created by this function
  // requires access check.
  DECL_BOOLEAN_ACCESSORS(needs_access_check)

  DECL_BOOLEAN_ACCESSORS(read_only_prototype)

  // If set, do not create a prototype property for the associated
  // JSFunction. This bit implies that neither the prototype_template nor the
  // prototype_provoider_template are instantiated.
  DECL_BOOLEAN_ACCESSORS(remove_prototype)

  // If not set an access may be performed on calling the associated JSFunction.
  DECL_BOOLEAN_ACCESSORS(accept_any_receiver)

  // This flag is used to check that the FunctionTemplateInfo instance is not
  // changed after it became visible to TurboFan (either set in a
  // SharedFunctionInfo or an accessor), because TF relies on immutability to
  // safely read concurrently.
  DECL_BOOLEAN_ACCESSORS(published)

  DECL_INT_ACCESSORS(allowed_receiver_range_start)
  DECL_INT_ACCESSORS(allowed_receiver_range_end)
  // End flag bits ---------------------

  // Dispatched behavior.
  DECL_PRINTER(FunctionTemplateInfo)

  inline int InstanceType() const;
  inline void SetInstanceType(int instance_type);

  static Handle<SharedFunctionInfo> GetOrCreateSharedFunctionInfo(
      Isolate* isolate, Handle<FunctionTemplateInfo> info,
      MaybeHandle<Name> maybe_name);

  static Handle<SharedFunctionInfo> GetOrCreateSharedFunctionInfo(
      LocalIsolate* isolate, Handle<FunctionTemplateInfo> info,
      Handle<Name> maybe_name) {
    // We don't support streaming compilation of scripts with natives, so we
    // don't need an off-thread implementation of this.
    UNREACHABLE();
  }

  // Returns parent function template or a null FunctionTemplateInfo.
  inline FunctionTemplateInfo GetParent(Isolate* isolate);
  // Returns true if |object| is an instance of this function template.
  inline bool IsTemplateFor(JSObject object);
  bool IsTemplateFor(Map map) const;
  // Returns true if |object| is an API object and is constructed by this
  // particular function template (skips walking up the chain of inheriting
  // functions that is done by IsTemplateFor).
  bool IsLeafTemplateForApiObject(Object object) const;
  inline bool instantiated();

  bool BreakAtEntry();

  // Helper function for cached accessors.
  static base::Optional<Name> TryGetCachedPropertyName(Isolate* isolate,
                                                       Object getter);
  // Fast API overloads.
  int GetCFunctionsCount() const;
  Address GetCFunction(int index) const;
  const CFunctionInfo* GetCSignature(int index) const;

  // CFunction data for a set of overloads is stored into a FixedArray, as
  // [address_0, signature_0, ... address_n-1, signature_n-1].
  static const int kFunctionOverloadEntrySize = 2;

  // Bit position in the flag, from least significant bit position.
  DEFINE_TORQUE_GENERATED_FUNCTION_TEMPLATE_INFO_FLAGS()

 private:
  static inline FunctionTemplateRareData EnsureFunctionTemplateRareData(
      Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info);

  static FunctionTemplateRareData AllocateFunctionTemplateRareData(
      Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info);

  TQ_OBJECT_CONSTRUCTORS(FunctionTemplateInfo)
};

class ObjectTemplateInfo
    : public TorqueGeneratedObjectTemplateInfo<ObjectTemplateInfo,
                                               TemplateInfo> {
 public:
  DECL_INT_ACCESSORS(embedder_field_count)
  DECL_BOOLEAN_ACCESSORS(immutable_proto)
  DECL_BOOLEAN_ACCESSORS(code_like)

  // Dispatched behavior.
  DECL_PRINTER(ObjectTemplateInfo)

  // Starting from given object template's constructor walk up the inheritance
  // chain till a function template that has an instance template is found.
  inline ObjectTemplateInfo GetParent(Isolate* isolate);

 private:
  DEFINE_TORQUE_GENERATED_OBJECT_TEMPLATE_INFO_FLAGS()

  TQ_OBJECT_CONSTRUCTORS(ObjectTemplateInfo)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_H_
