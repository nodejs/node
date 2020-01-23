// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_OBJECTS_H_
#define V8_OBJECTS_JS_OBJECTS_H_

#include "src/objects/embedder-data-slot.h"
// TODO(jkummerow): Consider forward-declaring instead.
#include "src/objects/internal-index.h"
#include "src/objects/objects.h"
#include "src/objects/property-array.h"
#include "torque-generated/class-definitions-tq.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

enum InstanceType : uint16_t;
class JSGlobalObject;
class JSGlobalProxy;
class NativeContext;

// JSReceiver includes types on which properties can be defined, i.e.,
// JSObject and JSProxy.
class JSReceiver : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE
  // Returns true if there is no slow (ie, dictionary) backing store.
  DECL_GETTER(HasFastProperties, bool)

  // Returns the properties array backing store if it
  // exists. Otherwise, returns an empty_property_array when there's a
  // Smi (hash code) or an empty_fixed_array for a fast properties
  // map.
  DECL_GETTER(property_array, PropertyArray)

  // Gets slow properties for non-global objects.
  DECL_GETTER(property_dictionary, NameDictionary)

  // Sets the properties backing store and makes sure any existing hash is moved
  // to the new properties store. To clear out the properties store, pass in the
  // empty_fixed_array(), the hash will be maintained in this case as well.
  void SetProperties(HeapObject properties);

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
  DECL_ACCESSORS(raw_properties_or_hash, Object)

  inline void initialize_properties(Isolate* isolate);

  // Deletes an existing named property in a normalized object.
  static void DeleteNormalizedProperty(Handle<JSReceiver> object, int entry);

  DECL_CAST(JSReceiver)
  DECL_VERIFIER(JSReceiver)

  // ES6 section 7.1.1 ToPrimitive
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ToPrimitive(
      Handle<JSReceiver> receiver,
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

  // ES6 section 7.1.1.1 OrdinaryToPrimitive
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> OrdinaryToPrimitive(
      Handle<JSReceiver> receiver, OrdinaryToPrimitiveHint hint);

  static MaybeHandle<NativeContext> GetFunctionRealm(
      Handle<JSReceiver> receiver);
  V8_EXPORT_PRIVATE static MaybeHandle<NativeContext> GetContextForMicrotask(
      Handle<JSReceiver> receiver);

  // Get the first non-hidden prototype.
  static inline MaybeHandle<HeapObject> GetPrototype(
      Isolate* isolate, Handle<JSReceiver> receiver);

  V8_WARN_UNUSED_RESULT static Maybe<bool> HasInPrototypeChain(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> proto);

  // Reads all enumerable own properties of source and adds them to
  // target, using either Set or CreateDataProperty depending on the
  // use_set argument. This only copies values not present in the
  // maybe_excluded_properties list.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetOrCopyDataProperties(
      Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
      const ScopedVector<Handle<Object>>* excluded_properties = nullptr,
      bool use_set = true);

  // Implementation of [[HasProperty]], ECMA-262 5th edition, section 8.12.6.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> HasProperty(
      LookupIterator* it);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasProperty(
      Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasElement(
      Handle<JSReceiver> object, uint32_t index);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> HasOwnProperty(
      Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasOwnProperty(
      Handle<JSReceiver> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, const char* key);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetElement(
      Isolate* isolate, Handle<JSReceiver> receiver, uint32_t index);

  // Implementation of ES6 [[Delete]]
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool>
  DeletePropertyOrElement(Handle<JSReceiver> object, Handle<Name> name,
                          LanguageMode language_mode = LanguageMode::kSloppy);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteProperty(
      Handle<JSReceiver> object, Handle<Name> name,
      LanguageMode language_mode = LanguageMode::kSloppy);
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteProperty(
      LookupIterator* it, LanguageMode language_mode);
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteElement(
      Handle<JSReceiver> object, uint32_t index,
      LanguageMode language_mode = LanguageMode::kSloppy);

  V8_WARN_UNUSED_RESULT static Object DefineProperty(Isolate* isolate,
                                                     Handle<Object> object,
                                                     Handle<Object> name,
                                                     Handle<Object> attributes);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> DefineProperties(
      Isolate* isolate, Handle<Object> object, Handle<Object> properties);

  // "virtual" dispatcher to the correct [[DefineOwnProperty]] implementation.
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> key,
      PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw);

  // ES6 7.3.4 (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Name> key,
      Handle<Object> value, Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      LookupIterator* it, Handle<Object> value,
      Maybe<ShouldThrow> should_throw);

  // ES6 9.1.6.1
  V8_WARN_UNUSED_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      Isolate* isolate, Handle<JSObject> object, Handle<Object> key,
      PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      LookupIterator* it, PropertyDescriptor* desc,
      Maybe<ShouldThrow> should_throw);
  // ES6 9.1.6.2
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsCompatiblePropertyDescriptor(
      Isolate* isolate, bool extensible, PropertyDescriptor* desc,
      PropertyDescriptor* current, Handle<Name> property_name,
      Maybe<ShouldThrow> should_throw);
  // ES6 9.1.6.3
  // |it| can be NULL in cases where the ES spec passes |undefined| as the
  // receiver. Exactly one of |it| and |property_name| must be provided.
  V8_WARN_UNUSED_RESULT static Maybe<bool> ValidateAndApplyPropertyDescriptor(
      Isolate* isolate, LookupIterator* it, bool extensible,
      PropertyDescriptor* desc, PropertyDescriptor* current,
      Maybe<ShouldThrow> should_throw, Handle<Name> property_name);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool>
  GetOwnPropertyDescriptor(Isolate* isolate, Handle<JSReceiver> object,
                           Handle<Object> key, PropertyDescriptor* desc);
  V8_WARN_UNUSED_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      LookupIterator* it, PropertyDescriptor* desc);

  using IntegrityLevel = PropertyAttributes;

  // ES6 7.3.14 (when passed kDontThrow)
  // 'level' must be SEALED or FROZEN.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetIntegrityLevel(
      Handle<JSReceiver> object, IntegrityLevel lvl, ShouldThrow should_throw);

  // ES6 7.3.15
  // 'level' must be SEALED or FROZEN.
  V8_WARN_UNUSED_RESULT static Maybe<bool> TestIntegrityLevel(
      Handle<JSReceiver> object, IntegrityLevel lvl);

  // ES6 [[PreventExtensions]] (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSReceiver> object, ShouldThrow should_throw);

  V8_WARN_UNUSED_RESULT static Maybe<bool> IsExtensible(
      Handle<JSReceiver> object);

  // Returns the class name ([[Class]] property in the specification).
  V8_EXPORT_PRIVATE String class_name();

  // Returns the constructor (the function that was used to instantiate the
  // object).
  static MaybeHandle<JSFunction> GetConstructor(Handle<JSReceiver> receiver);

  // Returns the constructor name (the name (possibly, inferred name) of the
  // function that was used to instantiate the object).
  static Handle<String> GetConstructorName(Handle<JSReceiver> receiver);

  V8_EXPORT_PRIVATE Handle<NativeContext> GetCreationContext();

  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetPropertyAttributes(Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Handle<JSReceiver> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetElementAttributes(Handle<JSReceiver> object, uint32_t index);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnElementAttributes(Handle<JSReceiver> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Handle<JSReceiver> object, Handle<Object> value, bool from_javascript,
      ShouldThrow should_throw);

  inline static Handle<Object> GetDataProperty(Handle<JSReceiver> object,
                                               Handle<Name> name);
  V8_EXPORT_PRIVATE static Handle<Object> GetDataProperty(LookupIterator* it);

  // Retrieves a permanent object identity hash code. The undefined value might
  // be returned in case no hash was created yet.
  V8_EXPORT_PRIVATE Object GetIdentityHash();

  // Retrieves a permanent object identity hash code. May create and store a
  // hash code if needed and none exists.
  static Smi CreateIdentityHash(Isolate* isolate, JSReceiver key);
  V8_EXPORT_PRIVATE Smi GetOrCreateIdentityHash(Isolate* isolate);

  // Stores the hash code. The hash passed in must be masked with
  // JSReceiver::kHashMask.
  V8_EXPORT_PRIVATE void SetIdentityHash(int masked_hash);

  // ES6 [[OwnPropertyKeys]] (modulo return type)
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<FixedArray> OwnPropertyKeys(
      Handle<JSReceiver> object);

  V8_WARN_UNUSED_RESULT static MaybeHandle<FixedArray> GetOwnValues(
      Handle<JSReceiver> object, PropertyFilter filter,
      bool try_fast_path = true);

  V8_WARN_UNUSED_RESULT static MaybeHandle<FixedArray> GetOwnEntries(
      Handle<JSReceiver> object, PropertyFilter filter,
      bool try_fast_path = true);

  static const int kHashMask = PropertyArray::HashField::kMask;

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_JS_RECEIVER_FIELDS)
  bool HasProxyInPrototype(Isolate* isolate);

  V8_WARN_UNUSED_RESULT static MaybeHandle<FixedArray> GetPrivateEntries(
      Isolate* isolate, Handle<JSReceiver> receiver);

  OBJECT_CONSTRUCTORS(JSReceiver, HeapObject);
};

