// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_OBJECT_H_
#define INCLUDE_V8_OBJECT_H_

#include "v8-local-handle.h"       // NOLINT(build/include_directory)
#include "v8-maybe.h"              // NOLINT(build/include_directory)
#include "v8-persistent-handle.h"  // NOLINT(build/include_directory)
#include "v8-primitive.h"          // NOLINT(build/include_directory)
#include "v8-traced-handle.h"      // NOLINT(build/include_directory)
#include "v8-value.h"              // NOLINT(build/include_directory)
#include "v8config.h"              // NOLINT(build/include_directory)

namespace v8 {

class Array;
class Function;
class FunctionTemplate;
template <typename T>
class PropertyCallbackInfo;

/**
 * A private symbol
 *
 * This is an experimental feature. Use at your own risk.
 */
class V8_EXPORT Private : public Data {
 public:
  /**
   * Returns the print name string of the private symbol, or undefined if none.
   */
  Local<Value> Name() const;

  /**
   * Create a private symbol. If name is not empty, it will be the description.
   */
  static Local<Private> New(Isolate* isolate,
                            Local<String> name = Local<String>());

  /**
   * Retrieve a global private symbol. If a symbol with this name has not
   * been retrieved in the same isolate before, it is created.
   * Note that private symbols created this way are never collected, so
   * they should only be used for statically fixed properties.
   * Also, there is only one global name space for the names used as keys.
   * To minimize the potential for clashes, use qualified names as keys,
   * e.g., "Class#property".
   */
  static Local<Private> ForApi(Isolate* isolate, Local<String> name);

  V8_INLINE static Private* Cast(Data* data);

 private:
  Private();

  static void CheckCast(Data* that);
};

/**
 * An instance of a Property Descriptor, see Ecma-262 6.2.4.
 *
 * Properties in a descriptor are present or absent. If you do not set
 * `enumerable`, `configurable`, and `writable`, they are absent. If `value`,
 * `get`, or `set` are absent, but you must specify them in the constructor, use
 * empty handles.
 *
 * Accessors `get` and `set` must be callable or undefined if they are present.
 *
 * \note Only query properties if they are present, i.e., call `x()` only if
 * `has_x()` returns true.
 *
 * \code
 * // var desc = {writable: false}
 * v8::PropertyDescriptor d(Local<Value>()), false);
 * d.value(); // error, value not set
 * if (d.has_writable()) {
 *   d.writable(); // false
 * }
 *
 * // var desc = {value: undefined}
 * v8::PropertyDescriptor d(v8::Undefined(isolate));
 *
 * // var desc = {get: undefined}
 * v8::PropertyDescriptor d(v8::Undefined(isolate), Local<Value>()));
 * \endcode
 */
class V8_EXPORT PropertyDescriptor {
 public:
  // GenericDescriptor
  PropertyDescriptor();

  // DataDescriptor
  explicit PropertyDescriptor(Local<Value> value);

  // DataDescriptor with writable property
  PropertyDescriptor(Local<Value> value, bool writable);

  // AccessorDescriptor
  PropertyDescriptor(Local<Value> get, Local<Value> set);

  ~PropertyDescriptor();

  Local<Value> value() const;
  bool has_value() const;

  Local<Value> get() const;
  bool has_get() const;
  Local<Value> set() const;
  bool has_set() const;

  void set_enumerable(bool enumerable);
  bool enumerable() const;
  bool has_enumerable() const;

  void set_configurable(bool configurable);
  bool configurable() const;
  bool has_configurable() const;

  bool writable() const;
  bool has_writable() const;

  struct PrivateData;
  PrivateData* get_private() const { return private_; }

  PropertyDescriptor(const PropertyDescriptor&) = delete;
  void operator=(const PropertyDescriptor&) = delete;

