// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_OBJECTS_H_
#define V8_OBJECTS_JS_OBJECTS_H_

#include <optional>

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/fixed-array.h"
// TODO(jkummerow): Consider forward-declaring instead.
#include "src/objects/internal-index.h"
#include "src/objects/objects.h"
#include "src/objects/property-array.h"
#include "src/sandbox/cppheap-pointer.h"
#include "src/sandbox/external-pointer.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

// Enum for functions that offer a second mode that does not cause allocations.
// Used in conjunction with LookupIterator and unboxed double fields.
enum class AllocationPolicy { kAllocationAllowed, kAllocationDisallowed };

enum InstanceType : uint16_t;
class JSGlobalObject;
class JSGlobalProxy;
class LookupIterator;
class PropertyDescriptor;
class PropertyKey;
class NativeContext;
class IsCompiledScope;
class StackTraceInfo;
class SwissNameDictionary;
class GlobalDictionary;
class ElementsAccessor;
class Undefined;
class Null;

#include "torque-generated/src/objects/js-objects-tq.inc"

// JSReceiver includes types on which properties can be defined, i.e.,
// JSObject and JSProxy.
V8_OBJECT class JSReceiver : public HeapObject {
 public:
  using Properties =
      UnionOf<SwissNameDictionary, FixedArrayBase, PropertyArray>;
  using PropertiesOrHash = UnionOf<SwissNameDictionary, FixedArrayBase,
                                   PropertyArray, Smi, GlobalDictionary>;

  // Returns true if there is no slow (ie, dictionary) backing store.
  DECL_GETTER(HasFastProperties, bool)

  // Returns the properties array backing store if it
  // exists. Otherwise, returns an empty_property_array when there's a
  // Smi (hash code) or an empty_fixed_array for a fast properties
  // map.
  DECL_GETTER(property_array, Tagged<PropertyArray>)

  // Gets slow properties for non-global objects (if
  // v8_enable_swiss_name_dictionary is not set).
  DECL_GETTER(property_dictionary, Tagged<NameDictionary>)

  // Gets slow properties for non-global objects (if
  // v8_enable_swiss_name_dictionary is set).
  DECL_GETTER(property_dictionary_swiss, Tagged<SwissNameDictionary>)

  // Sets the properties backing store and makes sure any existing hash is moved
  // to the new properties store. To clear out the properties store, pass in the
  // empty_fixed_array(), the hash will be maintained in this case as well.
  void SetProperties(Tagged<Properties> properties);

  // There are five possible values for the properties offset.
  // 1) EmptyFixedArray/EmptyPropertyDictionary - This is the standard
  // placeholder.
  //
  // 2) Smi - This is the hash code of the object.
  //
  // 3) PropertyArray - This is similar to a FixedArray but stores
  // the hash code of the object in its length field. This is a fast
  // backing store.
  //
  // 4) NameDictionary - This is the dictionary-mode backing store.
  //
  // 4) GlobalDictionary - This is the backing store for the
  // GlobalObject.
  //
  // This is used only in the deoptimizer and heap. Please use the
  // above typed getters and setters to access the properties.
  inline Tagged<PropertiesOrHash> raw_properties_or_hash() const;
  inline void set_raw_properties_or_hash(
      Tagged<PropertiesOrHash> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<PropertiesOrHash> raw_properties_or_hash(
      RelaxedLoadTag tag) const;
  inline void set_raw_properties_or_hash(
      Tagged<PropertiesOrHash> value, RelaxedStoreTag,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline void initialize_properties(Isolate* isolate);

  // Deletes an existing named property in a normalized object.
  static void DeleteNormalizedProperty(DirectHandle<JSReceiver> object,
                                       InternalIndex entry);

  // ES6 section 7.1.1 ToPrimitive
  template <template <typename> typename HandleType>
    requires(
        std::is_convertible_v<HandleType<JSReceiver>, DirectHandle<JSReceiver>>)
  V8_WARN_UNUSED_RESULT static typename HandleType<Object>::MaybeType
  ToPrimitive(Isolate* isolate, HandleType<JSReceiver> receiver,
              ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

  // ES6 section 7.1.1.1 OrdinaryToPrimitive
  template <template <typename> typename HandleType>
    requires(std::is_convertible_v<HandleType<Object>, DirectHandle<Object>>)
  V8_WARN_UNUSED_RESULT static typename HandleType<Object>::MaybeType
  OrdinaryToPrimitive(Isolate* isolate, DirectHandle<JSReceiver> receiver,
                      OrdinaryToPrimitiveHint hint);

  // Unwraps the chain of potential function wrappers or JSProxy objects and
  // return the leaf function's creation context.
  // Throws TypeError in case there's a revoked JSProxy on the way.
  // https://tc39.es/ecma262/#sec-getfunctionrealm
  static MaybeDirectHandle<NativeContext> GetFunctionRealm(
      DirectHandle<JSReceiver> receiver);

  V8_EXPORT_PRIVATE static MaybeHandle<NativeContext> GetContextForMicrotask(
      DirectHandle<JSReceiver> receiver);

  // Get the first non-hidden prototype.
  static inline MaybeDirectHandle<JSPrototype> GetPrototype(
      Isolate* isolate, DirectHandle<JSReceiver> receiver);

  V8_WARN_UNUSED_RESULT static Maybe<bool> HasInPrototypeChain(
      Isolate* isolate, DirectHandle<JSReceiver> object,
      DirectHandle<Object> proto);

  // Reads all enumerable own properties of source and adds them to
  // target, using either Set or CreateDataProperty depending on the
  // use_set argument. This only copies values not present in the
  // maybe_excluded_properties list.
  // If direct handles are enabled, it is the responsibility of the caller to
  // ensure that the memory pointed to by `excluded_properties` is scanned
  // during CSS, e.g., it comes from a `DirectHandleVector<Object>`.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetOrCopyDataProperties(
      Isolate* isolate, DirectHandle<JSReceiver> target,
      DirectHandle<Object> source, PropertiesEnumerationMode mode,
      base::Vector<DirectHandle<Object>> excluded_properties = {},
      bool use_set = true);

  // Implementation of [[HasProperty]], ECMA-262 5th edition, section 8.12.6.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> HasProperty(
      LookupIterator* it);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasProperty(
      Isolate* isolate, DirectHandle<JSReceiver> object,
      DirectHandle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasElement(
      Isolate* isolate, DirectHandle<JSReceiver> object, uint32_t index);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasPropertyOrElement(
      Isolate* isolate, DirectHandle<JSReceiver> object, PropertyKey key);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> HasOwnProperty(
      Isolate* isolate, DirectHandle<JSReceiver> object,
      DirectHandle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasOwnProperty(
      Isolate* isolate, DirectHandle<JSReceiver> object, size_t index);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, DirectHandle<JSReceiver> receiver, const char* key);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, DirectHandle<JSReceiver> receiver,
      DirectHandle<Name> name);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetElement(
      Isolate* isolate, DirectHandle<JSReceiver> receiver, uint32_t index);

  // Implementation of ES6 [[Delete]]
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool>
  DeletePropertyOrElement(Isolate* isolate, DirectHandle<JSReceiver> object,
                          DirectHandle<Name> name,
                          LanguageMode language_mode = LanguageMode::kSloppy);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool>
  DeletePropertyOrElement(Isolate* isolate, DirectHandle<JSReceiver> object,
                          PropertyKey key,
                          LanguageMode language_mode = LanguageMode::kSloppy);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteProperty(
      Isolate* isolate, DirectHandle<JSReceiver> object,
      DirectHandle<Name> name,
      LanguageMode language_mode = LanguageMode::kSloppy);
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteProperty(
      LookupIterator* it, LanguageMode language_mode);
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteElement(
      Isolate* isolate, DirectHandle<JSReceiver> object, uint32_t index,
      LanguageMode language_mode = LanguageMode::kSloppy);

  V8_WARN_UNUSED_RESULT static Tagged<Object> DefineProperty(
      Isolate* isolate, DirectHandle<Object> object, DirectHandle<Object> name,
      Handle<Object> attributes);
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object> DefineProperties(
      Isolate* isolate, DirectHandle<Object> object,
      DirectHandle<Object> properties);

  // "virtual" dispatcher to the correct [[DefineOwnProperty]] implementation.
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, DirectHandle<JSReceiver> object,
      DirectHandle<Object> key, PropertyDescriptor* desc,
      Maybe<ShouldThrow> should_throw);

  // Check if private name property can be store on the object. It will return
  // false with an error when it cannot but didn't throw, or a Nothing if
  // it throws.
  V8_WARN_UNUSED_RESULT static Maybe<bool> CheckPrivateNameStore(
      LookupIterator* it, bool is_define);

  // ES6 7.3.4 (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      Isolate* isolate, DirectHandle<JSReceiver> object, DirectHandle<Name> key,
      DirectHandle<Object> value, Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      Isolate* isolate, DirectHandle<JSAny> object, PropertyKey key,
      DirectHandle<Object> value, Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      Isolate* isolate, DirectHandle<JSReceiver> object, PropertyKey key,
      DirectHandle<Object> value, Maybe<ShouldThrow> should_throw);

  // Add private fields to the receiver, ignoring extensibility and the
  // traps. The caller should check that the private field does not already
  // exist on the receiver before calling this method.
  V8_WARN_UNUSED_RESULT static Maybe<bool> AddPrivateField(
      LookupIterator* it, DirectHandle<Object> value,
      Maybe<ShouldThrow> should_throw);

  // ES6 9.1.6.1
  V8_WARN_UNUSED_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      Isolate* isolate, DirectHandle<JSObject> object, DirectHandle<Object> key,
      PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      Isolate* isolate, DirectHandle<JSObject> object, const PropertyKey& key,
      PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      LookupIterator* it, PropertyDescriptor* desc,
      Maybe<ShouldThrow> should_throw);
  // ES6 9.1.6.2
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsCompatiblePropertyDescriptor(
      Isolate* isolate, bool extensible, PropertyDescriptor* desc,
      PropertyDescriptor* current, DirectHandle<Name> property_name,
      Maybe<ShouldThrow> should_throw);
  // ES6 9.1.6.3
  // |it| can be NULL in cases where the ES spec passes |undefined| as the
  // receiver. Exactly one of |it| and |property_name| must be provided.
  V8_WARN_UNUSED_RESULT static Maybe<bool> ValidateAndApplyPropertyDescriptor(
      Isolate* isolate, LookupIterator* it, bool extensible,
      PropertyDescriptor* desc, PropertyDescriptor* current,
      Maybe<ShouldThrow> should_throw, DirectHandle<Name> property_name);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool>
  GetOwnPropertyDescriptor(Isolate* isolate, DirectHandle<JSReceiver> object,
                           DirectHandle<Object> key, PropertyDescriptor* desc);
  V8_WARN_UNUSED_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      LookupIterator* it, PropertyDescriptor* desc);

  using IntegrityLevel = PropertyAttributes;

  // ES6 7.3.14 (when passed kDontThrow)
  // 'level' must be SEALED or FROZEN.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetIntegrityLevel(
      Isolate* isolate, DirectHandle<JSReceiver> object, IntegrityLevel lvl,
      ShouldThrow should_throw);

  // ES6 7.3.15
  // 'level' must be SEALED or FROZEN.
  V8_WARN_UNUSED_RESULT static Maybe<bool> TestIntegrityLevel(
      Isolate* isolate, DirectHandle<JSReceiver> object, IntegrityLevel lvl);

  // ES6 [[PreventExtensions]] (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      Isolate* isolate, DirectHandle<JSReceiver> object,
      ShouldThrow should_throw);

  V8_WARN_UNUSED_RESULT static Maybe<bool> IsExtensible(
      Isolate* isolate, DirectHandle<JSReceiver> object);

  // Returns the class name.
  V8_EXPORT_PRIVATE Tagged<String> class_name();

  // Returns the constructor (the function that was used to instantiate the
  // object).
  static MaybeDirectHandle<JSFunction> GetConstructor(
      Isolate* isolate, DirectHandle<JSReceiver> receiver);

  // Returns the constructor name (the (possibly inferred) name of the function
  // that was used to instantiate the object), if any. If a FunctionTemplate is
  // used to instantiate the object, the class_name of the FunctionTemplate is
  // returned instead.
  static DirectHandle<String> GetConstructorName(
      Isolate* isolate, DirectHandle<JSReceiver> receiver);

  V8_EXPORT_PRIVATE inline std::optional<Tagged<NativeContext>>
  GetCreationContext() const;
  V8_EXPORT_PRIVATE inline MaybeDirectHandle<NativeContext> GetCreationContext(
      Isolate* isolate) const;

  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetPropertyAttributes(Isolate* isolate, DirectHandle<JSReceiver> object,
                        DirectHandle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Isolate* isolate, DirectHandle<JSReceiver> object,
                           DirectHandle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Isolate* isolate, DirectHandle<JSReceiver> object,
                           size_t index);

  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetElementAttributes(Isolate* isolate, DirectHandle<JSReceiver> object,
                       uint32_t index);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnElementAttributes(Isolate* isolate, DirectHandle<JSReceiver> object,
                          uint32_t index);

  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Isolate* isolate, DirectHandle<JSReceiver> object,
      DirectHandle<Object> value, bool from_javascript,
      ShouldThrow should_throw);

  inline static Handle<Object> GetDataProperty(Isolate* isolate,
                                               DirectHandle<JSReceiver> object,
                                               DirectHandle<Name> name);
  V8_EXPORT_PRIVATE static Handle<Object> GetDataProperty(
      LookupIterator* it, AllocationPolicy allocation_policy =
                              AllocationPolicy::kAllocationAllowed);

  // Retrieves a permanent object identity hash code. The undefined value might
  // be returned in case no hash was created yet.
  V8_EXPORT_PRIVATE Tagged<Object> GetIdentityHash();

  // Retrieves a permanent object identity hash code. May create and store a
  // hash code if needed and none exists.
  static Tagged<Smi> CreateIdentityHash(Isolate* isolate,
                                        Tagged<JSReceiver> key);
  V8_EXPORT_PRIVATE Tagged<Smi> GetOrCreateIdentityHash(Isolate* isolate);

  // Stores the hash code. The hash passed in must be masked with
  // JSReceiver::kHashMask.
  V8_EXPORT_PRIVATE void SetIdentityHash(int masked_hash);

  // ES6 [[OwnPropertyKeys]] (modulo return type)
  V8_WARN_UNUSED_RESULT static inline MaybeDirectHandle<FixedArray>
  OwnPropertyKeys(Isolate* isolate, DirectHandle<JSReceiver> object);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<FixedArray> GetOwnValues(
      Isolate* isolate, DirectHandle<JSReceiver> object, PropertyFilter filter,
      bool try_fast_path = true);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<FixedArray> GetOwnEntries(
      Isolate* isolate, DirectHandle<JSReceiver> object, PropertyFilter filter,
      bool try_fast_path = true);

  static const int kHashMask = PropertyArray::HashField::kMask;

  bool HasProxyInPrototype(Isolate* isolate);

  // TC39 "Dynamic Code Brand Checks"
  bool IsCodeLike(Isolate* isolate) const;

  DECL_VERIFIER(JSReceiver)

 public:
  TaggedMember<PropertiesOrHash> properties_or_hash_;
} V8_OBJECT_END;