// The JSObject describes real heap allocated JavaScript objects with
// properties.
// Note that the map of JSObject changes during execution to enable inline
// caching.
class JSObject : public TorqueGeneratedJSObject<JSObject, JSReceiver> {
 public:
  static bool IsUnmodifiedApiObject(FullObjectSlot o);

  V8_EXPORT_PRIVATE static V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> New(
      Handle<JSFunction> constructor, Handle<JSReceiver> new_target,
      Handle<AllocationSite> site);

  static MaybeHandle<NativeContext> GetFunctionRealm(Handle<JSObject> object);

  // 9.1.12 ObjectCreate ( proto [ , internalSlotsList ] )
  // Notice: This is NOT 19.1.2.2 Object.create ( O, Properties )
  static V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> ObjectCreate(
      Isolate* isolate, Handle<Object> prototype);

  inline void initialize_elements();
  static inline void SetMapAndElements(Handle<JSObject> object, Handle<Map> map,
                                       Handle<FixedArrayBase> elements);
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
  DECL_GETTER(HasNonextensibleElements, bool)

  DECL_GETTER(HasTypedArrayElements, bool)

  DECL_GETTER(HasFixedUint8ClampedElements, bool)
  DECL_GETTER(HasFixedArrayElements, bool)
  DECL_GETTER(HasFixedInt8Elements, bool)
  DECL_GETTER(HasFixedUint8Elements, bool)
  DECL_GETTER(HasFixedInt16Elements, bool)
  DECL_GETTER(HasFixedUint16Elements, bool)
  DECL_GETTER(HasFixedInt32Elements, bool)
  DECL_GETTER(HasFixedUint32Elements, bool)
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
  DECL_GETTER(element_dictionary, NumberDictionary)

  // Requires: HasFastElements().
  static void EnsureWritableFastElements(Handle<JSObject> object);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithInterceptor(
      LookupIterator* it, Maybe<ShouldThrow> should_throw,
      Handle<Object> value);