 private:
  PrivateData* private_;
};

/**
 * PropertyAttribute.
 */
enum PropertyAttribute {
  /** None. **/
  None = 0,
  /** ReadOnly, i.e., not writable. **/
  ReadOnly = 1 << 0,
  /** DontEnum, i.e., not enumerable. **/
  DontEnum = 1 << 1,
  /** DontDelete, i.e., not configurable. **/
  DontDelete = 1 << 2
};

/**
 * Accessor[Getter|Setter] are used as callback functions when
 * setting|getting a particular property. See Object and ObjectTemplate's
 * method SetAccessor.
 */
using AccessorGetterCallback =
    void (*)(Local<String> property, const PropertyCallbackInfo<Value>& info);
using AccessorNameGetterCallback =
    void (*)(Local<Name> property, const PropertyCallbackInfo<Value>& info);

using AccessorSetterCallback = void (*)(Local<String> property,
                                        Local<Value> value,
                                        const PropertyCallbackInfo<void>& info);
using AccessorNameSetterCallback =
    void (*)(Local<Name> property, Local<Value> value,
             const PropertyCallbackInfo<void>& info);

/**
 * Access control specifications.
 *
 * Some accessors should be accessible across contexts.  These
 * accessors have an explicit access control parameter which specifies
 * the kind of cross-context access that should be allowed.
 *
 * TODO(dcarney): Remove PROHIBITS_OVERWRITING as it is now unused.
 */
enum AccessControl {
  DEFAULT = 0,
  ALL_CAN_READ = 1,
  ALL_CAN_WRITE = 1 << 1,
  PROHIBITS_OVERWRITING = 1 << 2
};

/**
 * Property filter bits. They can be or'ed to build a composite filter.
 */
enum PropertyFilter {
  ALL_PROPERTIES = 0,
  ONLY_WRITABLE = 1,
  ONLY_ENUMERABLE = 2,
  ONLY_CONFIGURABLE = 4,
  SKIP_STRINGS = 8,
  SKIP_SYMBOLS = 16
};

/**
 * Options for marking whether callbacks may trigger JS-observable side effects.
 * Side-effect-free callbacks are allowlisted during debug evaluation with
 * throwOnSideEffect. It applies when calling a Function, FunctionTemplate,
 * or an Accessor callback. For Interceptors, please see
 * PropertyHandlerFlags's kHasNoSideEffect.
 * Callbacks that only cause side effects to the receiver are allowlisted if
 * invoked on receiver objects that are created within the same debug-evaluate
 * call, as these objects are temporary and the side effect does not escape.
 */
enum class SideEffectType {
  kHasSideEffect,
  kHasNoSideEffect,
  kHasSideEffectToReceiver
};

/**
 * Keys/Properties filter enums:
 *
 * KeyCollectionMode limits the range of collected properties. kOwnOnly limits
 * the collected properties to the given Object only. kIncludesPrototypes will
 * include all keys of the objects's prototype chain as well.
 */
enum class KeyCollectionMode { kOwnOnly, kIncludePrototypes };

/**
 * kIncludesIndices allows for integer indices to be collected, while
 * kSkipIndices will exclude integer indices from being collected.
 */
enum class IndexFilter { kIncludeIndices, kSkipIndices };

/**
 * kConvertToString will convert integer indices to strings.
 * kKeepNumbers will return numbers for integer indices.
 */
enum class KeyConversionMode { kConvertToString, kKeepNumbers, kNoNumbers };

/**
 * Integrity level for objects.
 */
enum class IntegrityLevel { kFrozen, kSealed };

/**
 * A JavaScript object (ECMA-262, 4.3.3)
 */
class V8_EXPORT Object : public Value {
 public:
  /**
   * Set only return Just(true) or Empty(), so if it should never fail, use
   * result.Check().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context,
                                        Local<Value> key, Local<Value> value);

  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context, uint32_t index,
                                        Local<Value> value);

  // Implements CreateDataProperty (ECMA-262, 7.3.4).
  //
  // Defines a configurable, writable, enumerable property with the given value
  // on the object unless the property already exists and is not configurable
  // or the object is not extensible.
  //
  // Returns true on success.
  V8_WARN_UNUSED_RESULT Maybe<bool> CreateDataProperty(Local<Context> context,
                                                       Local<Name> key,
                                                       Local<Value> value);
  V8_WARN_UNUSED_RESULT Maybe<bool> CreateDataProperty(Local<Context> context,
                                                       uint32_t index,
                                                       Local<Value> value);

  // Implements DefineOwnProperty.
  //
  // In general, CreateDataProperty will be faster, however, does not allow
  // for specifying attributes.
  //
  // Returns true on success.
  V8_WARN_UNUSED_RESULT Maybe<bool> DefineOwnProperty(
      Local<Context> context, Local<Name> key, Local<Value> value,
      PropertyAttribute attributes = None);

  // Implements Object.DefineProperty(O, P, Attributes), see Ecma-262 19.1.2.4.
  //
  // The defineProperty function is used to add an own property or
  // update the attributes of an existing own property of an object.
  //
  // Both data and accessor descriptors can be used.
  //
  // In general, CreateDataProperty is faster, however, does not allow
  // for specifying attributes or an accessor descriptor.
  //
  // The PropertyDescriptor can change when redefining a property.
  //
  // Returns true on success.
  V8_WARN_UNUSED_RESULT Maybe<bool> DefineProperty(
      Local<Context> context, Local<Name> key, PropertyDescriptor& descriptor);

  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key);

  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              uint32_t index);

  /**
   * Gets the property attributes of a property which can be None or
   * any combination of ReadOnly, DontEnum and DontDelete. Returns
   * None when the property doesn't exist.
   */
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetPropertyAttributes(
      Local<Context> context, Local<Value> key);