// The JSObject describes real heap allocated JavaScript objects with
// properties.
// Note that the map of JSObject changes during execution to enable inline
// caching.
V8_OBJECT class JSObject : public JSReceiver {
 public:
  static const int kEndOfStrongFieldsOffset;
  static const int kHeaderSize;
  static constexpr int kMapOffset = offsetof(HeapObject, map_);

  // Mirror the JSReceiver::IntegrityLevel type alias so method signatures
  // that take `IntegrityLevel` parameters can be declared on JSObject.
  using IntegrityLevel = JSReceiver::IntegrityLevel;

  static bool IsUnmodifiedApiObject(FullObjectSlot o);

  V8_EXPORT_PRIVATE static V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> New(
      DirectHandle<JSFunction> constructor, DirectHandle<JSReceiver> new_target,
      DirectHandle<AllocationSite> site,
      NewJSObjectType = NewJSObjectType::kMaybeEmbedderFieldsAndNoApiWrapper);

  static MaybeDirectHandle<JSObject> NewWithMap(
      Isolate* isolate, DirectHandle<Map> initial_map,
      DirectHandle<AllocationSite> site,
      NewJSObjectType = NewJSObjectType::kMaybeEmbedderFieldsAndNoApiWrapper);

  // 9.1.12 ObjectCreate ( proto [ , internalSlotsList ] )
  // Notice: This is NOT 19.1.2.2 Object.create ( O, Properties )
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSObject> ObjectCreate(
      Isolate* isolate, DirectHandle<JSPrototype> prototype);

  DECL_ACCESSORS(elements, Tagged<FixedArrayBase>)
  DECL_RELAXED_GETTER(elements, Tagged<FixedArrayBase>)

  // Acquire/release semantics on this field are explicitly forbidden to avoid
  // confusion, since the default setter uses relaxed semantics. If
  // acquire/release semantics ever become necessary, the default setter should
  // be reverted to non-atomic behavior, and setters with explicit tags
  // introduced and used when required.
  Tagged<FixedArrayBase> elements(AcquireLoadTag tag) const = delete;
  void set_elements(Tagged<FixedArrayBase> value, ReleaseStoreTag tag,
                    WriteBarrierMode mode = UPDATE_WRITE_BARRIER) = delete;

  inline void initialize_elements();
  static inline void SetMapAndElements(Isolate* isolate,
                                       DirectHandle<JSObject> object,
                                       DirectHandle<Map> map,
                                       DirectHandle<FixedArrayBase> elements);
  DECL_GETTER(GetElementsKind, ElementsKind)
  DECL_GETTER(GetElementsAccessor, ElementsAccessor*)

  // Returns true if an object has elements of PACKED_SMI_ELEMENTS or
  // HOLEY_SMI_ELEMENTS ElementsKind.
  DECL_GETTER(HasSmiElements, bool)
  // Returns true if an object has elements of PACKED_ELEMENTS or
  // HOLEY_ELEMENTS ElementsKind.
  DECL_GETTER(HasObjectElements, bool)
  // Returns true if an object has elements of PACKED_SMI_ELEMENTS,
  // HOLEY_SMI_ELEMENTS, PACKED_ELEMENTS, or HOLEY_ELEMENTS.
  DECL_GETTER(HasSmiOrObjectElements, bool)
  // Returns true if an object has any of the "fast" elements kinds.
  DECL_GETTER(HasFastElements, bool)
  // Returns true if an object has any of the PACKED elements kinds.
  DECL_GETTER(HasFastPackedElements, bool)
  // Returns true if an object has elements of PACKED_DOUBLE_ELEMENTS or
  // HOLEY_DOUBLE_ELEMENTS ElementsKind.
  DECL_GETTER(HasDoubleElements, bool)
  // Returns true if an object has elements of HOLEY_SMI_ELEMENTS,
  // HOLEY_DOUBLE_ELEMENTS, or HOLEY_ELEMENTS ElementsKind.
  DECL_GETTER(HasHoleyElements, bool)
  DECL_GETTER(HasSloppyArgumentsElements, bool)
  DECL_GETTER(HasStringWrapperElements, bool)
  DECL_GETTER(HasDictionaryElements, bool)

  // Returns true if an object has elements of PACKED_ELEMENTS
  DECL_GETTER(HasPackedElements, bool)
  DECL_GETTER(HasAnyNonextensibleElements, bool)
  DECL_GETTER(HasSealedElements, bool)
  DECL_GETTER(HasSharedArrayElements, bool)
  DECL_GETTER(HasNonextensibleElements, bool)

  DECL_GETTER(HasTypedArrayOrRabGsabTypedArrayElements, bool)

  DECL_GETTER(HasFixedUint8ClampedElements, bool)
  DECL_GETTER(HasFixedArrayElements, bool)
  DECL_GETTER(HasFixedInt8Elements, bool)
  DECL_GETTER(HasFixedUint8Elements, bool)
  DECL_GETTER(HasFixedInt16Elements, bool)
  DECL_GETTER(HasFixedUint16Elements, bool)
  DECL_GETTER(HasFixedInt32Elements, bool)
  DECL_GETTER(HasFixedUint32Elements, bool)
  DECL_GETTER(HasFixedFloat16Elements, bool)
  DECL_GETTER(HasFixedFloat32Elements, bool)
  DECL_GETTER(HasFixedFloat64Elements, bool)
  DECL_GETTER(HasFixedBigInt64Elements, bool)
  DECL_GETTER(HasFixedBigUint64Elements, bool)

  DECL_GETTER(HasFastArgumentsElements, bool)
  DECL_GETTER(HasSlowArgumentsElements, bool)
  DECL_GETTER(HasFastStringWrapperElements, bool)
  DECL_GETTER(HasSlowStringWrapperElements, bool)
  bool HasEnumerableElements();

  // Gets slow elements.
  DECL_GETTER(element_dictionary, Tagged<NumberDictionary>)

  // Requires: HasFastElements().
  static inline void EnsureWritableFastElements(Isolate* isolate,
                                                DirectHandle<JSObject> object);
  V8_NOINLINE V8_PRESERVE_MOST static void MakeElementsWritable(
      Isolate* isolate, DirectHandle<JSObject> object);

  V8_WARN_UNUSED_RESULT static Maybe<InterceptorResult>
  SetPropertyWithInterceptor(LookupIterator* it,
                             Maybe<ShouldThrow> should_throw,
                             DirectHandle<Object> value);

  // The API currently still wants DefineOwnPropertyIgnoreAttributes to convert
  // AccessorInfo objects to data fields. We allow FORCE_FIELD as an exception
  // to the default behavior that calls the setter.
  enum AccessorInfoHandling { FORCE_FIELD, DONT_FORCE_FIELD };

  template <typename T, template <typename> typename HandleType>
    requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
  V8_WARN_UNUSED_RESULT static inline typename HandleType<Object>::MaybeType
  DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, HandleType<T> value, PropertyAttributes attributes,
      AccessorInfoHandling handling = DONT_FORCE_FIELD,
      EnforceDefineSemantics semantics = EnforceDefineSemantics::kSet);

  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, DirectHandle<Object> value,
      PropertyAttributes attributes, Maybe<ShouldThrow> should_throw,
      AccessorInfoHandling handling = DONT_FORCE_FIELD,
      EnforceDefineSemantics semantics = EnforceDefineSemantics::kSet,
      StoreOrigin store_origin = StoreOrigin::kNamed,
      MaybeDirectHandle<Object> old_value = {});

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object> V8_EXPORT_PRIVATE
  SetOwnPropertyIgnoreAttributes(DirectHandle<JSObject> object,
                                 DirectHandle<Name> name,
                                 DirectHandle<Object> value,
                                 PropertyAttributes attributes);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object>
  SetOwnElementIgnoreAttributes(DirectHandle<JSObject> object, size_t index,
                                DirectHandle<Object> value,
                                PropertyAttributes attributes);

  // Equivalent to one of the above depending on whether |name| can be converted
  // to an array index.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object>
  DefinePropertyOrElementIgnoreAttributes(DirectHandle<JSObject> object,
                                          DirectHandle<Name> name,
                                          DirectHandle<Object> value,
                                          PropertyAttributes attributes = NONE);

  // Adds or reconfigures a property to attributes NONE. It will fail when it
  // cannot.
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      Isolate* isolate, DirectHandle<JSObject> object, PropertyKey key,
      DirectHandle<Object> value,
      Maybe<ShouldThrow> should_throw = Just(kDontThrow));

  V8_EXPORT_PRIVATE static void AddProperty(Isolate* isolate,
                                            DirectHandle<JSObject> object,
                                            DirectHandle<Name> name,
                                            DirectHandle<Object> value,
                                            PropertyAttributes attributes);

  // {name} must be a UTF-8 encoded, null-terminated string.
  static void AddProperty(Isolate* isolate, DirectHandle<JSObject> object,
                          const char* name, DirectHandle<Object> value,
                          PropertyAttributes attributes);

  V8_EXPORT_PRIVATE static Maybe<bool> AddDataElement(
      Isolate* isolate, DirectHandle<JSObject> receiver, uint32_t index,
      DirectHandle<Object> value, PropertyAttributes attributes);

  // Extend the receiver with a single fast property appeared first in the
  // passed map. This also extends the property backing store if necessary.
  static void AllocateStorageForMap(Isolate* isolate,
                                    DirectHandle<JSObject> object,
                                    DirectHandle<Map> map);

  // Migrates the given object to a map whose field representations are the
  // lowest upper bound of all known representations for that field.
  static void MigrateInstance(Isolate* isolate,
                              DirectHandle<JSObject> instance);

  // Migrates the given object only if the target map is already available,
  // or returns false if such a map is not yet available.
  static bool TryMigrateInstance(Isolate* isolate,
                                 DirectHandle<JSObject> instance);

  // Sets the property value in a normalized object given (key, value, details).
  // Handles the special representation of JS global objects.
  static V8_WARN_UNUSED_RESULT Maybe<bool> SetNormalizedProperty(
      DirectHandle<JSObject> object, DirectHandle<Name> name,
      DirectHandle<Object> value, PropertyDetails details);
  static void SetNormalizedElement(DirectHandle<JSObject> object,
                                   uint32_t index, DirectHandle<Object> value,
                                   PropertyDetails details);

  static void OptimizeAsPrototype(DirectHandle<JSObject> object,
                                  bool enable_setup_mode = true);
  static void ReoptimizeIfPrototype(DirectHandle<JSObject> object);
  static void MakePrototypesFast(DirectHandle<Object> receiver,
                                 WhereToStart where_to_start, Isolate* isolate);
  static void LazyRegisterPrototypeUser(DirectHandle<Map> user,
                                        Isolate* isolate);
  static void UpdatePrototypeUserRegistration(DirectHandle<Map> old_map,
                                              DirectHandle<Map> new_map,
                                              Isolate* isolate);
  static bool UnregisterPrototypeUser(DirectHandle<Map> user, Isolate* isolate);
  static Tagged<Map> InvalidatePrototypeChains(Tagged<Map> map);
  static void InvalidatePrototypeValidityCell(Tagged<JSGlobalObject> global);

  // Updates prototype chain tracking information when an object changes its
  // map from |old_map| to |new_map|.
  static void NotifyMapChange(DirectHandle<Map> old_map,
                              DirectHandle<Map> new_map, Isolate* isolate);

  // Utility used by many Array builtins and runtime functions
  static inline bool PrototypeHasNoElements(Isolate* isolate,
                                            Tagged<JSObject> object);

  // To be passed to PrototypeUsers::Compact.
  static void PrototypeRegistryCompactionCallback(Tagged<HeapObject> value,
                                                  int old_index, int new_index);

  // Retrieve interceptors.
  DECL_GETTER(GetNamedInterceptor, Tagged<InterceptorInfo>)
  DECL_GETTER(GetIndexedInterceptor, Tagged<InterceptorInfo>)

  // Used from JSReceiver.
  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes>
  GetPropertyAttributesWithInterceptor(LookupIterator* it);
  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes>
  GetPropertyAttributesWithFailedAccessCheck(LookupIterator* it);

  // Defines an AccessorPair property on the given object.
  V8_EXPORT_PRIVATE static MaybeDirectHandle<Object>
  DefineOwnAccessorIgnoreAttributes(DirectHandle<JSObject> object,
                                    DirectHandle<Name> name,
                                    DirectHandle<Object> getter,
                                    DirectHandle<Object> setter,
                                    PropertyAttributes attributes);
  static MaybeDirectHandle<Object> DefineOwnAccessorIgnoreAttributes(
      LookupIterator* it, DirectHandle<Object> getter,
      DirectHandle<Object> setter, PropertyAttributes attributes);

  // Defines an AccessorInfo property on the given object.
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object> SetAccessor(
      DirectHandle<JSObject> object, DirectHandle<Name> name,
      DirectHandle<AccessorInfo> info, PropertyAttributes attributes);

  // Check if a data property can be created on the object. It will fail with
  // an error when it cannot.
  V8_WARN_UNUSED_RESULT static Maybe<bool> CheckIfCanDefineAsConfigurable(
      Isolate* isolate, LookupIterator* it, DirectHandle<Object> value,
      Maybe<ShouldThrow> should_throw);

  // The result must be checked first for exceptions. If there's no exception,
  // the output parameter |done| indicates whether the interceptor has a result
  // or not.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSAny> GetPropertyWithInterceptor(
      LookupIterator* it, bool* done);

  static void ValidateElements(Isolate* isolate, Tagged<JSObject> object);

  // Makes sure that this object can contain HeapObject as elements.
  static inline void EnsureCanContainHeapObjectElements(
      Isolate* isolate, DirectHandle<JSObject> obj);

  template <typename TSlot>
  static inline ElementsKind GetTransitionedElementsKind(
      Isolate* isolate, ElementsKind current_kind, TSlot elements,
      uint32_t count, EnsureElementsMode mode);

  // Makes sure that this object can contain the specified elements.
  // TSlot here is either ObjectSlot or FullObjectSlot.
  template <typename TSlot>
  static inline void EnsureCanContainElements(Isolate* isolate,
                                              DirectHandle<JSObject> object,
                                              TSlot elements, uint32_t count,
                                              EnsureElementsMode mode);
  static inline void EnsureCanContainElements(
      Isolate* isolate, DirectHandle<JSObject> object,
      DirectHandle<FixedArrayBase> elements, uint32_t length,
      EnsureElementsMode mode);
  static void EnsureCanContainElements(Isolate* isolate,
                                       DirectHandle<JSObject> object,
                                       JavaScriptArguments* arguments,
                                       uint32_t arg_count,
                                       EnsureElementsMode mode);

  // Would we convert a fast elements array to dictionary mode given
  // an access at key?
  bool WouldConvertToSlowElements(uint32_t index);

  static const uint32_t kMinAddedElementsCapacity = 16;

  // Computes the new capacity when expanding the elements of a JSObject.
  static constexpr uint32_t NewElementsCapacity(uint32_t old_capacity) {
    // (old_capacity + 50%) + kMinAddedElementsCapacity
    uint32_t new_capacity =
        old_capacity + (old_capacity >> 1) + kMinAddedElementsCapacity;

    // If we go past kMaxFixedArrayCapacity, but kMaxFixedArrayCapacity is still
    // more than the old_capacity plus the minimum growth amount, limit the
    // capacity to kMinAddedElementsCapacity.
    if (new_capacity > kMaxFixedArrayCapacity &&
        old_capacity + kMinAddedElementsCapacity <= kMaxFixedArrayCapacity) {
      return kMaxFixedArrayCapacity;
    }
    return new_capacity;
  }

  // These methods do not perform access checks!
  template <AllocationSiteUpdateMode update_or_check =
                AllocationSiteUpdateMode::kUpdate>
  static bool UpdateAllocationSite(Isolate* isolate,
                                   DirectHandle<JSObject> object,
                                   ElementsKind to_kind);

  // Lookup interceptors are used for handling properties controlled by host
  // objects.
  DECL_GETTER(HasNamedInterceptor, bool)
  DECL_GETTER(HasIndexedInterceptor, bool)

  // Support functions for v8 api (needed for correct interceptor behavior).
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealNamedProperty(
      Isolate* isolate, DirectHandle<JSObject> object, DirectHandle<Name> name);
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealElementProperty(
      Isolate* isolate, DirectHandle<JSObject> object, uint32_t index);
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealNamedCallbackProperty(
      Isolate* isolate, DirectHandle<JSObject> object, DirectHandle<Name> name);

  // Get the header size for a JSObject.  Used to compute the index of
  // embedder fields as well as the number of embedder fields.
  static V8_EXPORT_PRIVATE int GetHeaderSize(InstanceType instance_type);
  static inline int GetHeaderSize(Tagged<Map> map);

  static inline bool MayHaveEmbedderFields(Tagged<Map> map);
  inline bool MayHaveEmbedderFields() const;

  static inline int GetEmbedderFieldsStartOffset(Tagged<Map> map);
  inline int GetEmbedderFieldsStartOffset();

  static inline int GetEmbedderFieldCount(Tagged<Map> map);
  inline int GetEmbedderFieldCount() const;
  inline int GetEmbedderFieldOffset(int index);
  inline Tagged<Object> GetEmbedderField(int index);
  inline void SetEmbedderField(int index, Tagged<Object> value);
  inline void SetEmbedderField(int index, Tagged<Smi> value);

  // Returns true if this object is an Api object which can, if unmodified, be
  // dropped during minor GC because the embedder can recreate it again later.
  static inline bool IsDroppableApiObject(Tagged<Map>);
  inline bool IsDroppableApiObject() const;

  // Returns a new map with all transitions dropped from the object's current
  // map and the ElementsKind set.
  static DirectHandle<Map> GetElementsTransitionMap(
      Isolate* isolate, DirectHandle<JSObject> object, ElementsKind to_kind);
  V8_EXPORT_PRIVATE static void TransitionElementsKind(
      Isolate* isolate, DirectHandle<JSObject> object, ElementsKind to_kind);

  // Always use this to migrate an object to a new map.
  // |expected_additional_properties| is only used for fast-to-slow transitions
  // and ignored otherwise.
  V8_EXPORT_PRIVATE static void MigrateToMap(
      Isolate* isolate, DirectHandle<JSObject> object,
      DirectHandle<Map> new_map, int expected_additional_properties = 0);

  // Forces a prototype without any of the checks that the regular SetPrototype
  // would do.
  static void ForceSetPrototype(Isolate* isolate, DirectHandle<JSObject> object,
                                DirectHandle<JSPrototype> proto);

  // Convert the object to use the canonical dictionary
  // representation. If the object is expected to have additional properties
  // added this number can be indicated to have the backing store allocated to
  // an initial capacity for holding these properties.
  V8_EXPORT_PRIVATE static void NormalizeProperties(
      Isolate* isolate, DirectHandle<JSObject> object,
      PropertyNormalizationMode mode, int expected_additional_properties,
      bool use_cache, const char* reason);

  V8_EXPORT_PRIVATE static void NormalizeProperties(
      Isolate* isolate, DirectHandle<JSObject> object,
      PropertyNormalizationMode mode, int expected_additional_properties,
      const char* reason) {
    const bool kUseCache = true;
    NormalizeProperties(isolate, object, mode, expected_additional_properties,
                        kUseCache, reason);
  }

  // Convert and update the elements backing store to be a
  // NumberDictionary dictionary.  Returns the backing after conversion.
  V8_EXPORT_PRIVATE static DirectHandle<NumberDictionary> NormalizeElements(
      Isolate* isolate, DirectHandle<JSObject> object);

  void RequireSlowElements(Tagged<NumberDictionary> dictionary);

  // Transform slow named properties to fast variants.
  V8_EXPORT_PRIVATE static void MigrateSlowToFast(DirectHandle<JSObject> object,
                                                  int unused_property_fields,
                                                  const char* reason);

  // Access property in dictionary mode object at the given dictionary index.
  static Handle<Object> DictionaryPropertyAt(Isolate* isolate,
                                             DirectHandle<JSObject> object,
                                             InternalIndex dict_index);
  // Same as above, but it will return {} if we would be reading out of the
  // bounds of the object or if the dictionary is pending allocation. Use this
  // version for concurrent access.
  static std::optional<Tagged<Object>> DictionaryPropertyAt(
      DirectHandle<JSObject> object, InternalIndex dict_index, Heap* heap);

  // Access fast-case object properties at index.
  static Handle<JSAny> FastPropertyAt(Isolate* isolate,
                                      DirectHandle<JSObject> object,
                                      Representation representation,
                                      FieldIndex index);
  static DirectHandle<JSAny> FastPropertyAt(Isolate* isolate,
                                            DirectHandle<JSObject> object,
                                            Representation representation,
                                            FieldIndex index,
                                            SeqCstAccessTag tag);
  inline Tagged<JSAny> RawFastPropertyAt(FieldIndex index) const;
  inline Tagged<JSAny> RawFastPropertyAt(FieldIndex index,
                                         SeqCstAccessTag tag) const;

  // See comment in the body of the method to understand the conditions
  // in which this method is meant to be used, and what guarantees it
  // provides against invalid reads from another thread during object
  // mutation.
  inline std::optional<Tagged<Object>> RawInobjectPropertyAt(
      Tagged<Map> original_map, FieldIndex index) const;

  inline void FastPropertyAtPut(FieldIndex index, Tagged<Object> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void FastPropertyAtPut(FieldIndex index, Tagged<Object> value,
                                SeqCstAccessTag tag);
  inline void RawFastInobjectPropertyAtPut(
      FieldIndex index, Tagged<Object> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void RawFastInobjectPropertyAtPut(FieldIndex index,
                                           Tagged<Object> value,
                                           SeqCstAccessTag tag);
  inline void WriteToField(InternalIndex descriptor, PropertyDetails details,
                           Tagged<Object> value);

  inline Tagged<Object> RawFastInobjectPropertyAtSwap(FieldIndex index,
                                                      Tagged<Object> value,
                                                      SeqCstAccessTag tag);
  inline Tagged<Object> RawFastPropertyAtSwap(FieldIndex index,
                                              Tagged<Object> value,
                                              SeqCstAccessTag tag);
  Tagged<Object> RawFastPropertyAtCompareAndSwap(FieldIndex index,
                                                 Tagged<Object> expected,
                                                 Tagged<Object> value,
                                                 SeqCstAccessTag tag);
  inline Tagged<Object> RawFastInobjectPropertyAtCompareAndSwap(
      FieldIndex index, Tagged<Object> expected, Tagged<Object> value,
      SeqCstAccessTag tag);

  // Access to in object properties.
  inline int GetInObjectPropertyOffset(int index);
  inline Tagged<Object> InObjectPropertyAtOffset(int offset);
  inline Tagged<Object> InObjectPropertyPutAtIndex(
      int index, Tagged<Object> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> InObjectPropertyPutAtOffset(
      int index, Tagged<Object> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Isolate* isolate, DirectHandle<JSObject> object,
      DirectHandle<Object> value, bool from_javascript,
      ShouldThrow should_throw);

  // Makes the object prototype immutable
  // Never called from JavaScript
  static void SetImmutableProto(Isolate* isolate,
                                DirectHandle<JSObject> object);

  // Initializes the body starting at |start_offset|. It is responsibility of
  // the caller to initialize object header. Fill the pre-allocated fields with
  // undefined_value and the rest with filler_map.
  // Note: this call does not update write barrier, the caller is responsible
  // to ensure that |filler_map| can be collected without WB here.
  inline void InitializeBody(Tagged<Map> map, int start_offset,
                             bool is_slack_tracking_in_progress,
                             NewJSObjectType new_js_object_type);

  // Check whether this object references another object
  bool ReferencesObject(Tagged<Object> obj);

  V8_WARN_UNUSED_RESULT static Maybe<bool> TestIntegrityLevel(
      Isolate* isolate, DirectHandle<JSObject> object, IntegrityLevel lvl);

  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      Isolate* isolate, DirectHandle<JSObject> object,
      ShouldThrow should_throw);

  static bool IsExtensible(Isolate* isolate, DirectHandle<JSObject> object);

  static MaybeDirectHandle<Object> ReadFromOptionsBag(
      DirectHandle<Object> options, DirectHandle<String> option_name,
      Isolate* isolate);

  // Dispatched behavior.
  void JSObjectShortPrint(StringStream* accumulator);
  DECL_PRINTER(JSObject)
  EXPORT_DECL_VERIFIER(JSObject)
#ifdef OBJECT_PRINT
  bool PrintProperties(std::ostream& os);
  void PrintElements(std::ostream& os);
#endif
#if defined(DEBUG) || defined(OBJECT_PRINT)
  void PrintTransitions(std::ostream& os);
#endif

  static void PrintElementsTransition(
      FILE* file, DirectHandle<JSObject> object, ElementsKind from_kind,
      DirectHandle<FixedArrayBase> from_elements, ElementsKind to_kind,
      DirectHandle<FixedArrayBase> to_elements);

  void PrintInstanceMigration(FILE* file, Tagged<Map> original_map,
                              Tagged<Map> new_map);

#ifdef DEBUG
  // Structure for collecting spill information about JSObjects.
  class SpillInformation {
   public:
    void Clear();
    void Print();
    int number_of_objects_;
    int number_of_objects_with_fast_properties_;
    int number_of_objects_with_fast_elements_;
    int number_of_fast_used_fields_;
    int number_of_fast_unused_fields_;
    int number_of_slow_used_properties_;
    int number_of_slow_unused_properties_;
    int number_of_fast_used_elements_;
    int number_of_fast_unused_elements_;
    int number_of_slow_used_elements_;
    int number_of_slow_unused_elements_;
  };

  void IncrementSpillStatistics(Isolate* isolate, SpillInformation* info);
#endif

#ifdef VERIFY_HEAP
  // If a GC was caused while constructing this object, the elements pointer
  // may point to a one pointer filler map. The object won't be rooted, but
  // our heap verification code could stumble across it.
  V8_EXPORT_PRIVATE bool ElementsAreSafeToExamine() const;
#endif

  Tagged<Object> SlowReverseLookup(Tagged<Object> value);

  // Maximal number of elements (numbered 0 .. kMaxElementCount - 1).
  // Also maximal value of JSArray's length property.
  static constexpr uint32_t kMaxElementCount = kMaxUInt32;
  static constexpr uint32_t kMaxElementIndex = kMaxElementCount - 1;

  // Constants for heuristics controlling conversion of fast elements
  // to slow elements.

  // Maximal gap that can be introduced by adding an element beyond
  // the current elements length.
  static const uint32_t kMaxGap = 1024;

  // Maximal length of fast elements array that won't be checked for
  // being dense enough on expansion.
  static const int kMaxUncheckedFastElementsLength = 5000;

  // Same as above but for old arrays. This limit is more strict. We
  // don't want to be wasteful with long lived objects.
  static const int kMaxUncheckedOldFastElementsLength = 500;

  // This constant applies only to the initial map of "global.Object" and
  // not to arbitrary other JSObject maps.
  static const int kInitialGlobalObjectUnusedPropertiesCount = 4;

  static const int kMaxInstanceSize = 255 * kTaggedSize;
  // kMaxInstanceSize in words must fit in one byte.
  static_assert((kMaxInstanceSize >> kTaggedSizeLog2) <= kMaxUInt8);

  static const int kMapCacheSize = 128;

  // When extending the backing storage for property values, we increase
  // its size by more than the 1 entry necessary, so sequentially adding fields
  // to the same object requires fewer allocations and copies.
  static const int kFieldsAdded = 3;
  static_assert(kMaxNumberOfDescriptors + kFieldsAdded <=
                PropertyArray::kMaxLength);

  static const int kMaxInObjectProperties;
  static const int kMaxFirstInobjectPropertyOffset =
      (1 << kFirstInobjectPropertyOffsetBitCount) - 1;
  static const int kMaxEmbedderFields;
  static const int kMaxJSApiObjectInObjectProperties;
  static const int kMaxJSApiObjectEmbedderFields;

  class BodyDescriptor;

  class FastBodyDescriptor;

  // Gets the number of currently used elements.
  uint32_t GetFastElementsUsage();

  template <typename Dictionary>
  static void ApplyAttributesToDictionary(Isolate* isolate, ReadOnlyRoots roots,
                                          DirectHandle<Dictionary> dictionary,
                                          const PropertyAttributes attributes);

 private:
  friend class JSReceiver;
  friend class Object;

  // Used from Object::GetProperty().
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSAny>
  GetPropertyWithFailedAccessCheck(LookupIterator* it);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithFailedAccessCheck(
      LookupIterator* it, DirectHandle<Object> value,
      Maybe<ShouldThrow> should_throw);

  V8_WARN_UNUSED_RESULT static Maybe<InterceptorResult>
  DeletePropertyWithInterceptor(LookupIterator* it, ShouldThrow should_throw);

  bool ReferencesObjectFromElements(Tagged<FixedArray> elements,
                                    ElementsKind kind, Tagged<Object> object);

  // Helper for fast versions of preventExtensions, seal, and freeze.
  // attrs is one of NONE, SEALED, or FROZEN (depending on the operation).
  template <PropertyAttributes attrs>
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensionsWithTransition(
      Isolate* isolate, DirectHandle<JSObject> object,
      ShouldThrow should_throw);

  inline Tagged<Object> RawFastPropertyAtCompareAndSwapInternal(
      FieldIndex index, Tagged<Object> expected, Tagged<Object> value,
      SeqCstAccessTag tag);

 public:
  TaggedMember<FixedArrayBase> elements_;
} V8_OBJECT_END;

inline constexpr int JSObject::kEndOfStrongFieldsOffset =
    offsetof(JSObject, elements_) + kTaggedSize;
inline constexpr int JSObject::kHeaderSize = sizeof(JSObject);

inline constexpr int JSObject::kMaxInObjectProperties =
    (JSObject::kMaxInstanceSize - JSObject::kHeaderSize) >> kTaggedSizeLog2;
inline constexpr int JSObject::kMaxEmbedderFields =
    (JSObject::kMaxFirstInobjectPropertyOffset - JSObject::kHeaderSize) /
    kEmbedderDataSlotSize;
inline constexpr int JSObject::kMaxJSApiObjectInObjectProperties =
    (JSObject::kMaxInstanceSize - JSObject::kHeaderSize -
     kCppHeapPointerSlotSize) >>
    kTaggedSizeLog2;
inline constexpr int JSObject::kMaxJSApiObjectEmbedderFields =
    (JSObject::kMaxFirstInobjectPropertyOffset - JSObject::kHeaderSize -
     kCppHeapPointerSlotSize) /
    kEmbedderDataSlotSize;

static_assert(offsetof(JSObject, elements_) == sizeof(JSReceiver));
static_assert(JSObject::kHeaderSize == Internals::kJSObjectHeaderSize);
static_assert(JSObject::kMaxInObjectProperties <= kMaxNumberOfDescriptors);
static_assert(JSObject::kHeaderSize + JSObject::kMaxEmbedderFields *
                                          kEmbedderDataSlotSizeInTaggedSlots <=
              JSObject::kMaxInstanceSize);

// An abstract superclass for JSObjects that may contain EmbedderDataSlots.
// Carries no fields itself; the in-instance tail ([embedder fields] followed
// by [in-object properties]) is managed by the Map + BodyDescriptor and never
// shows up as a C++ member.
V8_OBJECT class JSObjectWithEmbedderSlots : public JSObject {
 public:
  DECL_PRINTER(JSObjectWithEmbedderSlots)
  DECL_VERIFIER(JSObjectWithEmbedderSlots)

  static const int kHeaderSize;
} V8_OBJECT_END;

inline constexpr int JSObjectWithEmbedderSlots::kHeaderSize =
    sizeof(JSObjectWithEmbedderSlots);

static_assert(JSObjectWithEmbedderSlots::kHeaderSize == JSObject::kHeaderSize);

// An abstract superclass for JSObjects that may contain EmbedderDataSlots and
// are used as API wrapper objects. Carries the cpp_heap_wrappable field; the
// [embedder fields] + [in-object properties] tail is managed by the Map +
// BodyDescriptor.
V8_OBJECT class JSAPIObjectWithEmbedderSlots : public JSObject {
 public:
  class BodyDescriptor;

  static const int kHeaderSize;

 public:
  CppHeapPointerMember cpp_heap_wrappable_;
} V8_OBJECT_END;

inline constexpr int JSAPIObjectWithEmbedderSlots::kHeaderSize =
    sizeof(JSAPIObjectWithEmbedderSlots);

// An abstract superclass for JSObjects that may have elements while having an
// empty fixed array as elements backing store. It doesn't carry any
// functionality but allows function classes to be identified in the type
// system.
V8_OBJECT class JSCustomElementsObject : public JSObject {
 public:
  static const int kHeaderSize;
} V8_OBJECT_END;

inline constexpr int JSCustomElementsObject::kHeaderSize =
    sizeof(JSCustomElementsObject);

static_assert(JSCustomElementsObject::kHeaderSize == JSObject::kHeaderSize);

// These may also contain EmbedderDataSlots but can't be a child class of
// JSAPIObjectWithEmbedderSlots due to type id constraints. These objects are
// also considered API wrapper objects. Mirrors the
// JSAPIObjectWithEmbedderSlots cpp_heap_wrappable field so CppHeapObjectWrapper
// can access both sibling hierarchies at the same offset (see the
// static_assert in cpp-heap-object-wrapper.h).
V8_OBJECT class JSSpecialObject : public JSCustomElementsObject {
 public:
  static const int kHeaderSize;

 public:
  CppHeapPointerMember cpp_heap_wrappable_;
} V8_OBJECT_END;

inline constexpr int JSSpecialObject::kHeaderSize = sizeof(JSSpecialObject);

// The set of tags that a JSExternalObject's value may carry. Any user-
// facing v8::External pointer maps to a tag in [kFirstExternalTypeTag,
// kLastExternalTypeTag]. Defined here so the ExternalPointerMember field
// below can reference it at template-argument position.
inline constexpr ExternalPointerTagRange kExternalObjectValueTagRange{
    kFirstExternalTypeTag, kLastExternalTypeTag};

// A JSObject created through the public api which wraps an external pointer.
// See v8::External.
V8_OBJECT class JSExternalObject : public JSObject {
 public:
  // [value]: field containing the pointer value.
  inline void* value(ExternalPointerTagRange tag_range) const;
  inline void* value(IsolateForSandbox isolate,
                     ExternalPointerTagRange tag_range) const;
  inline void init_value(IsolateForSandbox isolate, ExternalPointerTag tag,
                         void* initial_value);
  inline void set_value(IsolateForSandbox isolate, ExternalPointerTag tag,
                        void* value);

  DECL_PRINTER(JSExternalObject)
  DECL_VERIFIER(JSExternalObject)

  class BodyDescriptor;

 public:
  ExternalPointerMember<kExternalObjectValueTagRange> value_;
} V8_OBJECT_END;

// JSAccessorPropertyDescriptor is just a JSObject with a specific initial
// map. This initial map adds in-object properties for "get", "set",
// "enumerable" and "configurable" properties, as assigned by the
// FromPropertyDescriptor function for regular accessor properties.
class JSAccessorPropertyDescriptor : public JSObject {
 public:
  // Layout description.
#define JS_ACCESSOR_PROPERTY_DESCRIPTOR_FIELDS(V) \
  V(kGetOffset, kTaggedSize)                      \
  V(kSetOffset, kTaggedSize)                      \
  V(kEnumerableOffset, kTaggedSize)               \
  V(kConfigurableOffset, kTaggedSize)             \
  /* Total size. */                               \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_ACCESSOR_PROPERTY_DESCRIPTOR_FIELDS)
#undef JS_ACCESSOR_PROPERTY_DESCRIPTOR_FIELDS

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSAccessorPropertyDescriptor);
};