  // The API currently still wants DefineOwnPropertyIgnoreAttributes to convert
  // AccessorInfo objects to data fields. We allow FORCE_FIELD as an exception
  // to the default behavior that calls the setter.
  enum AccessorInfoHandling { FORCE_FIELD, DONT_FORCE_FIELD };

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      AccessorInfoHandling handling = DONT_FORCE_FIELD);

  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      Maybe<ShouldThrow> should_throw,
      AccessorInfoHandling handling = DONT_FORCE_FIELD);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> V8_EXPORT_PRIVATE
  SetOwnPropertyIgnoreAttributes(Handle<JSObject> object, Handle<Name> name,
                                 Handle<Object> value,
                                 PropertyAttributes attributes);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  SetOwnElementIgnoreAttributes(Handle<JSObject> object, uint32_t index,
                                Handle<Object> value,
                                PropertyAttributes attributes);

  // Equivalent to one of the above depending on whether |name| can be converted
  // to an array index.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  DefinePropertyOrElementIgnoreAttributes(Handle<JSObject> object,
                                          Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes = NONE);

  // Adds or reconfigures a property to attributes NONE. It will fail when it
  // cannot.
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      LookupIterator* it, Handle<Object> value,
      Maybe<ShouldThrow> should_throw = Just(kDontThrow));

  V8_EXPORT_PRIVATE static void AddProperty(Isolate* isolate,
                                            Handle<JSObject> object,
                                            Handle<Name> name,
                                            Handle<Object> value,
                                            PropertyAttributes attributes);

  // {name} must be a UTF-8 encoded, null-terminated string.
  static void AddProperty(Isolate* isolate, Handle<JSObject> object,
                          const char* name, Handle<Object> value,
                          PropertyAttributes attributes);

  V8_EXPORT_PRIVATE static void AddDataElement(Handle<JSObject> receiver,
                                               uint32_t index,
                                               Handle<Object> value,
                                               PropertyAttributes attributes);

  // Extend the receiver with a single fast property appeared first in the
  // passed map. This also extends the property backing store if necessary.
  static void AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map);

  // Migrates the given object to a map whose field representations are the
  // lowest upper bound of all known representations for that field.
  static void MigrateInstance(Isolate* isolate, Handle<JSObject> instance);

  // Migrates the given object only if the target map is already available,
  // or returns false if such a map is not yet available.
  static bool TryMigrateInstance(Isolate* isolate, Handle<JSObject> instance);

  // Sets the property value in a normalized object given (key, value, details).
  // Handles the special representation of JS global objects.
  static void SetNormalizedProperty(Handle<JSObject> object, Handle<Name> name,
                                    Handle<Object> value,
                                    PropertyDetails details);
  static void SetDictionaryElement(Handle<JSObject> object, uint32_t index,
                                   Handle<Object> value,
                                   PropertyAttributes attributes);
  static void SetDictionaryArgumentsElement(Handle<JSObject> object,
                                            uint32_t index,
                                            Handle<Object> value,
                                            PropertyAttributes attributes);

  static void OptimizeAsPrototype(Handle<JSObject> object,
                                  bool enable_setup_mode = true);
  static void ReoptimizeIfPrototype(Handle<JSObject> object);
  static void MakePrototypesFast(Handle<Object> receiver,
                                 WhereToStart where_to_start, Isolate* isolate);
  static void LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate);
  static void UpdatePrototypeUserRegistration(Handle<Map> old_map,
                                              Handle<Map> new_map,
                                              Isolate* isolate);
  static bool UnregisterPrototypeUser(Handle<Map> user, Isolate* isolate);
  static Map InvalidatePrototypeChains(Map map);
  static void InvalidatePrototypeValidityCell(JSGlobalObject global);

  // Updates prototype chain tracking information when an object changes its
  // map from |old_map| to |new_map|.
  static void NotifyMapChange(Handle<Map> old_map, Handle<Map> new_map,
                              Isolate* isolate);

  // Utility used by many Array builtins and runtime functions
  static inline bool PrototypeHasNoElements(Isolate* isolate, JSObject object);

  // To be passed to PrototypeUsers::Compact.
  static void PrototypeRegistryCompactionCallback(HeapObject value,
                                                  int old_index, int new_index);

  // Retrieve interceptors.
  DECL_GETTER(GetNamedInterceptor, InterceptorInfo)
  DECL_GETTER(GetIndexedInterceptor, InterceptorInfo)

  // Used from JSReceiver.
  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes>
  GetPropertyAttributesWithInterceptor(LookupIterator* it);
  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes>
  GetPropertyAttributesWithFailedAccessCheck(LookupIterator* it);

  // Defines an AccessorPair property on the given object.
  // TODO(mstarzinger): Rename to SetAccessor().
  V8_EXPORT_PRIVATE static MaybeHandle<Object> DefineAccessor(
      Handle<JSObject> object, Handle<Name> name, Handle<Object> getter,
      Handle<Object> setter, PropertyAttributes attributes);
  static MaybeHandle<Object> DefineAccessor(LookupIterator* it,
                                            Handle<Object> getter,
                                            Handle<Object> setter,
                                            PropertyAttributes attributes);

  // Defines an AccessorInfo property on the given object.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> SetAccessor(
      Handle<JSObject> object, Handle<Name> name, Handle<AccessorInfo> info,
      PropertyAttributes attributes);

  // The result must be checked first for exceptions. If there's no exception,
  // the output parameter |done| indicates whether the interceptor has a result
  // or not.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetPropertyWithInterceptor(
      LookupIterator* it, bool* done);

  static void ValidateElements(JSObject object);

  // Makes sure that this object can contain HeapObject as elements.
  static inline void EnsureCanContainHeapObjectElements(Handle<JSObject> obj);

  // Makes sure that this object can contain the specified elements.
  // TSlot here is either ObjectSlot or FullObjectSlot.
  template <typename TSlot>
  static inline void EnsureCanContainElements(Handle<JSObject> object,
                                              TSlot elements, uint32_t count,
                                              EnsureElementsMode mode);
  static inline void EnsureCanContainElements(Handle<JSObject> object,
                                              Handle<FixedArrayBase> elements,
                                              uint32_t length,
                                              EnsureElementsMode mode);
  static void EnsureCanContainElements(Handle<JSObject> object,
                                       Arguments* arguments, uint32_t first_arg,
                                       uint32_t arg_count,
                                       EnsureElementsMode mode);

  // Would we convert a fast elements array to dictionary mode given
  // an access at key?
  bool WouldConvertToSlowElements(uint32_t index);

  static const uint32_t kMinAddedElementsCapacity = 16;

  // Computes the new capacity when expanding the elements of a JSObject.
  static uint32_t NewElementsCapacity(uint32_t old_capacity) {
    // (old_capacity + 50%) + kMinAddedElementsCapacity
    return old_capacity + (old_capacity >> 1) + kMinAddedElementsCapacity;
  }

  // These methods do not perform access checks!
  template <AllocationSiteUpdateMode update_or_check =
                AllocationSiteUpdateMode::kUpdate>
  static bool UpdateAllocationSite(Handle<JSObject> object,
                                   ElementsKind to_kind);

  // Lookup interceptors are used for handling properties controlled by host
  // objects.
  DECL_GETTER(HasNamedInterceptor, bool)
  DECL_GETTER(HasIndexedInterceptor, bool)

  // Support functions for v8 api (needed for correct interceptor behavior).
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealNamedProperty(
      Handle<JSObject> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealElementProperty(
      Handle<JSObject> object, uint32_t index);
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealNamedCallbackProperty(
      Handle<JSObject> object, Handle<Name> name);

  // Get the header size for a JSObject.  Used to compute the index of
  // embedder fields as well as the number of embedder fields.
  // The |function_has_prototype_slot| parameter is needed only for
  // JSFunction objects.
  static int GetHeaderSize(InstanceType instance_type,
                           bool function_has_prototype_slot = false);
  static inline int GetHeaderSize(Map map);

  static inline int GetEmbedderFieldsStartOffset(Map map);
  inline int GetEmbedderFieldsStartOffset();

  static inline int GetEmbedderFieldCount(Map map);
  inline int GetEmbedderFieldCount() const;
  inline int GetEmbedderFieldOffset(int index);
  inline Object GetEmbedderField(int index);
  inline void SetEmbedderField(int index, Object value);
  inline void SetEmbedderField(int index, Smi value);

  // Returns true when the object is potentially a wrapper that gets special
  // garbage collection treatment.
  // TODO(mlippautz): Make check exact and replace the pattern match in
  // Heap::TracePossibleWrapper.
  bool IsApiWrapper();

  // Same as IsApiWrapper() but also allow dropping the wrapper on minor GCs.
  bool IsDroppableApiWrapper();

  // Returns a new map with all transitions dropped from the object's current
  // map and the ElementsKind set.
  static Handle<Map> GetElementsTransitionMap(Handle<JSObject> object,
                                              ElementsKind to_kind);
  V8_EXPORT_PRIVATE static void TransitionElementsKind(Handle<JSObject> object,
                                                       ElementsKind to_kind);

  // Always use this to migrate an object to a new map.
  // |expected_additional_properties| is only used for fast-to-slow transitions
  // and ignored otherwise.
  V8_EXPORT_PRIVATE static void MigrateToMap(
      Isolate* isolate, Handle<JSObject> object, Handle<Map> new_map,
      int expected_additional_properties = 0);

  // Forces a prototype without any of the checks that the regular SetPrototype
  // would do.
  static void ForceSetPrototype(Handle<JSObject> object,
                                Handle<HeapObject> proto);

  // Convert the object to use the canonical dictionary
  // representation. If the object is expected to have additional properties
  // added this number can be indicated to have the backing store allocated to
  // an initial capacity for holding these properties.
  V8_EXPORT_PRIVATE static void NormalizeProperties(
      Isolate* isolate, Handle<JSObject> object, PropertyNormalizationMode mode,
      int expected_additional_properties, const char* reason);

  // Convert and update the elements backing store to be a
  // NumberDictionary dictionary.  Returns the backing after conversion.
  V8_EXPORT_PRIVATE static Handle<NumberDictionary> NormalizeElements(
      Handle<JSObject> object);

  void RequireSlowElements(NumberDictionary dictionary);

  // Transform slow named properties to fast variants.
  V8_EXPORT_PRIVATE static void MigrateSlowToFast(Handle<JSObject> object,
                                                  int unused_property_fields,
                                                  const char* reason);

  inline bool IsUnboxedDoubleField(FieldIndex index) const;
  inline bool IsUnboxedDoubleField(Isolate* isolate, FieldIndex index) const;

  // Access fast-case object properties at index.
  static Handle<Object> FastPropertyAt(Handle<JSObject> object,
                                       Representation representation,
                                       FieldIndex index);
  inline Object RawFastPropertyAt(FieldIndex index) const;
  inline Object RawFastPropertyAt(Isolate* isolate, FieldIndex index) const;
  inline double RawFastDoublePropertyAt(FieldIndex index) const;
  inline uint64_t RawFastDoublePropertyAsBitsAt(FieldIndex index) const;

  inline void FastPropertyAtPut(FieldIndex index, Object value);
  inline void RawFastPropertyAtPut(
      FieldIndex index, Object value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void RawFastInobjectPropertyAtPut(
      FieldIndex index, Object value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void RawFastDoublePropertyAsBitsAtPut(FieldIndex index, uint64_t bits);
  inline void WriteToField(InternalIndex descriptor, PropertyDetails details,
                           Object value);

  // Access to in object properties.
  inline int GetInObjectPropertyOffset(int index);
  inline Object InObjectPropertyAt(int index);
  inline Object InObjectPropertyAtPut(
      int index, Object value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Handle<JSObject> object, Handle<Object> value, bool from_javascript,
      ShouldThrow should_throw);

  // Makes the object prototype immutable
  // Never called from JavaScript
  static void SetImmutableProto(Handle<JSObject> object);

  // Initializes the body starting at |start_offset|. It is responsibility of
  // the caller to initialize object header. Fill the pre-allocated fields with
  // pre_allocated_value and the rest with filler_value.
  // Note: this call does not update write barrier, the caller is responsible
  // to ensure that |filler_value| can be collected without WB here.
  inline void InitializeBody(Map map, int start_offset,
                             Object pre_allocated_value, Object filler_value);

  // Check whether this object references another object
  bool ReferencesObject(Object obj);

  V8_WARN_UNUSED_RESULT static Maybe<bool> TestIntegrityLevel(
      Handle<JSObject> object, IntegrityLevel lvl);

  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSObject> object, ShouldThrow should_throw);

  static bool IsExtensible(Handle<JSObject> object);

  // Dispatched behavior.
  void JSObjectShortPrint(StringStream* accumulator);
  DECL_PRINTER(JSObject)
  DECL_VERIFIER(JSObject)
#ifdef OBJECT_PRINT
  bool PrintProperties(std::ostream& os);  // NOLINT
  void PrintElements(std::ostream& os);    // NOLINT
#endif
#if defined(DEBUG) || defined(OBJECT_PRINT)
  void PrintTransitions(std::ostream& os);  // NOLINT
#endif

  static void PrintElementsTransition(FILE* file, Handle<JSObject> object,
                                      ElementsKind from_kind,
                                      Handle<FixedArrayBase> from_elements,
                                      ElementsKind to_kind,
                                      Handle<FixedArrayBase> to_elements);

  void PrintInstanceMigration(FILE* file, Map original_map, Map new_map);

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
  V8_EXPORT_PRIVATE bool ElementsAreSafeToExamine(Isolate* isolate) const;
#endif

  Object SlowReverseLookup(Object value);

  // Maximal number of elements (numbered 0 .. kMaxElementCount - 1).
  // Also maximal value of JSArray's length property.
  static const uint32_t kMaxElementCount = 0xffffffffu;

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

  // When extending the backing storage for property values, we increase
  // its size by more than the 1 entry necessary, so sequentially adding fields
  // to the same object requires fewer allocations and copies.
  static const int kFieldsAdded = 3;
  STATIC_ASSERT(kMaxNumberOfDescriptors + kFieldsAdded <=
                PropertyArray::kMaxLength);

  STATIC_ASSERT(kHeaderSize == Internals::kJSObjectHeaderSize);
  static const int kMaxInObjectProperties =
      (kMaxInstanceSize - kHeaderSize) >> kTaggedSizeLog2;
  STATIC_ASSERT(kMaxInObjectProperties <= kMaxNumberOfDescriptors);

  static const int kMaxFirstInobjectPropertyOffset =
      (1 << kFirstInobjectPropertyOffsetBitCount) - 1;
  static const int kMaxEmbedderFields =
      (kMaxFirstInobjectPropertyOffset - kHeaderSize) / kEmbedderDataSlotSize;
  STATIC_ASSERT(kHeaderSize +
                    kMaxEmbedderFields * kEmbedderDataSlotSizeInTaggedSlots <=
                kMaxInstanceSize);

  class BodyDescriptor;

  class FastBodyDescriptor;

  // Gets the number of currently used elements.
  int GetFastElementsUsage();

  static bool AllCanRead(LookupIterator* it);
  static bool AllCanWrite(LookupIterator* it);

  template <typename Dictionary>
  static void ApplyAttributesToDictionary(Isolate* isolate, ReadOnlyRoots roots,
                                          Handle<Dictionary> dictionary,
                                          const PropertyAttributes attributes);

 private:
  friend class JSReceiver;
  friend class Object;

  // Used from Object::GetProperty().
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  GetPropertyWithFailedAccessCheck(LookupIterator* it);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithFailedAccessCheck(
      LookupIterator* it, Handle<Object> value,
      Maybe<ShouldThrow> should_throw);

  V8_WARN_UNUSED_RESULT static Maybe<bool> DeletePropertyWithInterceptor(
      LookupIterator* it, ShouldThrow should_throw);

  bool ReferencesObjectFromElements(FixedArray elements, ElementsKind kind,
                                    Object object);

  // Helper for fast versions of preventExtensions, seal, and freeze.
  // attrs is one of NONE, SEALED, or FROZEN (depending on the operation).
  template <PropertyAttributes attrs>
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensionsWithTransition(
      Handle<JSObject> object, ShouldThrow should_throw);

  TQ_OBJECT_CONSTRUCTORS(JSObject)
};

// An abstract superclass for JSObjects that may have elements while having an
// empty fixed array as elements backing store. It doesn't carry any
// functionality but allows function classes to be identified in the type
// system.
class JSCustomElementsObject
    : public TorqueGeneratedJSCustomElementsObject<JSCustomElementsObject,
                                                   JSObject> {
 public:
  STATIC_ASSERT(kHeaderSize == JSObject::kHeaderSize);
  TQ_OBJECT_CONSTRUCTORS(JSCustomElementsObject)
};

// An abstract superclass for JSObjects that require non-standard element
// access. It doesn't carry any functionality but allows function classes to be
// identified in the type system.
class JSSpecialObject
    : public TorqueGeneratedJSSpecialObject<JSSpecialObject,
                                            JSCustomElementsObject> {
 public:
  STATIC_ASSERT(kHeaderSize == JSObject::kHeaderSize);
  TQ_OBJECT_CONSTRUCTORS(JSSpecialObject)
};

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

  // Indices of in-object properties.
  static const int kGetIndex = 0;
  static const int kSetIndex = 1;
  static const int kEnumerableIndex = 2;
  static const int kConfigurableIndex = 3;

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

  // Indices of in-object properties.
  static const int kValueIndex = 0;
  static const int kWritableIndex = 1;
  static const int kEnumerableIndex = 2;
  static const int kConfigurableIndex = 3;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDataPropertyDescriptor);
};