  /**
   * Returns Object.getOwnPropertyDescriptor as per ES2016 section 19.1.2.6.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetOwnPropertyDescriptor(
      Local<Context> context, Local<Name> key);

  /**
   * Object::Has() calls the abstract operation HasProperty(O, P) described
   * in ECMA-262, 7.3.10. Has() returns
   * true, if the object has the property, either own or on the prototype chain.
   * Interceptors, i.e., PropertyQueryCallbacks, are called if present.
   *
   * Has() has the same side effects as JavaScript's `variable in object`.
   * For example, calling Has() on a revoked proxy will throw an exception.
   *
   * \note Has() converts the key to a name, which possibly calls back into
   * JavaScript.
   *
   * See also v8::Object::HasOwnProperty() and
   * v8::Object::HasRealNamedProperty().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context,
                                        Local<Value> key);

  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           Local<Value> key);

  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context, uint32_t index);

  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           uint32_t index);

  /**
   * Note: SideEffectType affects the getter only, not the setter.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetAccessor(
      Local<Context> context, Local<Name> name,
      AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      MaybeLocal<Value> data = MaybeLocal<Value>(),
      AccessControl settings = DEFAULT, PropertyAttribute attribute = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  void SetAccessorProperty(Local<Name> name, Local<Function> getter,
                           Local<Function> setter = Local<Function>(),
                           PropertyAttribute attribute = None,
                           AccessControl settings = DEFAULT);

  /**
   * Sets a native data property like Template::SetNativeDataProperty, but
   * this method sets on this object directly.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetNativeDataProperty(
      Local<Context> context, Local<Name> name,
      AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), PropertyAttribute attributes = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Attempts to create a property with the given name which behaves like a data
   * property, except that the provided getter is invoked (and provided with the
   * data value) to supply its value the first time it is read. After the
   * property is accessed once, it is replaced with an ordinary data property.
   *
   * Analogous to Template::SetLazyDataProperty.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetLazyDataProperty(
      Local<Context> context, Local<Name> name,
      AccessorNameGetterCallback getter, Local<Value> data = Local<Value>(),
      PropertyAttribute attributes = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Functionality for private properties.
   * This is an experimental feature, use at your own risk.
   * Note: Private properties are not inherited. Do not rely on this, since it
   * may change.
   */
  Maybe<bool> HasPrivate(Local<Context> context, Local<Private> key);
  Maybe<bool> SetPrivate(Local<Context> context, Local<Private> key,
                         Local<Value> value);
  Maybe<bool> DeletePrivate(Local<Context> context, Local<Private> key);
  MaybeLocal<Value> GetPrivate(Local<Context> context, Local<Private> key);