// JSDataPropertyDescriptor is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "value", "writable",
// "enumerable" and "configurable" properties, as assigned by the
// FromPropertyDescriptor function for regular data properties.
class JSDataPropertyDescriptor : public JSObject {
 public:
  // Layout description.
#define JS_DATA_PROPERTY_DESCRIPTOR_FIELDS(V) \
  V(kValueOffset, kTaggedSize)                \
  V(kWritableOffset, kTaggedSize)             \
  V(kEnumerableOffset, kTaggedSize)           \
  V(kConfigurableOffset, kTaggedSize)         \
  /* Total size. */                           \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_DATA_PROPERTY_DESCRIPTOR_FIELDS)
#undef JS_DATA_PROPERTY_DESCRIPTOR_FIELDS

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDataPropertyDescriptor);
};

// JSIteratorResult is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "done" and "value",
// as specified by ES6 section 25.1.1.3 The IteratorResult Interface.
class JSIteratorResult : public JSObject {
 public:
  DECL_ACCESSORS(value, Tagged<Object>)

  DECL_ACCESSORS(done, Tagged<Object>)

  // Layout description.
#define JS_ITERATOR_RESULT_FIELDS(V) \
  V(kValueOffset, kTaggedSize)       \
  V(kDoneOffset, kTaggedSize)        \
  /* Total size. */                  \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_ITERATOR_RESULT_FIELDS)
