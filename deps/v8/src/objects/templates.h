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

class TemplateInfo : public Struct {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_ACCESSORS(tag, Object)
  DECL_ACCESSORS(serial_number, Object)
  DECL_INT_ACCESSORS(number_of_properties)
  DECL_ACCESSORS(property_list, Object)
  DECL_ACCESSORS(property_accessors, Object)

  DECL_VERIFIER(TemplateInfo)

  DECL_CAST(TemplateInfo)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_TEMPLATE_INFO_FIELDS)

  static const int kFastTemplateInstantiationsCacheSize = 1 * KB;

  // While we could grow the slow cache until we run out of memory, we put
  // a limit on it anyway to not crash for embedders that re-create templates
  // instead of caching them.
  static const int kSlowTemplateInstantiationsCacheSize = 1 * MB;

  OBJECT_CONSTRUCTORS(TemplateInfo, Struct);
};

// Contains data members that are rarely set on a FunctionTemplateInfo.
class FunctionTemplateRareData : public Struct {
 public:
  // See DECL_RARE_ACCESSORS in FunctionTemplateInfo.
  DECL_ACCESSORS(prototype_template, Object)
  DECL_ACCESSORS(prototype_provider_template, Object)
  DECL_ACCESSORS(parent_template, Object)
  DECL_ACCESSORS(named_property_handler, Object)
  DECL_ACCESSORS(indexed_property_handler, Object)
  DECL_ACCESSORS(instance_template, Object)
  DECL_ACCESSORS(instance_call_handler, Object)
  DECL_ACCESSORS(access_check_info, Object)

  DECL_CAST(FunctionTemplateRareData)

  // Dispatched behavior.
  DECL_PRINTER(FunctionTemplateRareData)
  DECL_VERIFIER(FunctionTemplateRareData)

  DEFINE_FIELD_OFFSET_CONSTANTS(
      HeapObject::kHeaderSize,
      TORQUE_GENERATED_FUNCTION_TEMPLATE_RARE_DATA_FIELDS)

  OBJECT_CONSTRUCTORS(FunctionTemplateRareData, Struct);
};

// See the api-exposed FunctionTemplate for more information.
class FunctionTemplateInfo : public TemplateInfo {
 public:
  // Handler invoked when calling an instance of this FunctionTemplateInfo.
  // Either CallInfoHandler or Undefined.
  DECL_ACCESSORS(call_code, Object)

  DECL_ACCESSORS(class_name, Object)

  // If the signature is a FunctionTemplateInfo it is used to check whether the
  // receiver calling the associated JSFunction is a compatible receiver, i.e.
  // it is an instance of the signature FunctionTemplateInfo or any of the
  // receiver's prototypes are.
  DECL_ACCESSORS(signature, Object)

  // If any of the setters below declared by DECL_RARE_ACCESSORS are used then
  // a FunctionTemplateRareData will be stored here. Until then this contains
  // undefined.
  DECL_ACCESSORS(rare_data, HeapObject)

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

  DECL_ACCESSORS(shared_function_info, Object)

  // Internal field to store a flag bitfield.
  DECL_INT_ACCESSORS(flag)

  // "length" property of the final JSFunction.
  DECL_INT_ACCESSORS(length)

  // Either the_hole or a private symbol. Used to cache the result on
  // the receiver under the the cached_property_name when this
  // FunctionTemplateInfo is used as a getter.
  DECL_ACCESSORS(cached_property_name, Object)

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

  DECL_CAST(FunctionTemplateInfo)

  // Dispatched behavior.
  DECL_PRINTER(FunctionTemplateInfo)
  DECL_VERIFIER(FunctionTemplateInfo)

  static const int kInvalidSerialNumber = 0;

  DEFINE_FIELD_OFFSET_CONSTANTS(TemplateInfo::kHeaderSize,
                                TORQUE_GENERATED_FUNCTION_TEMPLATE_INFO_FIELDS)

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

  OBJECT_CONSTRUCTORS(FunctionTemplateInfo, TemplateInfo);
};

class ObjectTemplateInfo : public TemplateInfo {
 public:
  DECL_ACCESSORS(constructor, Object)
  DECL_ACCESSORS(data, Object)
  DECL_INT_ACCESSORS(embedder_field_count)
  DECL_BOOLEAN_ACCESSORS(immutable_proto)

  DECL_CAST(ObjectTemplateInfo)

  // Dispatched behavior.
  DECL_PRINTER(ObjectTemplateInfo)
  DECL_VERIFIER(ObjectTemplateInfo)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(TemplateInfo::kHeaderSize,
                                TORQUE_GENERATED_OBJECT_TEMPLATE_INFO_FIELDS)

  // Starting from given object template's constructor walk up the inheritance
  // chain till a function template that has an instance template is found.
  inline ObjectTemplateInfo GetParent(Isolate* isolate);

 private:
  class IsImmutablePrototype : public BitField<bool, 0, 1> {};
  class EmbedderFieldCount
      : public BitField<int, IsImmutablePrototype::kNext, 29> {};

  OBJECT_CONSTRUCTORS(ObjectTemplateInfo, TemplateInfo);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_H_