  /**
   * Returns an array containing the names of the enumerable properties
   * of this object, including properties from prototype objects.  The
   * array returned by this method contains the same values as would
   * be enumerated by a for-in statement over this object.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetPropertyNames(
      Local<Context> context);
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetPropertyNames(
      Local<Context> context, KeyCollectionMode mode,
      PropertyFilter property_filter, IndexFilter index_filter,
      KeyConversionMode key_conversion = KeyConversionMode::kKeepNumbers);

  /**
   * This function has the same functionality as GetPropertyNames but
   * the returned array doesn't contain the names of properties from
   * prototype objects.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
      Local<Context> context);

  /**
   * Returns an array containing the names of the filtered properties
   * of this object, including properties from prototype objects.  The
   * array returned by this method contains the same values as would
   * be enumerated by a for-in statement over this object.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
      Local<Context> context, PropertyFilter filter,
      KeyConversionMode key_conversion = KeyConversionMode::kKeepNumbers);

  /**
   * Get the prototype object.  This does not skip objects marked to
   * be skipped by __proto__ and it does not consult the security
   * handler.
   */
  Local<Value> GetPrototype();

  /**
   * Set the prototype object.  This does not skip objects marked to
   * be skipped by __proto__ and it does not consult the security
   * handler.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetPrototype(Local<Context> context,
                                                 Local<Value> prototype);

  /**
   * Finds an instance of the given function template in the prototype
   * chain.
   */
  Local<Object> FindInstanceInPrototypeChain(Local<FunctionTemplate> tmpl);

  /**
   * Call builtin Object.prototype.toString on this object.
   * This is different from Value::ToString() that may call
   * user-defined toString function. This one does not.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ObjectProtoToString(
      Local<Context> context);

  /**
   * Returns the name of the function invoked as a constructor for this object.
   */
  Local<String> GetConstructorName();

  /**
   * Sets the integrity level of the object.
   */
  Maybe<bool> SetIntegrityLevel(Local<Context> context, IntegrityLevel level);

  /** Gets the number of internal fields for this Object. */
  int InternalFieldCount() const;

  /** Same as above, but works for PersistentBase. */
  V8_INLINE static int InternalFieldCount(
      const PersistentBase<Object>& object) {
    return object.val_->InternalFieldCount();
  }

  /** Same as above, but works for BasicTracedReference. */
  V8_INLINE static int InternalFieldCount(
      const BasicTracedReference<Object>& object) {
    return object->InternalFieldCount();
  }

  /** Gets the value from an internal field. */
  V8_INLINE Local<Value> GetInternalField(int index);

  /** Sets the value in an internal field. */
  void SetInternalField(int index, Local<Value> value);

  /**
   * Gets a 2-byte-aligned native pointer from an internal field. This field
   * must have been set by SetAlignedPointerInInternalField, everything else
   * leads to undefined behavior.
   */
  V8_INLINE void* GetAlignedPointerFromInternalField(int index);

  /** Same as above, but works for PersistentBase. */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const PersistentBase<Object>& object, int index) {
    return object.val_->GetAlignedPointerFromInternalField(index);
  }