#undef JS_ITERATOR_RESULT_FIELDS

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSIteratorResult);
};

// JSGlobalProxy's prototype must be a JSGlobalObject or null,
// and the prototype is hidden. JSGlobalProxy always delegates
// property accesses to its prototype if the prototype is not null.
//
// A JSGlobalProxy can be reinitialized which will preserve its identity.
//
// Accessing a JSGlobalProxy requires security check.

V8_OBJECT class JSGlobalProxy : public JSSpecialObject {
 public:
  inline bool IsDetachedFrom(Tagged<JSGlobalObject> global) const;
  inline bool IsDetached() const;

  static int SizeWithEmbedderFields(int embedder_field_count);

  // Dispatched behavior.
  DECL_PRINTER(JSGlobalProxy)
  DECL_VERIFIER(JSGlobalProxy)

  // Back-compat.
  static const int kHeaderSize;
} V8_OBJECT_END;

inline constexpr int JSGlobalProxy::kHeaderSize = sizeof(JSGlobalProxy);

// JavaScript global object.
V8_OBJECT class JSGlobalObject : public JSSpecialObject {
 public:
  inline Tagged<JSGlobalProxy> global_proxy() const;
  inline void set_global_proxy(Tagged<JSGlobalProxy> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSGlobalProxy> global_proxy_for_api() const;
  inline void set_global_proxy_for_api(
      Tagged<JSGlobalProxy> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<GlobalDictionary> global_dictionary(AcquireLoadTag) const;
  inline void set_global_dictionary(
      Tagged<GlobalDictionary> value, ReleaseStoreTag,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<HeapObject> raw_global_proxy() const;

  static void InvalidatePropertyCell(DirectHandle<JSGlobalObject> object,
                                     DirectHandle<Name> name);

  inline bool IsDetached();
  inline Tagged<NativeContext> native_context();

  // Dispatched behavior.
  DECL_PRINTER(JSGlobalObject)
  DECL_VERIFIER(JSGlobalObject)

  static const int kHeaderSize;

 public:
  TaggedMember<JSGlobalProxy> global_proxy_;
  TaggedMember<JSGlobalProxy> global_proxy_for_api_;
} V8_OBJECT_END;

inline constexpr int JSGlobalObject::kHeaderSize = sizeof(JSGlobalObject);

// Representation for JS Wrapper objects, String, Number, Boolean, etc.
V8_OBJECT class JSPrimitiveWrapper : public JSCustomElementsObject {
 public:
  inline Tagged<JSAny> value() const;
  inline void set_value(Tagged<JSAny> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Dispatched behavior.
  DECL_PRINTER(JSPrimitiveWrapper)
  DECL_VERIFIER(JSPrimitiveWrapper)

 public:
  TaggedMember<JSAny> value_;
} V8_OBJECT_END;

class DateCache;

// Representation for JS date objects.
V8_OBJECT class JSDate : public JSObject {
 public:
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSDate> New(
      Isolate* isolate, DirectHandle<JSFunction> constructor,
      DirectHandle<JSReceiver> new_target, double tv);

  // Returns the time value (UTC) identifying the current time in milliseconds.
  static int64_t CurrentTimeValue(Isolate* isolate);

  // Returns the date field with the specified index.
  // See FieldIndex for the list of date fields.
  // Arguments and result are raw Address values because this is called
  // via ExternalReference.
  // {raw_date} is a tagged Object pointer.
  // {smi_index} is a tagged Smi.
  // The return value is a tagged Object pointer.
  static Address GetField(Isolate* isolate, Address raw_date,
                          Address smi_index);

  inline double value() const;
  inline void set_value(double v);

  inline Tagged<UnionOf<Smi, HeapNumber>> year() const;
  inline void set_year(Tagged<UnionOf<Smi, HeapNumber>> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Smi, HeapNumber>> month() const;
  inline void set_month(Tagged<UnionOf<Smi, HeapNumber>> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Smi, HeapNumber>> day() const;
  inline void set_day(Tagged<UnionOf<Smi, HeapNumber>> value,
                      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Smi, HeapNumber>> weekday() const;
  inline void set_weekday(Tagged<UnionOf<Smi, HeapNumber>> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Smi, HeapNumber>> hour() const;
  inline void set_hour(Tagged<UnionOf<Smi, HeapNumber>> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Smi, HeapNumber>> min() const;
  inline void set_min(Tagged<UnionOf<Smi, HeapNumber>> value,
                      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Smi, HeapNumber>> sec() const;
  inline void set_sec(Tagged<UnionOf<Smi, HeapNumber>> value,
                      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Smi, HeapNumber>> cache_stamp() const;
  inline void set_cache_stamp(Tagged<UnionOf<Smi, HeapNumber>> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  void SetValue(Isolate* isolate, double v);
  void SetNanValue();

  void UpdateFieldsAfterDeserialization(Isolate* isolate);

  // Dispatched behavior.
  DECL_PRINTER(JSDate)
  EXPORT_DECL_VERIFIER(JSDate)

  // The order is important. It must be kept in sync with date macros
  // in macros.py.
  enum FieldIndex {
    kYear,
    kMonth,
    kDay,
    kWeekday,
    kHour,
    kMinute,
    kSecond,
    kFirstUncachedField,
    kMillisecond = kFirstUncachedField,
    kDays,
    kTimeInDay,
    kFirstUTCField,
    kYearUTC = kFirstUTCField,
    kMonthUTC,
    kDayUTC,
    kWeekdayUTC,
    kHourUTC,
    kMinuteUTC,
    kSecondUTC,
    kMillisecondUTC,
    kDaysUTC,
    kTimeInDayUTC,
    kTimezoneOffset
  };

  class BodyDescriptor;

 private:
  Tagged<Object> DoGetField(Isolate* isolate, FieldIndex index);
  Tagged<Object> GetUTCField(FieldIndex index, double value,
                             DateCache* date_cache);

  // Computes and caches the cacheable fields of the date.
  inline void SetCachedFields(Isolate* isolate, int64_t local_time_ms,
                              DateCache* date_cache);

 public:
  UnalignedDoubleMember value_;
  TaggedMember<UnionOf<Smi, HeapNumber>> year_;
  TaggedMember<UnionOf<Smi, HeapNumber>> month_;
  TaggedMember<UnionOf<Smi, HeapNumber>> day_;
  TaggedMember<UnionOf<Smi, HeapNumber>> weekday_;
  TaggedMember<UnionOf<Smi, HeapNumber>> hour_;
  TaggedMember<UnionOf<Smi, HeapNumber>> min_;
  TaggedMember<UnionOf<Smi, HeapNumber>> sec_;
  TaggedMember<UnionOf<Smi, HeapNumber>> cache_stamp_;
} V8_OBJECT_END;

// Representation of message objects used for error reporting through
// the API. The messages are formatted in JavaScript so this object is
// a real JavaScript object. The information used for formatting the
// error messages are not directly accessible from JavaScript to
// prevent leaking information to user code called during error
// formatting.
V8_OBJECT class JSMessageObject : public JSObject {
 public:
  // [type]: the type of error message.
  inline MessageTemplate type() const;
  inline void set_type(MessageTemplate value);

  // Initializes the source positions in the object if possible. Does nothing if
  // called more than once. If called when stack space is exhausted, then the
  // source positions will be not be set and calling it again when there is more
  // stack space will not have any effect.
  static inline void EnsureSourcePositionsAvailable(
      Isolate* isolate, DirectHandle<JSMessageObject> message);

  // Gets the start and end positions for the message.
  // EnsureSourcePositionsAvailable must have been called before calling these.
  inline int GetStartPosition() const;
  inline int GetEndPosition() const;

  // Returns the line number for the error message (1-based), or
  // Message::kNoLineNumberInfo if the line cannot be determined.
  // EnsureSourcePositionsAvailable must have been called before calling this.
  V8_EXPORT_PRIVATE int GetLineNumber() const;

  // Returns the offset of the given position within the containing line.
  // EnsureSourcePositionsAvailable must have been called before calling this.
  V8_EXPORT_PRIVATE int GetColumnNumber() const;

  // Returns the source code
  V8_EXPORT_PRIVATE Tagged<String> GetSource() const;

  // Returns the source code line containing the given source
  // position, or the empty string if the position is invalid.
  // EnsureSourcePositionsAvailable must have been called before calling this.
  DirectHandle<String> GetSourceLine() const;

  inline Tagged<Object> argument() const;
  inline void set_argument(Tagged<Object> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Script> script() const;
  inline void set_script(Tagged<Script> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<StackTraceInfo, Hole>> stack_trace() const;
  inline void set_stack_trace(Tagged<UnionOf<StackTraceInfo, Hole>> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int error_level() const;
  inline void set_error_level(int value);

  // Dispatched behavior.
  DECL_PRINTER(JSMessageObject)
  DECL_VERIFIER(JSMessageObject)

  class BodyDescriptor;

 private:
  friend class Factory;

  inline bool DidEnsureSourcePositionsAvailable() const;
  static void V8_PRESERVE_MOST V8_EXPORT_PRIVATE InitializeSourcePositions(
      Isolate* isolate, DirectHandle<JSMessageObject> message);

  // [shared]: optional SharedFunctionInfo that can be used to reconstruct the
  // source position if not available when the message was generated.
  inline Tagged<Object> shared_info() const;
  inline void set_shared_info(Tagged<Object> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [bytecode_offset]: optional offset using along with |shared| to generation
  // source positions.
  inline Tagged<Smi> bytecode_offset() const;
  inline void set_bytecode_offset(Tagged<Smi> value);

  // [start_position]: the start position in the script for the error message.
  inline int start_position() const;
  inline void set_start_position(int value);

  // [end_position]: the end position in the script for the error message.
  inline int end_position() const;
  inline void set_end_position(int value);

  inline int raw_type() const;
  inline void set_raw_type(int value);

 public:
  TaggedMember<Smi> message_type_;
  TaggedMember<Object> argument_;
  TaggedMember<Script> script_;
  TaggedMember<UnionOf<StackTraceInfo, Hole>> stack_trace_;
  TaggedMember<UnionOf<SharedFunctionInfo, Smi>> shared_info_;
  // Raw data fields below (treated as non-pointers by BodyDescriptor).
  TaggedMember<Smi> bytecode_offset_;
  TaggedMember<Smi> start_position_;
  TaggedMember<Smi> end_position_;
  TaggedMember<Smi> error_level_;
} V8_OBJECT_END;

// The [Async-from-Sync Iterator] object
// (https://tc39.es/proposal-async-iteration/#sec-async-from-sync-iterator-objects)
// An object which wraps an ordinary Iterator and converts it to behave
// according to the Async Iterator protocol.
// (See https://tc39.es/proposal-async-iteration/#sec-iteration)
V8_OBJECT class JSAsyncFromSyncIterator : public JSObject {
 public:
  // Async-from-Sync Iterator instances are ordinary objects that inherit
  // properties from the %AsyncFromSyncIteratorPrototype% intrinsic object.
  // Async-from-Sync Iterator instances are initially created with the internal
  // slots listed in Table 4.
  // (https://tc39.es/proposal-async-iteration/#table-async-from-sync-iterator-internal-slots)
  inline Tagged<JSReceiver> sync_iterator() const;
  inline void set_sync_iterator(Tagged<JSReceiver> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // The "next" method is loaded during GetIterator, and is not reloaded for
  // subsequent "next" invocations.
  inline Tagged<Object> next() const;
  inline void set_next(Tagged<Object> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSAsyncFromSyncIterator)
  DECL_VERIFIER(JSAsyncFromSyncIterator)

 public:
  TaggedMember<JSReceiver> sync_iterator_;
  TaggedMember<Object> next_;
} V8_OBJECT_END;

V8_OBJECT class JSStringIterator : public JSObject {
 public:
  inline Tagged<String> string() const;
  inline void set_string(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int index() const;
  inline void set_index(int value);

  // Dispatched behavior.
  DECL_PRINTER(JSStringIterator)
  DECL_VERIFIER(JSStringIterator)

 public:
  TaggedMember<String> string_;
  TaggedMember<Smi> index_;
} V8_OBJECT_END;

// The valid iterator wrapper is the wrapper object created by
// Iterator.from(obj), which attempts to wrap iterator-like objects into an
// actual iterator with %Iterator.prototype%.
V8_OBJECT class JSValidIteratorWrapper : public JSObject {
 public:
  // The [[Iterated]] slot, modelled as the two fields of an
  // iterator::IteratorRecord struct (object + next).
  inline Tagged<JSReceiver> underlying_object() const;
  inline void set_underlying_object(
      Tagged<JSReceiver> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSAny> underlying_next() const;
  inline void set_underlying_next(Tagged<JSAny> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSValidIteratorWrapper)
  DECL_VERIFIER(JSValidIteratorWrapper)

 public:
  TaggedMember<JSReceiver> underlying_object_;
  TaggedMember<JSAny> underlying_next_;
} V8_OBJECT_END;

// JSPromiseWithResolversResult is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "promise", "resolve", and
// "reject", in that order.
class JSPromiseWithResolversResult : public JSObject {
 public:
  DECL_ACCESSORS(promise, Tagged<Object>)

  DECL_ACCESSORS(resolve, Tagged<Object>)

  DECL_ACCESSORS(reject, Tagged<Object>)

  // Layout description.
#define JS_PROMISE_WITHRESOLVERS_RESULT_FIELDS(V) \
  V(kPromiseOffset, kTaggedSize)                  \
  V(kResolveOffset, kTaggedSize)                  \
  V(kRejectOffset, kTaggedSize)                   \
  /* Total size. */                               \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_PROMISE_WITHRESOLVERS_RESULT_FIELDS)
#undef JS_PROMISE_WITHRESOLVERS_RESULT_FIELDS

  // Indices of in-object properties.
  static const int kPromiseIndex = 0;
  static const int kResolveIndex = 1;
  static const int kRejectIndex = 2;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSPromiseWithResolversResult);
};

// JSUint8ArraySetFromResult is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "read" and "written",
class JSUint8ArraySetFromResult : public JSObject {
 public:
  DECL_ACCESSORS(read, Tagged<Object>)

  DECL_ACCESSORS(written, Tagged<Object>)

  // Layout description.
#define JS_UINT8_ARRAY_SET_FROM_RESULT_FIELDS(V) \
  V(kReadOffset, kTaggedSize)                    \
  V(kWrittenOffset, kTaggedSize)                 \
  /* Total size. */                              \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_UINT8_ARRAY_SET_FROM_RESULT_FIELDS)
#undef JS_UINT8_ARRAY_SET_FROM_RESULT_FIELDS

  // Indices of in-object properties.
  static const int kReadIndex = 0;
  static const int kWrittenIndex = 1;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSUint8ArraySetFromResult);
};

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_OBJECTS_H_