// JSIteratorResult is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "done" and "value",
// as specified by ES6 section 25.1.1.3 The IteratorResult Interface.
class JSIteratorResult : public JSObject {
 public:
  DECL_ACCESSORS(value, Object)

  DECL_ACCESSORS(done, Object)

  // Layout description.
#define JS_ITERATOR_RESULT_FIELDS(V) \
  V(kValueOffset, kTaggedSize)       \
  V(kDoneOffset, kTaggedSize)        \
  /* Total size. */                  \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_ITERATOR_RESULT_FIELDS)
#undef JS_ITERATOR_RESULT_FIELDS

  // Indices of in-object properties.
  static const int kValueIndex = 0;
  static const int kDoneIndex = 1;

  DECL_CAST(JSIteratorResult)

  OBJECT_CONSTRUCTORS(JSIteratorResult, JSObject);
};

// An abstract superclass for classes representing JavaScript function values.
// It doesn't carry any functionality but allows function classes to be
// identified in the type system.
class JSFunctionOrBoundFunction
    : public TorqueGeneratedJSFunctionOrBoundFunction<JSFunctionOrBoundFunction,
                                                      JSObject> {
 public:
  STATIC_ASSERT(kHeaderSize == JSObject::kHeaderSize);
  TQ_OBJECT_CONSTRUCTORS(JSFunctionOrBoundFunction)
};