  /** Same as above, but works for TracedReference. */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const BasicTracedReference<Object>& object, int index) {
    return object->GetAlignedPointerFromInternalField(index);
  }

  /**
   * Sets a 2-byte-aligned native pointer in an internal field. To retrieve such
   * a field, GetAlignedPointerFromInternalField must be used, everything else
   * leads to undefined behavior.
   */
  void SetAlignedPointerInInternalField(int index, void* value);
  void SetAlignedPointerInInternalFields(int argc, int indices[],
                                         void* values[]);

  /**
   * HasOwnProperty() is like JavaScript's Object.prototype.hasOwnProperty().
   *
   * See also v8::Object::Has() and v8::Object::HasRealNamedProperty().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   Local<Name> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   uint32_t index);
  /**
   * Use HasRealNamedProperty() if you want to check if an object has an own
   * property without causing side effects, i.e., without calling interceptors.
   *
   * This function is similar to v8::Object::HasOwnProperty(), but it does not
   * call interceptors.
   *
   * \note Consider using non-masking interceptors, i.e., the interceptors are
   * not called if the receiver has the real named property. See
   * `v8::PropertyHandlerFlags::kNonMasking`.
   *
   * See also v8::Object::Has().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedProperty(Local<Context> context,
                                                         Local<Name> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealIndexedProperty(
      Local<Context> context, uint32_t index);
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedCallbackProperty(
      Local<Context> context, Local<Name> key);

  /**
   * If result.IsEmpty() no real property was located in the prototype chain.
   * This means interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedPropertyInPrototypeChain(
      Local<Context> context, Local<Name> key);

  /**
   * Gets the property attributes of a real property in the prototype chain,
   * which can be None or any combination of ReadOnly, DontEnum and DontDelete.
   * Interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute>
  GetRealNamedPropertyAttributesInPrototypeChain(Local<Context> context,
                                                 Local<Name> key);

  /**
   * If result.IsEmpty() no real property was located on the object or
   * in the prototype chain.
   * This means interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedProperty(
      Local<Context> context, Local<Name> key);

  /**
   * Gets the property attributes of a real property which can be
   * None or any combination of ReadOnly, DontEnum and DontDelete.
   * Interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetRealNamedPropertyAttributes(
      Local<Context> context, Local<Name> key);

  /** Tests for a named lookup interceptor.*/
  bool HasNamedLookupInterceptor() const;

  /** Tests for an index lookup interceptor.*/
  bool HasIndexedLookupInterceptor() const;

  /**
   * Returns the identity hash for this object. The current implementation
   * uses a hidden property on the object to store the identity hash.
   *
   * The return value will never be 0. Also, it is not guaranteed to be
   * unique.
   */
  int GetIdentityHash();

  /**
   * Clone this object with a fast but shallow copy.  Values will point
   * to the same values as the original object.
   */
  // TODO(dcarney): take an isolate and optionally bail out?
  Local<Object> Clone();

  /**
   * Returns the context in which the object was created.
   */
  MaybeLocal<Context> GetCreationContext();

  /**
   * Shortcut for GetCreationContext().ToLocalChecked().
   **/
  Local<Context> GetCreationContextChecked();

  /** Same as above, but works for Persistents */
  V8_INLINE static MaybeLocal<Context> GetCreationContext(
      const PersistentBase<Object>& object) {
    return object.val_->GetCreationContext();
  }

  /**
   * Gets the context in which the object was created (see GetCreationContext())
   * and if it's available reads respective embedder field value.
   * If the context can't be obtained nullptr is returned.
   * Basically it's a shortcut for
   *   obj->GetCreationContext().GetAlignedPointerFromEmbedderData(index)
   * which doesn't create a handle for Context object on the way and doesn't
   * try to expand the embedder data attached to the context.
   * In case the Local<Context> is already available because of other reasons,
   * it's fine to keep using Context::GetAlignedPointerFromEmbedderData().
   */
  void* GetAlignedPointerFromEmbedderDataInCreationContext(int index);

  /**
   * Checks whether a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   * When an Object is callable this method returns true.
   */
  bool IsCallable() const;

  /**
   * True if this object is a constructor.
   */
  bool IsConstructor() const;

  /**
   * True if this object can carry information relevant to the embedder in its
   * embedder fields, false otherwise. This is generally true for objects
   * constructed through function templates but also holds for other types where
   * V8 automatically adds internal fields at compile time, such as e.g.
   * v8::ArrayBuffer.
   */
  bool IsApiWrapper() const;

  /**
   * True if this object was created from an object template which was marked
   * as undetectable. See v8::ObjectTemplate::MarkAsUndetectable for more
   * information.
   */
  bool IsUndetectable() const;

  /**
   * Call an Object as a function if a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsFunction(Local<Context> context,
                                                         Local<Value> recv,
                                                         int argc,
                                                         Local<Value> argv[]);

  /**
   * Call an Object as a constructor if a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   * Note: This method behaves like the Function::NewInstance method.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsConstructor(
      Local<Context> context, int argc, Local<Value> argv[]);

  /**
   * Return the isolate to which the Object belongs to.
   */
  Isolate* GetIsolate();

  /**
   * If this object is a Set, Map, WeakSet or WeakMap, this returns a
   * representation of the elements of this object as an array.
   * If this object is a SetIterator or MapIterator, this returns all
   * elements of the underlying collection, starting at the iterator's current
   * position.
   * For other types, this will return an empty MaybeLocal<Array> (without
   * scheduling an exception).
   */
  MaybeLocal<Array> PreviewEntries(bool* is_key_value);

  static Local<Object> New(Isolate* isolate);

  /**
   * Creates a JavaScript object with the given properties, and
   * a the given prototype_or_null (which can be any JavaScript
   * value, and if it's null, the newly created object won't have
   * a prototype at all). This is similar to Object.create().
   * All properties will be created as enumerable, configurable
   * and writable properties.
   */
  static Local<Object> New(Isolate* isolate, Local<Value> prototype_or_null,
                           Local<Name>* names, Local<Value>* values,
                           size_t length);

  V8_INLINE static Object* Cast(Value* obj);

  /**
   * Support for TC39 "dynamic code brand checks" proposal.
   *
   * This API allows to query whether an object was constructed from a
   * "code like" ObjectTemplate.
   *
   * See also: v8::ObjectTemplate::SetCodeLike
   */
  bool IsCodeLike(Isolate* isolate) const;

 private:
  Object();
  static void CheckCast(Value* obj);
  Local<Value> SlowGetInternalField(int index);
  void* SlowGetAlignedPointerFromInternalField(int index);
};

