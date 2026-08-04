// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATES_H_
#define V8_OBJECTS_TEMPLATES_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "include/v8-exception.h"
#include "include/v8-memory-span.h"
#include "src/handles/handles.h"
#include "src/objects/contexts.h"
#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {

class CFunctionInfo;
class StructBodyDescriptor;

namespace internal {

class FunctionTemplateRareData;

#include "torque-generated/src/objects/templates-tq.inc"

struct CFunctionWithSignature {
  static constexpr ExternalPointerTag kManagedTag = kCFunctionWithSignatureTag;
  const Address address;
  const CFunctionInfo* signature;

  CFunctionWithSignature(const Address address, const CFunctionInfo* signature)
      : address(address), signature(signature) {}
};

V8_OBJECT class TemplateInfo : public HeapObjectLayout {
 public:
  static const int kFastTemplateInstantiationsCacheSize = 1 * KB;

  // While we could grow the cache until we run out of memory, we put
  // a limit on it anyway to not crash for embedders that re-create templates
  // instead of caching them.
  static constexpr int kMaxTemplateInstantiationsCacheSize = 1 * MB;

  // Initial serial number value.
  static const int kUninitializedSerialNumber = 0;

  // Serial numbers less than this must not be reused.
  static const int kFirstNonUniqueSerialNumber =
      kFastTemplateInstantiationsCacheSize;

  DECL_BOOLEAN_ACCESSORS(is_cacheable)
  DECL_BOOLEAN_ACCESSORS(should_promote_to_read_only)
  DECL_PRIMITIVE_ACCESSORS(serial_number, uint32_t)

  // Initializes serial number if necessary and returns it.
  inline uint32_t EnsureHasSerialNumber(Isolate* isolate);

  inline uint32_t GetHash() const;

  inline uint32_t template_info_flags() const;
  inline void set_template_info_flags(uint32_t value);

  // Whether or not to cache every instance: when we materialize a getter or
  // setter from an lazy AccessorPair, we rely on this cache to be able to
  // always return the same getter or setter. However, objects will be cloned
  // anyways, so it's not observable if we didn't cache an instance.
  // Furthermore, a badly behaved embedder might create an unlimited number of
  // objects, so we limit the cache for those cases.
  enum class CachingMode { kLimited, kUnlimited };

  template <typename ReturnType>
  static MaybeHandle<ReturnType> ProbeInstantiationsCache(
      Isolate* isolate, DirectHandle<NativeContext> native_context,
      DirectHandle<TemplateInfo> info, CachingMode caching_mode) {
    return Cast<ReturnType>(
        ProbeInstantiationsCache(isolate, native_context, info, caching_mode));
  }

  inline static MaybeHandle<Object> ProbeInstantiationsCache(
      Isolate* isolate, DirectHandle<NativeContext> native_context,
      DirectHandle<TemplateInfo> info, CachingMode caching_mode);

  inline static void CacheTemplateInstantiation(
      Isolate* isolate, DirectHandle<NativeContext> native_context,
      DirectHandle<TemplateInfo> info, CachingMode caching_mode,
      DirectHandle<Object> object);

  inline static void UncacheTemplateInstantiation(
      Isolate* isolate, DirectHandle<NativeContext> native_context,
      DirectHandle<TemplateInfo> info, CachingMode caching_mode);

  // Bit position in the template_info_base_flags, from least significant bit
  // position.
  DEFINE_TORQUE_GENERATED_TEMPLATE_INFO_FLAGS()

  TaggedMember<Smi> template_info_flags_;
} V8_OBJECT_END;

V8_OBJECT class TemplateInfoWithProperties : public TemplateInfo {
 public:
  inline int number_of_properties() const;
  inline void set_number_of_properties(int value);

  inline Tagged<UnionOf<ArrayList, Undefined>> property_list() const;
  inline void set_property_list(Tagged<UnionOf<ArrayList, Undefined>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<ArrayList, Undefined>> property_accessors() const;
  inline void set_property_accessors(
      Tagged<UnionOf<ArrayList, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  TaggedMember<Smi> number_of_properties_;
  TaggedMember<UnionOf<ArrayList, Undefined>> property_list_;
  TaggedMember<UnionOf<ArrayList, Undefined>> property_accessors_;
} V8_OBJECT_END;

// Contains data members that are rarely set on a FunctionTemplateInfo.
V8_OBJECT class FunctionTemplateRareData : public Struct {
 public:
  inline Tagged<UnionOf<Undefined, ObjectTemplateInfo>> prototype_template()
      const;
  inline void set_prototype_template(
      Tagged<UnionOf<Undefined, ObjectTemplateInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, FunctionTemplateInfo>>
  prototype_provider_template() const;
  inline void set_prototype_provider_template(
      Tagged<UnionOf<Undefined, FunctionTemplateInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, FunctionTemplateInfo>> parent_template()
      const;
  inline void set_parent_template(
      Tagged<UnionOf<Undefined, FunctionTemplateInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, InterceptorInfo>> named_property_handler()
      const;
  inline void set_named_property_handler(
      Tagged<UnionOf<Undefined, InterceptorInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, InterceptorInfo>> indexed_property_handler()
      const;
  inline void set_indexed_property_handler(
      Tagged<UnionOf<Undefined, InterceptorInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, ObjectTemplateInfo>> instance_template()
      const;
  inline void set_instance_template(
      Tagged<UnionOf<Undefined, ObjectTemplateInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, FunctionTemplateInfo>>
  instance_call_handler() const;
  inline void set_instance_call_handler(
      Tagged<UnionOf<Undefined, FunctionTemplateInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, AccessCheckInfo>> access_check_info() const;
  inline void set_access_check_info(
      Tagged<UnionOf<Undefined, AccessCheckInfo>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> c_function_overloads() const;
  inline void set_c_function_overloads(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(FunctionTemplateRareData)
  DECL_VERIFIER(FunctionTemplateRareData)

  using BodyDescriptor = StructBodyDescriptor;

 public:
  TaggedMember<UnionOf<Undefined, ObjectTemplateInfo>> prototype_template_;
  TaggedMember<UnionOf<Undefined, FunctionTemplateInfo>>
      prototype_provider_template_;
  TaggedMember<UnionOf<Undefined, FunctionTemplateInfo>> parent_template_;
  TaggedMember<UnionOf<Undefined, InterceptorInfo>> named_property_handler_;
  TaggedMember<UnionOf<Undefined, InterceptorInfo>> indexed_property_handler_;
  TaggedMember<UnionOf<Undefined, ObjectTemplateInfo>> instance_template_;
  TaggedMember<UnionOf<Undefined, FunctionTemplateInfo>> instance_call_handler_;
  TaggedMember<UnionOf<Undefined, AccessCheckInfo>> access_check_info_;
  TaggedMember<FixedArray> c_function_overloads_;
} V8_OBJECT_END;

// See the api-exposed FunctionTemplate for more information.
V8_OBJECT class FunctionTemplateInfo : public TemplateInfoWithProperties {
 public:
#define DECL_RARE_ACCESSORS(Name, CamelName, ...)                \
  inline Tagged<__VA_ARGS__> Get##CamelName() const;             \
  static inline void Set##CamelName(                             \
      Isolate* isolate,                                          \
      DirectHandle<FunctionTemplateInfo> function_template_info, \
      DirectHandle<__VA_ARGS__> Name);

  // ObjectTemplateInfo or Undefined, used for the prototype property of the
  // resulting JSFunction instance of this FunctionTemplate.
  DECL_RARE_ACCESSORS(prototype_template, PrototypeTemplate,
                      UnionOf<Undefined, ObjectTemplateInfo>)

  // In the case the prototype_template is Undefined we use the
  // prototype_provider_template to retrieve the instance prototype. Either
  // contains an FunctionTemplateInfo or Undefined.
  DECL_RARE_ACCESSORS(prototype_provider_template, PrototypeProviderTemplate,
                      UnionOf<Undefined, FunctionTemplateInfo>)

  // Used to create prototype chains. The parent_template's prototype is set as
  // __proto__ of this FunctionTemplate's instance prototype. Is either a
  // FunctionTemplateInfo or Undefined.
  DECL_RARE_ACCESSORS(parent_template, ParentTemplate,
                      UnionOf<Undefined, FunctionTemplateInfo>)

  // Returns an InterceptorInfo or Undefined for named properties.
  DECL_RARE_ACCESSORS(named_property_handler, NamedPropertyHandler,
                      UnionOf<Undefined, InterceptorInfo>)
  // Returns an InterceptorInfo or Undefined for indexed properties/elements.
  DECL_RARE_ACCESSORS(indexed_property_handler, IndexedPropertyHandler,
                      UnionOf<Undefined, InterceptorInfo>)

  // An ObjectTemplateInfo that is used when instantiating the JSFunction
  // associated with this FunctionTemplateInfo. Contains either an
  // ObjectTemplateInfo or Undefined. A default instance_template is assigned
  // upon first instantiation if it's Undefined.
  DECL_RARE_ACCESSORS(instance_template, InstanceTemplate,
                      UnionOf<Undefined, ObjectTemplateInfo>)

  // Either a FunctionTemplateInfo or Undefined. If an instance_call_handler is
  // provided the instances created from the associated JSFunction are marked as
  // callable.
  DECL_RARE_ACCESSORS(instance_call_handler, InstanceCallHandler,
                      UnionOf<Undefined, FunctionTemplateInfo>)

  DECL_RARE_ACCESSORS(access_check_info, AccessCheckInfo,
                      UnionOf<Undefined, AccessCheckInfo>)

  DECL_RARE_ACCESSORS(c_function_overloads, CFunctionOverloads, FixedArray)
#undef DECL_RARE_ACCESSORS

  DECL_PRIMITIVE_ACCESSORS(flag, uint32_t)
  DECL_RELAXED_UINT32_ACCESSORS(flag)

  // Begin flag bits ---------------------

  // This FunctionTemplateInfo is just a storage for callback function and
  // callback data for a callable ObjectTemplate object.
  DECL_BOOLEAN_ACCESSORS(is_object_template_call_handler)

  DECL_BOOLEAN_ACCESSORS(has_side_effects)

  DECL_BOOLEAN_ACCESSORS(undetectable)

  // If set, object instances created by this function requires access check.
  DECL_BOOLEAN_ACCESSORS(needs_access_check)

  DECL_BOOLEAN_ACCESSORS(read_only_prototype)

  // If set, do not create a prototype property for the associated
  // JSFunction. This bit implies that neither the prototype_template nor the
  // prototype_provider_template are instantiated.
  DECL_BOOLEAN_ACCESSORS(remove_prototype)

  // If not set an access may be performed on calling the associated JSFunction.
  DECL_BOOLEAN_ACCESSORS(accept_any_receiver)

  // This flag is used to check that the FunctionTemplateInfo instance is not
  // changed after it became visible to TurboFan (either set in a
  // SharedFunctionInfo or an accessor), because TF relies on immutability to
  // safely read concurrently.
  DECL_BOOLEAN_ACCESSORS(published)

  // This specifies the permissible range of instance type of objects that can
  // be allowed to be used as receivers with the given template.
  DECL_PRIMITIVE_GETTER(allowed_receiver_instance_type_range_start,
                        InstanceType)
  DECL_PRIMITIVE_GETTER(allowed_receiver_instance_type_range_end, InstanceType)

  // End flag bits ---------------------

  inline InstanceType GetInstanceType() const;
  inline void SetInstanceType(int api_instance_type);

  inline void SetAllowedReceiverInstanceTypeRange(int api_instance_type_start,
                                                  int api_instance_type_end);

  static Handle<SharedFunctionInfo> GetOrCreateSharedFunctionInfo(
      Isolate* isolate, DirectHandle<FunctionTemplateInfo> info,
      MaybeDirectHandle<Name> maybe_name);

  static Handle<SharedFunctionInfo> GetOrCreateSharedFunctionInfo(
      LocalIsolate* isolate, DirectHandle<FunctionTemplateInfo> info,
      DirectHandle<Name> maybe_name) {
    // We don't support streaming compilation of scripts with natives, so we
    // don't need an off-thread implementation of this.
    UNREACHABLE();
  }

  // Returns parent function template or a null FunctionTemplateInfo.
  inline Tagged<FunctionTemplateInfo> GetParent(Isolate* isolate);
  // Returns true if |object| is an instance of this function template.
  inline bool IsTemplateFor(Tagged<JSObject> object) const;
  bool IsTemplateFor(Tagged<Map> map) const;
  // Returns true if |object| is an API object and is constructed by this
  // particular function template (skips walking up the chain of inheriting
  // functions that is done by IsTemplateFor).
  bool IsLeafTemplateForApiObject(Tagged<Object> object) const;
  inline bool instantiated();

  static void SealAndPrepareForPromotionToReadOnly(
      Isolate* isolate, DirectHandle<FunctionTemplateInfo> info);

  bool BreakAtEntry(Isolate* isolate);
  bool HasInstanceType();

  // Helper function for cached accessors.
  static std::optional<Tagged<Name>> TryGetCachedPropertyName(
      Isolate* isolate, Tagged<Object> getter);
  // Fast API overloads.
  uint32_t GetCFunctionsCount() const;
  CFunctionWithSignature GetCFunction(uint32_t index) const;

  // Bit position in the flag, from least significant bit position.
  DEFINE_TORQUE_GENERATED_FUNCTION_TEMPLATE_INFO_FLAGS()

  // C function pointer that can be called from native code.
  inline Address callback(IsolateForSandbox isolate) const;
  inline void set_callback(IsolateForSandbox isolate, Address value);
  inline void init_callback(IsolateForSandbox isolate, Address value);

  inline void RemoveCallbackRedirectionForSerialization(
      IsolateForSandbox isolate);
  inline void RestoreCallbackRedirectionAfterDeserialization(
      IsolateForSandbox isolate);

  template <class IsolateT>
  inline bool has_callback(IsolateT* isolate) const;

  DECL_PRINTER(FunctionTemplateInfo)
  DECL_VERIFIER(FunctionTemplateInfo)

  class BodyDescriptor;

 private:
  // For ease of use of the BITFIELD macro.
  inline int32_t relaxed_flag() const;
  inline void set_relaxed_flag(int32_t flags);

  // Enforce using SetInstanceType() and SetAllowedReceiverInstanceTypeRange()
  // instead of raw accessors.
  void set_instance_type(int value);
  DECL_PRIMITIVE_SETTER(allowed_receiver_instance_type_range_start,
                        InstanceType)
  DECL_PRIMITIVE_SETTER(allowed_receiver_instance_type_range_end, InstanceType)

  static constexpr int kNoJSApiObjectType = 0;
  static inline Tagged<FunctionTemplateRareData> EnsureFunctionTemplateRareData(
      Isolate* isolate,
      DirectHandle<FunctionTemplateInfo> function_template_info);

  static Tagged<FunctionTemplateRareData> AllocateFunctionTemplateRareData(
      Isolate* isolate,
      DirectHandle<FunctionTemplateInfo> function_template_info);

 public:
  inline Tagged<UnionOf<String, Undefined>> class_name() const;
  inline void set_class_name(Tagged<UnionOf<String, Undefined>> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<String, Undefined>> interface_name() const;
  inline void set_interface_name(Tagged<UnionOf<String, Undefined>> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<FunctionTemplateInfo, Undefined>> signature() const;
  inline void set_signature(
      Tagged<UnionOf<FunctionTemplateInfo, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<FunctionTemplateRareData, Undefined>> rare_data() const;
  inline Tagged<UnionOf<FunctionTemplateRareData, Undefined>> rare_data(
      AcquireLoadTag tag) const;
  inline void set_rare_data(
      Tagged<UnionOf<FunctionTemplateRareData, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set_rare_data(
      Tagged<UnionOf<FunctionTemplateRareData, Undefined>> value,
      ReleaseStoreTag tag, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<SharedFunctionInfo, Undefined>> shared_function_info()
      const;
  inline void set_shared_function_info(
      Tagged<UnionOf<SharedFunctionInfo, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> cached_property_name() const;
  inline void set_cached_property_name(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> callback_data() const;
  inline Tagged<Object> callback_data(AcquireLoadTag tag) const;
  inline void set_callback_data(Tagged<Object> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set_callback_data(Tagged<Object> value, ReleaseStoreTag tag,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int16_t length() const;
  inline void set_length(int16_t value);

  inline InstanceType instance_type() const;
  inline void set_instance_type(InstanceType value);

  inline uint32_t exception_context() const;
  inline void set_exception_context(uint32_t value);

  TaggedMember<UnionOf<String, Undefined>> class_name_;
  TaggedMember<UnionOf<String, Undefined>> interface_name_;
  TaggedMember<UnionOf<FunctionTemplateInfo, Undefined>> signature_;
  TaggedMember<UnionOf<FunctionTemplateRareData, Undefined>> rare_data_;
  TaggedMember<UnionOf<SharedFunctionInfo, Undefined>> shared_function_info_;
  TaggedMember<Object> cached_property_name_;
  TaggedMember<Object> callback_data_;
  uint32_t flag_;
  int16_t length_;
  InstanceType instance_type_;
  uint32_t exception_context_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
  ExternalPointerMember<kFunctionTemplateInfoCallbackTag> callback_;
} V8_OBJECT_END;

V8_OBJECT class ObjectTemplateInfo : public TemplateInfoWithProperties {
 public:
  DECL_INT_ACCESSORS(embedder_field_count)
  DECL_BOOLEAN_ACCESSORS(immutable_proto)
  DECL_BOOLEAN_ACCESSORS(code_like)

  // Starting from given object template's constructor walk up the inheritance
  // chain till a function template that has an instance template is found.
  inline Tagged<ObjectTemplateInfo> GetParent(Isolate* isolate);

  static void SealAndPrepareForPromotionToReadOnly(
      Isolate* isolate, DirectHandle<ObjectTemplateInfo> info);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_PRINTER(ObjectTemplateInfo)
  DECL_VERIFIER(ObjectTemplateInfo)

  inline Tagged<UnionOf<FunctionTemplateInfo, Undefined>> constructor() const;
  inline void set_constructor(
      Tagged<UnionOf<FunctionTemplateInfo, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int data() const;
  inline void set_data(int value);

  TaggedMember<UnionOf<FunctionTemplateInfo, Undefined>> constructor_;
  TaggedMember<Smi> data_;

 private:
  DEFINE_TORQUE_GENERATED_OBJECT_TEMPLATE_INFO_FLAGS()
} V8_OBJECT_END;

V8_OBJECT class DictionaryTemplateInfo : public TemplateInfo {
 public:
  using BodyDescriptor = StructBodyDescriptor;

  static DirectHandle<DictionaryTemplateInfo> Create(
      Isolate* isolate, const v8::MemorySpan<const std::string_view>& names);

  static DirectHandle<JSObject> NewInstance(
      DirectHandle<NativeContext> context,
      DirectHandle<DictionaryTemplateInfo> self,
      const MemorySpan<MaybeLocal<Value>>& property_values);

  DECL_PRINTER(DictionaryTemplateInfo)
  DECL_VERIFIER(DictionaryTemplateInfo)

  inline Tagged<FixedArray> property_names() const;
  inline void set_property_names(Tagged<FixedArray> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  TaggedMember<FixedArray> property_names_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_H_