// JSBoundFunction describes a bound function exotic object.
class JSBoundFunction
    : public TorqueGeneratedJSBoundFunction<JSBoundFunction,
                                            JSFunctionOrBoundFunction> {
 public:
  static MaybeHandle<String> GetName(Isolate* isolate,
                                     Handle<JSBoundFunction> function);
  static Maybe<int> GetLength(Isolate* isolate,
                              Handle<JSBoundFunction> function);
  static MaybeHandle<NativeContext> GetFunctionRealm(
      Handle<JSBoundFunction> function);

  // Dispatched behavior.
  DECL_PRINTER(JSBoundFunction)
  DECL_VERIFIER(JSBoundFunction)

  // The bound function's string representation implemented according
  // to ES6 section 19.2.3.5 Function.prototype.toString ( ).
  static Handle<String> ToString(Handle<JSBoundFunction> function);

  TQ_OBJECT_CONSTRUCTORS(JSBoundFunction)
};

// JSFunction describes JavaScript functions.
class JSFunction : public JSFunctionOrBoundFunction {
 public:
  // [prototype_or_initial_map]:
  DECL_ACCESSORS(prototype_or_initial_map, HeapObject)

  // [shared]: The information about the function that
  // can be shared by instances.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  static const int kLengthDescriptorIndex = 0;
  static const int kNameDescriptorIndex = 1;
  // Home object descriptor index when function has a [[HomeObject]] slot.
  static const int kMaybeHomeObjectDescriptorIndex = 2;