// --- Implementation ---

Local<Value> Object::GetInternalField(int index) {
#ifndef V8_ENABLE_CHECKS
  using A = internal::Address;
  using I = internal::Internals;
  A obj = internal::ValueHelper::ValueAsAddress(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  int instance_type = I::GetInstanceType(obj);
  if (I::CanHaveInternalField(instance_type)) {
    int offset = I::kJSObjectHeaderSize + (I::kEmbedderDataSlotSize * index);
    A value = I::ReadRawField<A>(obj, offset);
#ifdef V8_COMPRESS_POINTERS
    // We read the full pointer value and then decompress it in order to avoid
    // dealing with potential endiannes issues.
    value = I::DecompressTaggedField(obj, static_cast<uint32_t>(value));
#endif

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
    return Local<Value>(reinterpret_cast<Value*>(value));
#else
    internal::Isolate* isolate =
        internal::IsolateFromNeverReadOnlySpaceObject(obj);
    A* result = HandleScope::CreateHandle(isolate, value);
    return Local<Value>(reinterpret_cast<Value*>(result));
#endif
  }
#endif
  return SlowGetInternalField(index);
}

void* Object::GetAlignedPointerFromInternalField(int index) {
#if !defined(V8_ENABLE_CHECKS)
  using A = internal::Address;
  using I = internal::Internals;
  A obj = internal::ValueHelper::ValueAsAddress(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  auto instance_type = I::GetInstanceType(obj);
  if (I::CanHaveInternalField(instance_type)) {
    int offset = I::kJSObjectHeaderSize + (I::kEmbedderDataSlotSize * index) +
                 I::kEmbedderDataSlotExternalPointerOffset;
    Isolate* isolate = I::GetIsolateForSandbox(obj);
    A value =
        I::ReadExternalPointerField<internal::kEmbedderDataSlotPayloadTag>(
            isolate, obj, offset);
    return reinterpret_cast<void*>(value);
  }
#endif
  return SlowGetAlignedPointerFromInternalField(index);
}

Private* Private::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<Private*>(data);
}

Object* Object::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Object*>(value);
}

}  // namespace v8

#endif  // INCLUDE_V8_OBJECT_H_
