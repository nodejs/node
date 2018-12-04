// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATES_H_
#define V8_OBJECTS_TEMPLATES_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class TemplateInfo : public Struct, public NeverReadOnlySpaceObject {
 public:
  DECL_ACCESSORS(tag, Object)
  DECL_ACCESSORS(serial_number, Object)
  DECL_INT_ACCESSORS(number_of_properties)
  DECL_ACCESSORS(property_list, Object)
  DECL_ACCESSORS(property_accessors, Object)

  DECL_VERIFIER(TemplateInfo)

  DECL_CAST(TemplateInfo)

  static const int kTagOffset = HeapObject::kHeaderSize;
  static const int kSerialNumberOffset = kTagOffset + kPointerSize;
  static const int kNumberOfProperties = kSerialNumberOffset + kPointerSize;
  static const int kPropertyListOffset = kNumberOfProperties + kPointerSize;
  static const int kPropertyAccessorsOffset =
      kPropertyListOffset + kPointerSize;
  static const int kHeaderSize = kPropertyAccessorsOffset + kPointerSize;

  static const int kFastTemplateInstantiationsCacheSize = 1 * KB;

  // While we could grow the slow cache until we run out of memory, we put
  // a limit on it anyway to not crash for embedders that re-create templates
  // instead of caching them.
  static const int kSlowTemplateInstantiationsCacheSize = 1 * MB;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateInfo);
};

// See the api-exposed FunctionTemplate for more information.
class FunctionTemplateInfo : public TemplateInfo {
 public:
  // Handler invoked when calling an instance of this FunctionTemplateInfo.
  // Either CallInfoHandler or Undefined.
  DECL_ACCESSORS(call_code, Object)

  // ObjectTemplateInfo or Undefined, used for the prototype property of the
  // resulting JSFunction instance of this FunctionTemplate.
  DECL_ACCESSORS(prototype_template, Object)

  // In the case the prototype_template is Undefined we use the
  // protoype_provider_template to retrieve the instance prototype. Either
  // contains an ObjectTemplateInfo or Undefined.
  DECL_ACCESSORS(prototype_provider_template, Object)

  // Used to create protoype chains. The parent_template's prototype is set as
  // __proto__ of this FunctionTemplate's instance prototype. Is either a
  // FunctionTemplateInfo or Undefined.
  DECL_ACCESSORS(parent_template, Object)

  // Returns an InterceptorInfo or Undefined for named properties.
  DECL_ACCESSORS(named_property_handler, Object)
  // Returns an InterceptorInfo or Undefined for indexed properties/elements.
  DECL_ACCESSORS(indexed_property_handler, Object)

  // An ObjectTemplateInfo that is used when instantiating the JSFunction
  // associated with this FunctionTemplateInfo. Contains either an
  // ObjectTemplateInfo or Undefined. A default instance_template is assigned
  // upon first instantiation if it's Undefined.
  DECL_ACCESSORS(instance_template, Object)

  DECL_ACCESSORS(class_name, Object)

  // If the signature is a FunctionTemplateInfo it is used to check whether the
  // receiver calling the associated JSFunction is a compatible receiver, i.e.
  // it is an instance of the signare FunctionTemplateInfo or any of the
  // receiver's prototypes are.
  DECL_ACCESSORS(signature, Object)

  // Either a CallHandlerInfo or Undefined. If an instance_call_handler is
  // provided the instances created from the associated JSFunction are marked as
  // callable.
  DECL_ACCESSORS(instance_call_handler, Object)

  DECL_ACCESSORS(access_check_info, Object)
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
  DECL_BOOLEAN_ACCESSORS(hidden_prototype)
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

  static const int kCallCodeOffset = TemplateInfo::kHeaderSize;
  static const int kPrototypeTemplateOffset = kCallCodeOffset + kPointerSize;
  static const int kPrototypeProviderTemplateOffset =
      kPrototypeTemplateOffset + kPointerSize;
  static const int kParentTemplateOffset =
      kPrototypeProviderTemplateOffset + kPointerSize;
  static const int kNamedPropertyHandlerOffset =
      kParentTemplateOffset + kPointerSize;
  static const int kIndexedPropertyHandlerOffset =
      kNamedPropertyHandlerOffset + kPointerSize;
  static const int kInstanceTemplateOffset =
      kIndexedPropertyHandlerOffset + kPointerSize;
  static const int kClassNameOffset = kInstanceTemplateOffset + kPointerSize;
  static const int kSignatureOffset = kClassNameOffset + kPointerSize;
  static const int kInstanceCallHandlerOffset = kSignatureOffset + kPointerSize;
  static const int kAccessCheckInfoOffset =
      kInstanceCallHandlerOffset + kPointerSize;
  static const int kSharedFunctionInfoOffset =
      kAccessCheckInfoOffset + kPointerSize;
  static const int kFlagOffset = kSharedFunctionInfoOffset + kPointerSize;
  static const int kLengthOffset = kFlagOffset + kPointerSize;
  static const int kCachedPropertyNameOffset = kLengthOffset + kPointerSize;
  static const int kSize = kCachedPropertyNameOffset + kPointerSize;

  static Handle<SharedFunctionInfo> GetOrCreateSharedFunctionInfo(
      Isolate* isolate, Handle<FunctionTemplateInfo> info,
      MaybeHandle<Name> maybe_name);
  // Returns parent function template or null.
  inline FunctionTemplateInfo* GetParent(Isolate* isolate);
  // Returns true if |object| is an instance of this function template.
  inline bool IsTemplateFor(JSObject* object);
  bool IsTemplateFor(Map* map);
  inline bool instantiated();

  inline bool BreakAtEntry();

  // Helper function for cached accessors.
  static MaybeHandle<Name> TryGetCachedPropertyName(Isolate* isolate,
                                                    Handle<Object> getter);

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kHiddenPrototypeBit = 0;
  static const int kUndetectableBit = 1;
  static const int kNeedsAccessCheckBit = 2;
  static const int kReadOnlyPrototypeBit = 3;
  static const int kRemovePrototypeBit = 4;
  static const int kDoNotCacheBit = 5;
  static const int kAcceptAnyReceiver = 6;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionTemplateInfo);
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

  static const int kConstructorOffset = TemplateInfo::kHeaderSize;
  // LSB is for immutable_proto, higher bits for embedder_field_count
  static const int kDataOffset = kConstructorOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

  // Starting from given object template's constructor walk up the inheritance
  // chain till a function template that has an instance template is found.
  inline ObjectTemplateInfo* GetParent(Isolate* isolate);

 private:
  class IsImmutablePrototype : public BitField<bool, 0, 1> {};
  class EmbedderFieldCount
      : public BitField<int, IsImmutablePrototype::kNext, 29> {};
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_H_