  // [context]: The context for this function.
  inline Context context();
  inline bool has_context() const;
  inline void set_context(HeapObject context);
  inline JSGlobalProxy global_proxy();
  inline NativeContext native_context();
  inline int length();

  static Handle<Object> GetName(Isolate* isolate, Handle<JSFunction> function);
  static Handle<NativeContext> GetFunctionRealm(Handle<JSFunction> function);

  // [code]: The generated code object for this function.  Executed
  // when the function is invoked, e.g. foo() or new foo(). See
  // [[Call]] and [[Construct]] description in ECMA-262, section
  // 8.6.2, page 27.
  inline Code code() const;
  inline void set_code(Code code);
  inline void set_code_no_write_barrier(Code code);

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  inline AbstractCode abstract_code();

  // Tells whether or not this function is interpreted.
  //
  // Note: function->IsInterpreted() does not necessarily return the same value
  // as function->shared()->IsInterpreted() because the closure might have been
  // optimized.
  inline bool IsInterpreted();

  // Tells whether or not this function checks its optimization marker in its
  // feedback vector.
  inline bool ChecksOptimizationMarker();

  // Tells whether or not this function holds optimized code.
  //
  // Note: Returning false does not necessarily mean that this function hasn't
  // been optimized, as it may have optimized code on its feedback vector.
  inline bool IsOptimized();

  // Tells whether or not this function has optimized code available to it,
  // either because it is optimized or because it has optimized code in its
  // feedback vector.
  inline bool HasOptimizedCode();

  // Tells whether or not this function has a (non-zero) optimization marker.
  inline bool HasOptimizationMarker();

  // Mark this function for lazy recompilation. The function will be recompiled
  // the next time it is executed.
  void MarkForOptimization(ConcurrencyMode mode);

  // Tells whether or not the function is already marked for lazy recompilation.
  inline bool IsMarkedForOptimization();
  inline bool IsMarkedForConcurrentOptimization();

  // Tells whether or not the function is on the concurrent recompilation queue.
  inline bool IsInOptimizationQueue();

  // Clears the optimized code slot in the function's feedback vector.
  inline void ClearOptimizedCodeSlot(const char* reason);

  // Sets the optimization marker in the function's feedback vector.
  inline void SetOptimizationMarker(OptimizationMarker marker);

  // Clears the optimization marker in the function's feedback vector.
  inline void ClearOptimizationMarker();

  // If slack tracking is active, it computes instance size of the initial map
  // with minimum permissible object slack.  If it is not active, it simply
  // returns the initial map's instance size.
  int ComputeInstanceSizeWithMinSlack(Isolate* isolate);

  // Completes inobject slack tracking on initial map if it is active.
  inline void CompleteInobjectSlackTrackingIfActive();

  // [raw_feedback_cell]: Gives raw access to the FeedbackCell used to hold the
  /// FeedbackVector eventually. Generally this shouldn't be used to get the
  // feedback_vector, instead use feedback_vector() which correctly deals with
  // the JSFunction's bytecode being flushed.
  DECL_ACCESSORS(raw_feedback_cell, FeedbackCell)

  // Functions related to feedback vector. feedback_vector() can be used once
  // the function has feedback vectors allocated. feedback vectors may not be
  // available after compile when lazily allocating feedback vectors.
  inline FeedbackVector feedback_vector() const;
  inline bool has_feedback_vector() const;
  V8_EXPORT_PRIVATE static void EnsureFeedbackVector(
      Handle<JSFunction> function);

  // Functions related to clousre feedback cell array that holds feedback cells
  // used to create closures from this function. We allocate closure feedback
  // cell arrays after compile, when we want to allocate feedback vectors
  // lazily.
  inline bool has_closure_feedback_cell_array() const;
  inline ClosureFeedbackCellArray closure_feedback_cell_array() const;
  static void EnsureClosureFeedbackCellArray(Handle<JSFunction> function);

