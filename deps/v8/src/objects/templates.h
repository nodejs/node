// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATES_H_
#define V8_OBJECTS_TEMPLATES_H_

#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class TemplateInfo : public TorqueGeneratedTemplateInfo<TemplateInfo, Struct> {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_INT_ACCESSORS(number_of_properties)

  static const int kFastTemplateInstantiationsCacheSize = 1 * KB;

  // While we could grow the slow cache until we run out of memory, we put
  // a limit on it anyway to not crash for embedders that re-create templates
  // instead of caching them.
  static const int kSlowTemplateInstantiationsCacheSize = 1 * MB;

  TQ_OBJECT_CONSTRUCTORS(TemplateInfo)
};

// Contains data members that are rarely set on a FunctionTemplateInfo.
class FunctionTemplateRareData
    : public TorqueGeneratedFunctionTemplateRareData<FunctionTemplateRareData,
                                                     Struct> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(FunctionTemplateRareData)

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
  DECL_RARE_ACCESSORS(prototype_template, PrototypeTemplate, Object)

  // In the case the prototype_template is Undefined we use the
  // prototype_provider_template to retrieve the instance prototype. Either
  // contains an ObjectTemplateInfo or Undefined.
  DECL_RARE_ACCESSORS(prototype_provider_template, PrototypeProviderTemplate,
                      Object)

  // Used to create prototype chains. The parent_template's prototype is set as
  // __proto__ of this FunctionTemplate's instance prototype. Is either a
  // FunctionTemplateInfo or Undefined.
  DECL_RARE_ACCESSORS(parent_template, ParentTemplate, Object)

  // Returns an InterceptorInfo or Undefined for named properties.
  DECL_RARE_ACCESSORS(named_property_handler, NamedPropertyHandler, Object)
  // Returns an InterceptorInfo or Undefined for indexed properties/elements.
  DECL_RARE_ACCESSORS(indexed_property_handler, IndexedPropertyHandler, Object)

  // An ObjectTemplateInfo that is used when instantiating the JSFunction
  // associated with this FunctionTemplateInfo. Contains either an
  // ObjectTemplateInfo or Undefined. A default instance_template is assigned
  // upon first instantiation if it's Undefined.
  DECL_RARE_ACCESSORS(instance_template, InstanceTemplate, Object)

  // Either a CallHandlerInfo or Undefined. If an instance_call_handler is
  // provided the instances created from the associated JSFunction are marked as
  // callable.
  DECL_RARE_ACCESSORS(instance_call_handler, InstanceCallHandler, Object)

  DECL_RARE_ACCESSORS(access_check_info, AccessCheckInfo, Object)
#undef DECL_RARE_ACCESSORS

  // Internal field to store a flag bitfield.
  DECL_INT_ACCESSORS(flag)

  // "length" property of the final JSFunction.
  DECL_INT_ACCESSORS(length)

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

  // If set, do not attach a serial number to this FunctionTemplate and thus do
  // not keep an instance boilerplate around.
  DECL_BOOLEAN_ACCESSORS(do_not_cache)

  // If not set an access may be performed on calling the associated JSFunction.
  DECL_BOOLEAN_ACCESSORS(accept_any_receiver)
  // End flag bits ---------------------

  // Dispatched behavior.
  DECL_PRINTER(FunctionTemplateInfo)

  static const int kInvalidSerialNumber = 0;

  static Handle<SharedFunctionInfo> GetOrCreateSharedFunctionInfo(
      Isolate* isolate, Handle<FunctionTemplateInfo> info,
      MaybeHandle<Name> maybe_name);
  // Returns parent function template or a null FunctionTemplateInfo.
  inline FunctionTemplateInfo GetParent(Isolate* isolate);
  // Returns true if |object| is an instance of this function template.
  inline bool IsTemplateFor(JSObject object);
  bool IsTemplateFor(Map map);
  inline bool instantiated();

  inline bool BreakAtEntry();

  // Helper function for cached accessors.
  static MaybeHandle<Name> TryGetCachedPropertyName(Isolate* isolate,
                                                    Handle<Object> getter);

  // Bit position in the flag, from least significant bit position.
  static const int kUndetectableBit = 0;
  static const int kNeedsAccessCheckBit = 1;
  static const int kReadOnlyPrototypeBit = 2;
  static const int kRemovePrototypeBit = 3;
  static const int kDoNotCacheBit = 4;
  static const int kAcceptAnyReceiver = 5;

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

  // Dispatched behavior.
  DECL_PRINTER(ObjectTemplateInfo)

  // Starting from given object template's constructor walk up the inheritance
  // chain till a function template that has an instance template is found.
  inline ObjectTemplateInfo GetParent(Isolate* isolate);

 private:
  using IsImmutablePrototype = base::BitField<bool, 0, 1>;
  using EmbedderFieldCount = IsImmutablePrototype::Next<int, 29>;

  TQ_OBJECT_CONSTRUCTORS(ObjectTemplateInfo)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_H_