  // Initializes the feedback cell of |function|. In lite mode, this would be
  // initialized to the closure feedback cell array that holds the feedback
  // cells for create closure calls from this function. In the regular mode,
  // this allocates feedback vector.
  static void InitializeFeedbackCell(Handle<JSFunction> function);

  // Unconditionally clear the type feedback vector.
  void ClearTypeFeedbackInfo();

  // Resets function to clear compiled data after bytecode has been flushed.
  inline bool NeedsResetDueToFlushedBytecode();
  inline void ResetIfBytecodeFlushed();

  DECL_GETTER(has_prototype_slot, bool)

  // The initial map for an object created by this constructor.
  DECL_GETTER(initial_map, Map)

  static void SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                            Handle<HeapObject> prototype);
  DECL_GETTER(has_initial_map, bool)
  V8_EXPORT_PRIVATE static void EnsureHasInitialMap(
      Handle<JSFunction> function);

  // Creates a map that matches the constructor's initial map, but with
  // [[prototype]] being new.target.prototype. Because new.target can be a
  // JSProxy, this can call back into JavaScript.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Map> GetDerivedMap(
      Isolate* isolate, Handle<JSFunction> constructor,
      Handle<JSReceiver> new_target);

  // Get and set the prototype property on a JSFunction. If the
  // function has an initial map the prototype is set on the initial
  // map. Otherwise, the prototype is put in the initial map field
  // until an initial map is needed.
  DECL_GETTER(has_prototype, bool)
  DECL_GETTER(has_instance_prototype, bool)
  DECL_GETTER(prototype, Object)
  DECL_GETTER(instance_prototype, HeapObject)
  DECL_GETTER(has_prototype_property, bool)
  DECL_GETTER(PrototypeRequiresRuntimeLookup, bool)
  static void SetPrototype(Handle<JSFunction> function, Handle<Object> value);

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled() const;

  static int GetHeaderSize(bool function_has_prototype_slot) {
    return function_has_prototype_slot ? JSFunction::kSizeWithPrototype
                                       : JSFunction::kSizeWithoutPrototype;
  }

  // Prints the name of the function using PrintF.
  void PrintName(FILE* out = stdout);

  DECL_CAST(JSFunction)

  // Calculate the instance size and in-object properties count.
  static V8_WARN_UNUSED_RESULT int CalculateExpectedNofProperties(
      Isolate* isolate, Handle<JSFunction> function);
  static void CalculateInstanceSizeHelper(InstanceType instance_type,
                                          bool has_prototype_slot,
                                          int requested_embedder_fields,
                                          int requested_in_object_properties,
                                          int* instance_size,
                                          int* in_object_properties);

  // Dispatched behavior.
  DECL_PRINTER(JSFunction)
  DECL_VERIFIER(JSFunction)

  // The function's name if it is configured, otherwise shared function info
  // debug name.
  static Handle<String> GetName(Handle<JSFunction> function);

  // ES6 section 9.2.11 SetFunctionName
  // Because of the way this abstract operation is used in the spec,
  // it should never fail, but in practice it will fail if the generated
  // function name's length exceeds String::kMaxLength.
  static V8_WARN_UNUSED_RESULT bool SetName(Handle<JSFunction> function,
                                            Handle<Name> name,
                                            Handle<String> prefix);

  // The function's displayName if it is set, otherwise name if it is
  // configured, otherwise shared function info
  // debug name.
  static Handle<String> GetDebugName(Handle<JSFunction> function);

  // The function's string representation implemented according to
  // ES6 section 19.2.3.5 Function.prototype.toString ( ).
  static Handle<String> ToString(Handle<JSFunction> function);

  DEFINE_FIELD_OFFSET_CONSTANTS(JSFunctionOrBoundFunction::kHeaderSize,
                                TORQUE_GENERATED_JS_FUNCTION_FIELDS)

  static constexpr int kSizeWithoutPrototype = kPrototypeOrInitialMapOffset;
  static constexpr int kSizeWithPrototype = kSize;

  OBJECT_CONSTRUCTORS(JSFunction, JSFunctionOrBoundFunction);
};

// JSGlobalProxy's prototype must be a JSGlobalObject or null,
// and the prototype is hidden. JSGlobalProxy always delegates
// property accesses to its prototype if the prototype is not null.
//
// A JSGlobalProxy can be reinitialized which will preserve its identity.
//
// Accessing a JSGlobalProxy requires security check.

class JSGlobalProxy
    : public TorqueGeneratedJSGlobalProxy<JSGlobalProxy, JSSpecialObject> {
 public:
  inline bool IsDetachedFrom(JSGlobalObject global) const;

  static int SizeWithEmbedderFields(int embedder_field_count);

  // Dispatched behavior.
  DECL_PRINTER(JSGlobalProxy)
  DECL_VERIFIER(JSGlobalProxy)

  TQ_OBJECT_CONSTRUCTORS(JSGlobalProxy)
};

// JavaScript global object.
class JSGlobalObject : public JSSpecialObject {
 public:
  // [native context]: the natives corresponding to this global object.
  DECL_ACCESSORS(native_context, NativeContext)

  // [global proxy]: the global proxy object of the context
  DECL_ACCESSORS(global_proxy, JSGlobalProxy)

  // Gets global object properties.
  DECL_GETTER(global_dictionary, GlobalDictionary)
  inline void set_global_dictionary(GlobalDictionary dictionary);

  static void InvalidatePropertyCell(Handle<JSGlobalObject> object,
                                     Handle<Name> name);
  // Ensure that the global object has a cell for the given property name.
  static Handle<PropertyCell> EnsureEmptyPropertyCell(
      Handle<JSGlobalObject> global, Handle<Name> name,
      PropertyCellType cell_type, int* entry_out = nullptr);

  DECL_CAST(JSGlobalObject)

  inline bool IsDetached();

  // Dispatched behavior.
  DECL_PRINTER(JSGlobalObject)
  DECL_VERIFIER(JSGlobalObject)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSSpecialObject::kHeaderSize,
                                TORQUE_GENERATED_JS_GLOBAL_OBJECT_FIELDS)

  OBJECT_CONSTRUCTORS(JSGlobalObject, JSSpecialObject);
};

// Representation for JS Wrapper objects, String, Number, Boolean, etc.
class JSPrimitiveWrapper
    : public TorqueGeneratedJSPrimitiveWrapper<JSPrimitiveWrapper,
                                               JSCustomElementsObject> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSPrimitiveWrapper)

  TQ_OBJECT_CONSTRUCTORS(JSPrimitiveWrapper)
};

class DateCache;

// Representation for JS date objects.
class JSDate : public TorqueGeneratedJSDate<JSDate, JSObject> {
 public:
  static V8_WARN_UNUSED_RESULT MaybeHandle<JSDate> New(
      Handle<JSFunction> constructor, Handle<JSReceiver> new_target, double tv);

  // Returns the time value (UTC) identifying the current time.
  static double CurrentTimeValue(Isolate* isolate);

  // Returns the date field with the specified index.
  // See FieldIndex for the list of date fields.
  // Arguments and result are raw Address values because this is called
  // via ExternalReference.
  // {raw_date} is a tagged Object pointer.
  // {smi_index} is a tagged Smi.
  // The return value is a tagged Object pointer.
  static Address GetField(Address raw_date, Address smi_index);

  static Handle<Object> SetValue(Handle<JSDate> date, double v);

  void SetValue(Object value, bool is_value_nan);

  // Dispatched behavior.
  DECL_PRINTER(JSDate)
  DECL_VERIFIER(JSDate)

  // The order is important. It must be kept in sync with date macros
  // in macros.py.
  enum FieldIndex {
    kDateValue,
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

 private:
  inline Object DoGetField(FieldIndex index);

  Object GetUTCField(FieldIndex index, double value, DateCache* date_cache);

  // Computes and caches the cacheable fields of the date.
  inline void SetCachedFields(int64_t local_time_ms, DateCache* date_cache);

  TQ_OBJECT_CONSTRUCTORS(JSDate)
};

// Representation of message objects used for error reporting through
// the API. The messages are formatted in JavaScript so this object is
// a real JavaScript object. The information used for formatting the
// error messages are not directly accessible from JavaScript to
// prevent leaking information to user code called during error
// formatting.
class JSMessageObject : public JSObject {
 public:
  // [type]: the type of error message.
  inline MessageTemplate type() const;
  inline void set_type(MessageTemplate value);

  // [arguments]: the arguments for formatting the error message.
  DECL_ACCESSORS(argument, Object)

  // [script]: the script from which the error message originated.
  DECL_ACCESSORS(script, Script)

  // [stack_frames]: an array of stack frames for this error object.
  DECL_ACCESSORS(stack_frames, Object)

  // Initializes the source positions in the object if possible. Does nothing if
  // called more than once. If called when stack space is exhausted, then the
  // source positions will be not be set and calling it again when there is more
  // stack space will not have any effect.
  static void EnsureSourcePositionsAvailable(Isolate* isolate,
                                             Handle<JSMessageObject> message);

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

  // Returns the source code line containing the given source
  // position, or the empty string if the position is invalid.
  // EnsureSourcePositionsAvailable must have been called before calling this.
  Handle<String> GetSourceLine() const;

  DECL_INT_ACCESSORS(error_level)

  DECL_CAST(JSMessageObject)

  // Dispatched behavior.
  DECL_PRINTER(JSMessageObject)
  DECL_VERIFIER(JSMessageObject)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JS_MESSAGE_OBJECT_FIELDS)
  // TODO(v8:8989): [torque] Support marker constants.
  static const int kPointerFieldsEndOffset = kStartPositionOffset;

  using BodyDescriptor = FixedBodyDescriptor<HeapObject::kMapOffset,
                                             kPointerFieldsEndOffset, kSize>;

 private:
  friend class Factory;

  inline bool DidEnsureSourcePositionsAvailable() const;

  // [shared]: optional SharedFunctionInfo that can be used to reconstruct the
  // source position if not available when the message was generated.
  DECL_ACCESSORS(shared_info, HeapObject)

  // [bytecode_offset]: optional offset using along with |shared| to generation
  // source positions.
  DECL_ACCESSORS(bytecode_offset, Smi)

  // [start_position]: the start position in the script for the error message.
  DECL_INT_ACCESSORS(start_position)

  // [end_position]: the end position in the script for the error message.
  DECL_INT_ACCESSORS(end_position)

  DECL_INT_ACCESSORS(raw_type)

  OBJECT_CONSTRUCTORS(JSMessageObject, JSObject);
};

// The [Async-from-Sync Iterator] object
// (proposal-async-iteration/#sec-async-from-sync-iterator-objects)
// An object which wraps an ordinary Iterator and converts it to behave
// according to the Async Iterator protocol.
// (See https://tc39.github.io/proposal-async-iteration/#sec-iteration)
class JSAsyncFromSyncIterator
    : public TorqueGeneratedJSAsyncFromSyncIterator<JSAsyncFromSyncIterator,
                                                    JSObject> {
 public:
  DECL_PRINTER(JSAsyncFromSyncIterator)

  // Async-from-Sync Iterator instances are ordinary objects that inherit
  // properties from the %AsyncFromSyncIteratorPrototype% intrinsic object.
  // Async-from-Sync Iterator instances are initially created with the internal
  // slots listed in Table 4.
  // (proposal-async-iteration/#table-async-from-sync-iterator-internal-slots)

  TQ_OBJECT_CONSTRUCTORS(JSAsyncFromSyncIterator)
};

class JSStringIterator
    : public TorqueGeneratedJSStringIterator<JSStringIterator, JSObject> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSStringIterator)
  DECL_VERIFIER(JSStringIterator)

  // [index]: The [[StringIteratorNextIndex]] inobject property.
  inline int index() const;
  inline void set_index(int value);

  TQ_OBJECT_CONSTRUCTORS(JSStringIterator)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_OBJECTS_H_
