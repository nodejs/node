// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_OBJECT_H_
#define INCLUDE_V8_OBJECT_H_

#include "cppgc/garbage-collected.h"
#include "cppgc/name-provider.h"
#include "v8-internal.h"           // NOLINT(build/include_directory)
#include "v8-local-handle.h"       // NOLINT(build/include_directory)
#include "v8-maybe.h"              // NOLINT(build/include_directory)
#include "v8-persistent-handle.h"  // NOLINT(build/include_directory)
#include "v8-primitive.h"          // NOLINT(build/include_directory)
#include "v8-sandbox.h"            // NOLINT(build/include_directory)
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
 * A tag for embedder data. Objects with different C++ types should use
 * different values of EmbedderDataTypeTag when written to embedder data. The
 * allowed range is 0..V8_EMBEDDER_DATA_TAG_COUNT - 1. If this is not
 * sufficient, V8_EMBEDDER_DATA_TAG_COUNT can be increased.
 */
using EmbedderDataTypeTag = uint16_t;

constexpr EmbedderDataTypeTag kEmbedderDataTypeTagDefault = 0;

V8_EXPORT internal::ExternalPointerTag ToExternalPointerTag(
    v8::EmbedderDataTypeTag api_tag);

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
 * Accessor[Getter|Setter] are used as callback functions when setting|getting
 * a particular data property. See Object::SetNativeDataProperty and
 * ObjectTemplate::SetNativeDataProperty methods.
 */
using AccessorNameGetterCallback =
    void (*)(Local<Name> property, const PropertyCallbackInfo<Value>& info);

using AccessorNameSetterCallback =
    void (*)(Local<Name> property, Local<Value> value,
             const PropertyCallbackInfo<void>& info);

/**
 * Access control specifications.
 *
 * Some accessors should be accessible across contexts. These
 * accessors have an explicit access control parameter which specifies
 * the kind of cross-context access that should be allowed.
 *
 */
enum V8_DEPRECATED(
    "This enum is no longer used and will be removed in V8 14.3.")
    AccessControl {
      DEFAULT V8_ENUM_DEPRECATED("not used") = 0,
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
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context,
                                        Local<Value> key, Local<Value> value,
                                        MaybeLocal<Object> receiver);

  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context, uint32_t index,
                                        Local<Value> value);

  /**
   * Implements CreateDataProperty(O, P, V), see
   * https://tc39.es/ecma262/#sec-createdataproperty.
   *
   * Defines a configurable, writable, enumerable property with the given value
   * on the object unless the property already exists and is not configurable
   * or the object is not extensible.
   *
   * Returns true on success.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> CreateDataProperty(Local<Context> context,
                                                       Local<Name> key,
                                                       Local<Value> value);
  V8_WARN_UNUSED_RESULT Maybe<bool> CreateDataProperty(Local<Context> context,
                                                       uint32_t index,
                                                       Local<Value> value);

  /**
   * Implements [[DefineOwnProperty]] for data property case, see
   * https://tc39.es/ecma262/#table-essential-internal-methods.
   *
   * In general, CreateDataProperty will be faster, however, does not allow
   * for specifying attributes.
   *
   * Returns true on success.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> DefineOwnProperty(
      Local<Context> context, Local<Name> key, Local<Value> value,
      PropertyAttribute attributes = None);

  /**
   * Implements Object.defineProperty(O, P, Attributes), see
   * https://tc39.es/ecma262/#sec-object.defineproperty.
   *
   * The defineProperty function is used to add an own property or
   * update the attributes of an existing own property of an object.
   *
   * Both data and accessor descriptors can be used.
   *
   * In general, CreateDataProperty is faster, however, does not allow
   * for specifying attributes or an accessor descriptor.
   *
   * The PropertyDescriptor can change when redefining a property.
   *
   * Returns true on success.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> DefineProperty(
      Local<Context> context, Local<Name> key, PropertyDescriptor& descriptor);

  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key);
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key,
                                              MaybeLocal<Object> receiver);

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
   * Implements Object.getOwnPropertyDescriptor(O, P), see
   * https://tc39.es/ecma262/#sec-object.getownpropertydescriptor.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetOwnPropertyDescriptor(
      Local<Context> context, Local<Name> key);

  /**
   * Object::Has() calls the abstract operation HasProperty(O, P), see
   * https://tc39.es/ecma262/#sec-hasproperty. Has() returns
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
   * Sets an accessor property like Template::SetAccessorProperty, but
   * this method sets on this object directly.
   */
  void SetAccessorProperty(Local<Name> name, Local<Function> getter,
                           Local<Function> setter = Local<Function>(),
                           PropertyAttribute attributes = None);

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
   * Get the prototype object (same as calling Object.getPrototypeOf(..)).
   * This does not consult the security handler.
   * TODO(http://crbug.com/333672197): rename back to GetPrototype().
   */
  Local<Value> GetPrototypeV2();

  /**
   * Set the prototype object (same as calling Object.setPrototypeOf(..)).
   * This does not consult the security handler.
   * TODO(http://crbug.com/333672197): rename back to SetPrototype().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetPrototypeV2(Local<Context> context,
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
    return object.template value<Object>()->InternalFieldCount();
  }

  /** Same as above, but works for BasicTracedReference. */
  V8_INLINE static int InternalFieldCount(
      const BasicTracedReference<Object>& object) {
    return object.template value<Object>()->InternalFieldCount();
  }

  /**
   * Gets the data from an internal field.
   * To cast the return value into v8::Value subtypes, it needs to be
   * casted to a v8::Value first. For example, to cast it into v8::External:
   *
   * object->GetInternalField(index).As<v8::Value>().As<v8::External>();
   *
   * The embedder should make sure that the internal field being retrieved
   * using this method has already been set with SetInternalField() before.
   **/
  V8_INLINE Local<Data> GetInternalField(int index);

  /** Sets the data in an internal field. */
  void SetInternalField(int index, Local<Data> data);

  /**
   * Gets a 2-byte-aligned native pointer from an internal field. This field
   * must have been set by SetAlignedPointerInInternalField, everything else
   * leads to undefined behavior.
   */
  V8_INLINE void* GetAlignedPointerFromInternalField(int index,
                                                     EmbedderDataTypeTag tag);
  V8_INLINE void* GetAlignedPointerFromInternalField(v8::Isolate* isolate,
                                                     int index,
                                                     EmbedderDataTypeTag tag);

  V8_DEPRECATE_SOON(
      "Use GetAlignedPointerFromInternalField with EmbedderDataTypeTag "
      "parameter instead.")
  V8_INLINE void* GetAlignedPointerFromInternalField(int index) {
    return GetAlignedPointerFromInternalField(index,
                                              kEmbedderDataTypeTagDefault);
  }

  V8_DEPRECATE_SOON(
      "Use GetAlignedPointerFromInternalField with EmbedderDataTypeTag "
      "parameter instead.")
  V8_INLINE void* GetAlignedPointerFromInternalField(v8::Isolate* isolate,
                                                     int index) {
    return GetAlignedPointerFromInternalField(isolate, index,
                                              kEmbedderDataTypeTagDefault);
  }

  /** Same as above, but works for PersistentBase. */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const PersistentBase<Object>& object, int index,
      EmbedderDataTypeTag tag) {
    return object.template value<Object>()->GetAlignedPointerFromInternalField(
        index, tag);
  }

  V8_DEPRECATE_SOON(
      "Use GetAlignedPointerFromInternalField with EmbedderDataTypeTag "
      "parameter instead.")
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const PersistentBase<Object>& object, int index) {
    return object.template value<Object>()->GetAlignedPointerFromInternalField(
        index);
  }

  /** Same as above, but works for TracedReference. */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const BasicTracedReference<Object>& object, int index,
      EmbedderDataTypeTag tag) {
    return object.template value<Object>()->GetAlignedPointerFromInternalField(
        index, tag);
  }

  V8_DEPRECATE_SOON(
      "Use GetAlignedPointerFromInternalField with EmbedderDataTypeTag "
      "parameter instead.")
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const BasicTracedReference<Object>& object, int index) {
    return object.template value<Object>()->GetAlignedPointerFromInternalField(
        index);
  }

  /**
   * Sets a 2-byte-aligned native pointer in an internal field. To retrieve such
   * a field, GetAlignedPointerFromInternalField must be used, everything else
   * leads to undefined behavior.
   */
  void SetAlignedPointerInInternalField(int index, void* value,
                                        EmbedderDataTypeTag tag);

  V8_DEPRECATE_SOON(
      "Use SetAlignedPointerInInternalField with EmbedderDataTypeTag parameter "
      "instead.")
  void SetAlignedPointerInInternalField(int index, void* value) {
    SetAlignedPointerInInternalField(index, value, kEmbedderDataTypeTagDefault);
  }

  V8_DEPRECATE_SOON(
      "Use SetAlignedPointerInInternalField with EmbedderDataTypeTag "
      "parameter instead.")
  void SetAlignedPointerInInternalFields(int argc, int indices[],
                                         void* values[]);

  // Type information for a Wrappable object that got wrapped with
  // `v8::Object::Wrap()`.
  struct WrapperTypeInfo {
    const int16_t type_id;
  };

  // v8::Object::Wrappable serves as the base class for all C++ objects that can
  // be wrapped by a JavaScript object using `v8::Object::Wrap()`.
  //
  // Note that v8::Object::Wrappable` inherits from `NameProvider` and provides
  // `GetWrapperTypeInfo` to allow subclasses to have smaller object sizes.
  class Wrappable : public cppgc::GarbageCollected<Wrappable>,
                    public cppgc::NameProvider {
   public:
    virtual const WrapperTypeInfo* GetWrapperTypeInfo() const {
      return nullptr;
    }

    const char* GetHumanReadableName() const override { return "internal"; }

    virtual void Trace(cppgc::Visitor* visitor) const {}
  };

  /**
   * Unwraps a JS wrapper object.
   *
   * \param tag The tag for retrieving the wrappable instance. Must match the
   * tag that has been used for a previous `Wrap()` operation.
   * \param isolate The Isolate for the `wrapper` object.
   * \param wrapper The JS wrapper object that should be unwrapped.
   * \returns the C++ wrappable instance, or nullptr if the JS object has never
   * been wrapped.
   */
  template <CppHeapPointerTag tag, typename T = void>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate,
                             const v8::Local<v8::Object>& wrapper);
  template <CppHeapPointerTag tag, typename T = void>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate,
                             const PersistentBase<Object>& wrapper);
  template <CppHeapPointerTag tag, typename T = void>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate,
                             const BasicTracedReference<Object>& wrapper);

  template <typename T = void>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate,
                             const v8::Local<v8::Object>& wrapper,
                             CppHeapPointerTagRange tag_range);
  template <typename T = void>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate,
                             const PersistentBase<Object>& wrapper,
                             CppHeapPointerTagRange tag_range);
  template <typename T = void>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate,
                             const BasicTracedReference<Object>& wrapper,
                             CppHeapPointerTagRange tag_range);

  /**
   * Wraps a JS wrapper with a C++ instance.
   *
   * \param tag The pointer tag that should be used for storing this object.
   * Future `Unwrap()` operations must provide a matching tag.
   * \param isolate The Isolate for the `wrapper` object.
   * \param wrapper The JS wrapper object.
   * \param wrappable The C++ object instance that is wrapped by the JS object.
   */
  template <CppHeapPointerTag tag>
  static V8_INLINE void Wrap(v8::Isolate* isolate,
                             const v8::Local<v8::Object>& wrapper,
                             Wrappable* wrappable);
  template <CppHeapPointerTag tag>
  static V8_INLINE void Wrap(v8::Isolate* isolate,
                             const PersistentBase<Object>& wrapper,
                             Wrappable* wrappable);
  template <CppHeapPointerTag tag>
  static V8_INLINE void Wrap(v8::Isolate* isolate,
                             const BasicTracedReference<Object>& wrapper,
                             Wrappable* wrappable);
  static V8_INLINE void Wrap(v8::Isolate* isolate,
                             const v8::Local<v8::Object>& wrapper,
                             Wrappable* wrappable, CppHeapPointerTag tag);
  static V8_INLINE void Wrap(v8::Isolate* isolate,
                             const PersistentBase<Object>& wrapper,
                             Wrappable* wrappable, CppHeapPointerTag tag);
  static V8_INLINE void Wrap(v8::Isolate* isolate,
                             const BasicTracedReference<Object>& wrapper,
                             Wrappable* wrappable, CppHeapPointerTag tag);

  // Version of Wrap() function for v8::Context::Global() objects.
  // Unlike the functions above it wraps both JSGlobalProxy and its hidden
  // prototype (JSGlobalObject or remote object).
  static void WrapGlobal(v8::Isolate* isolate,
                         const v8::Local<v8::Object>& wrapper,
                         Wrappable* wrappable, CppHeapPointerTag tag);

  // Checks that wrappables set on JSGlobalProxy and its hidden prototype are
  // the same.
  static bool CheckGlobalWrappable(v8::Isolate* isolate,
                                   const v8::Local<v8::Object>& wrapper,
                                   CppHeapPointerTagRange tag_range);

  /**
   * HasOwnProperty() is like JavaScript's
   * Object.prototype.hasOwnProperty().
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
   * Clone this object with a fast but shallow copy. Values will point to the
   * same values as the original object.
   *
   * Prefer using version with Isolate parameter.
   */
  Local<Object> Clone(v8::Isolate* isolate);
  Local<Object> Clone();

  /**
   * Returns the context in which the object was created.
   *
   * Prefer using version with Isolate parameter.
   */
  MaybeLocal<Context> GetCreationContext(v8::Isolate* isolate);
  V8_DEPRECATE_SOON("Use the version with the isolate argument.")
  MaybeLocal<Context> GetCreationContext();

  /**
   * Shortcut for GetCreationContext(...).ToLocalChecked().
   *
   * Prefer using version with Isolate parameter.
   **/
  Local<Context> GetCreationContextChecked(v8::Isolate* isolate);
  V8_DEPRECATE_SOON("Use the version with the isolate argument.")
  Local<Context> GetCreationContextChecked();

  /** Same as above, but works for Persistents */
  V8_INLINE static MaybeLocal<Context> GetCreationContext(
      v8::Isolate* isolate, const PersistentBase<Object>& object) {
    return object.template value<Object>()->GetCreationContext(isolate);
  }
  V8_DEPRECATE_SOON("Use the version with the isolate argument.")
  V8_INLINE static MaybeLocal<Context> GetCreationContext(
      const PersistentBase<Object>& object);

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
   *
   * Prefer using version with Isolate parameter if you have an Isolate,
   * otherwise use the other one.
   */
  void* GetAlignedPointerFromEmbedderDataInCreationContext(
      v8::Isolate* isolate, int index, EmbedderDataTypeTag tag);
  void* GetAlignedPointerFromEmbedderDataInCreationContext(
      int index, EmbedderDataTypeTag tag);

  V8_DEPRECATE_SOON(
      "Use GetAlignedPointerFromEmbedderDataInCreationContext with "
      "EmbedderDataTypeTag parameter instead.")
  void* GetAlignedPointerFromEmbedderDataInCreationContext(v8::Isolate* isolate,
                                                           int index) {
    return GetAlignedPointerFromEmbedderDataInCreationContext(
        isolate, index, kEmbedderDataTypeTagDefault);
  }

  V8_DEPRECATE_SOON(
      "Use GetAlignedPointerFromEmbedderDataInCreationContext with "
      "EmbedderDataTypeTag parameter instead.")
  void* GetAlignedPointerFromEmbedderDataInCreationContext(int index) {
    return GetAlignedPointerFromEmbedderDataInCreationContext(
        index, kEmbedderDataTypeTagDefault);
  }

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
   * Returns true if this object can be generally used to wrap object objects.
   * This means that the object either follows the convention of using embedder
   * fields to denote type/instance pointers or is using the Wrap()/Unwrap()
   * APIs for the same purpose. Returns false otherwise.
   *
   * Note that there may be other objects that use embedder fields but are not
   * used as API wrapper objects. E.g., v8::Promise may in certain configuration
   * use embedder fields but promises are not generally supported as API
   * wrappers. The method will return false in those cases.
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
  static void* Unwrap(v8::Isolate* isolate, internal::Address wrapper_obj,
                      CppHeapPointerTagRange tag_range);
  static void Wrap(v8::Isolate* isolate, internal::Address wrapper_obj,
                   CppHeapPointerTag tag, void* wrappable);

  Object();
  static void CheckCast(Value* obj);
  Local<Data> SlowGetInternalField(int index);
  void* SlowGetAlignedPointerFromInternalField(int index,
                                               EmbedderDataTypeTag tag);
  void* SlowGetAlignedPointerFromInternalField(v8::Isolate* isolate, int index,
                                               EmbedderDataTypeTag tag);
};

// --- Implementation ---

Local<Data> Object::GetInternalField(int index) {
#ifndef V8_ENABLE_CHECKS
  using A = internal::Address;
  using I = internal::Internals;
  A obj = internal::ValueHelper::ValueAsAddress(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  int instance_type = I::GetInstanceType(obj);
  if (I::CanHaveInternalField(instance_type)) {
    int offset = I::kJSAPIObjectWithEmbedderSlotsHeaderSize +
                 (I::kEmbedderDataSlotSize * index);
    A value = I::ReadRawField<A>(obj, offset);
#ifdef V8_COMPRESS_POINTERS
    // We read the full pointer value and then decompress it in order to avoid
    // dealing with potential endianness issues.
    value = I::DecompressTaggedField(obj, static_cast<uint32_t>(value));
#endif

    auto* isolate = I::GetCurrentIsolate();
    return Local<Data>::New(isolate, value);
  }
#endif
  return SlowGetInternalField(index);
}

void* Object::GetAlignedPointerFromInternalField(v8::Isolate* isolate,
                                                 int index,
                                                 EmbedderDataTypeTag tag) {
#if !defined(V8_ENABLE_CHECKS)
  using A = internal::Address;
  using I = internal::Internals;
  A obj = internal::ValueHelper::ValueAsAddress(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  auto instance_type = I::GetInstanceType(obj);
  if (V8_LIKELY(I::CanHaveInternalField(instance_type))) {
    int offset = I::kJSAPIObjectWithEmbedderSlotsHeaderSize +
                 (I::kEmbedderDataSlotSize * index) +
                 I::kEmbedderDataSlotExternalPointerOffset;
    A value = I::ReadExternalPointerField(isolate, obj, offset,
                                          ToExternalPointerTag(tag));
    return reinterpret_cast<void*>(value);
  }
#endif
  return SlowGetAlignedPointerFromInternalField(isolate, index, tag);
}

void* Object::GetAlignedPointerFromInternalField(int index,
                                                 EmbedderDataTypeTag tag) {
#if !defined(V8_ENABLE_CHECKS)
  using A = internal::Address;
  using I = internal::Internals;
  A obj = internal::ValueHelper::ValueAsAddress(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  auto instance_type = I::GetInstanceType(obj);
  if (V8_LIKELY(I::CanHaveInternalField(instance_type))) {
    int offset = I::kJSAPIObjectWithEmbedderSlotsHeaderSize +
                 (I::kEmbedderDataSlotSize * index) +
                 I::kEmbedderDataSlotExternalPointerOffset;
    Isolate* isolate = I::GetCurrentIsolateForSandbox();
    A value = I::ReadExternalPointerField(isolate, obj, offset,
                                          ToExternalPointerTag(tag));
    return reinterpret_cast<void*>(value);
  }
#endif
  return SlowGetAlignedPointerFromInternalField(index, tag);
}

// static
template <CppHeapPointerTag tag, typename T>
T* Object::Unwrap(v8::Isolate* isolate, const v8::Local<v8::Object>& wrapper) {
  CppHeapPointerTagRange tag_range(tag, tag);
  auto obj = internal::ValueHelper::ValueAsAddress(*wrapper);
#if !defined(V8_ENABLE_CHECKS)
  return internal::ReadCppHeapPointerField<T>(
      isolate, obj, internal::Internals::kJSObjectHeaderSize, tag_range);
#else   // defined(V8_ENABLE_CHECKS)
  return reinterpret_cast<T*>(Unwrap(isolate, obj, tag_range));
#endif  // defined(V8_ENABLE_CHECKS)
}

// static
template <CppHeapPointerTag tag, typename T>
T* Object::Unwrap(v8::Isolate* isolate, const PersistentBase<Object>& wrapper) {
  CppHeapPointerTagRange tag_range(tag, tag);
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
#if !defined(V8_ENABLE_CHECKS)
  return internal::ReadCppHeapPointerField<T>(
      isolate, obj, internal::Internals::kJSObjectHeaderSize, tag_range);
#else   // defined(V8_ENABLE_CHECKS)
  return reinterpret_cast<T*>(Unwrap(isolate, obj, tag_range));
#endif  // defined(V8_ENABLE_CHECKS)
}

// static
template <CppHeapPointerTag tag, typename T>
T* Object::Unwrap(v8::Isolate* isolate,
                  const BasicTracedReference<Object>& wrapper) {
  CppHeapPointerTagRange tag_range(tag, tag);
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
#if !defined(V8_ENABLE_CHECKS)
  return internal::ReadCppHeapPointerField<T>(
      isolate, obj, internal::Internals::kJSObjectHeaderSize, tag_range);
#else   // defined(V8_ENABLE_CHECKS)
  return reinterpret_cast<T*>(Unwrap(isolate, obj, tag_range));
#endif  // defined(V8_ENABLE_CHECKS)
}

// static
template <typename T>
T* Object::Unwrap(v8::Isolate* isolate, const v8::Local<v8::Object>& wrapper,
                  CppHeapPointerTagRange tag_range) {
  auto obj = internal::ValueHelper::ValueAsAddress(*wrapper);
#if !defined(V8_ENABLE_CHECKS)
  return internal::ReadCppHeapPointerField<T>(
      isolate, obj, internal::Internals::kJSObjectHeaderSize, tag_range);
#else   // defined(V8_ENABLE_CHECKS)
  return reinterpret_cast<T*>(Unwrap(isolate, obj, tag_range));
#endif  // defined(V8_ENABLE_CHECKS)
}

// static
template <typename T>
T* Object::Unwrap(v8::Isolate* isolate, const PersistentBase<Object>& wrapper,
                  CppHeapPointerTagRange tag_range) {
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
#if !defined(V8_ENABLE_CHECKS)
  return internal::ReadCppHeapPointerField<T>(
      isolate, obj, internal::Internals::kJSObjectHeaderSize, tag_range);
#else   // defined(V8_ENABLE_CHECKS)

  return reinterpret_cast<T*>(Unwrap(isolate, obj, tag_range));
#endif  // defined(V8_ENABLE_CHECKS)
}

// static
template <typename T>
T* Object::Unwrap(v8::Isolate* isolate,
                  const BasicTracedReference<Object>& wrapper,
                  CppHeapPointerTagRange tag_range) {
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
#if !defined(V8_ENABLE_CHECKS)
  return internal::ReadCppHeapPointerField<T>(
      isolate, obj, internal::Internals::kJSObjectHeaderSize, tag_range);
#else   // defined(V8_ENABLE_CHECKS)
  return reinterpret_cast<T*>(Unwrap(isolate, obj, tag_range));
#endif  // defined(V8_ENABLE_CHECKS)
}

// static
template <CppHeapPointerTag tag>
void Object::Wrap(v8::Isolate* isolate, const v8::Local<v8::Object>& wrapper,
                  v8::Object::Wrappable* wrappable) {
  auto obj = internal::ValueHelper::ValueAsAddress(*wrapper);
  Wrap(isolate, obj, tag, wrappable);
}

// static
template <CppHeapPointerTag tag>
void Object::Wrap(v8::Isolate* isolate, const PersistentBase<Object>& wrapper,
                  v8::Object::Wrappable* wrappable) {
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
  Wrap(isolate, obj, tag, wrappable);
}

// static
template <CppHeapPointerTag tag>
void Object::Wrap(v8::Isolate* isolate,
                  const BasicTracedReference<Object>& wrapper,
                  v8::Object::Wrappable* wrappable) {
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
  Wrap(isolate, obj, tag, wrappable);
}

// static
void Object::Wrap(v8::Isolate* isolate, const v8::Local<v8::Object>& wrapper,
                  v8::Object::Wrappable* wrappable, CppHeapPointerTag tag) {
  auto obj = internal::ValueHelper::ValueAsAddress(*wrapper);
  Wrap(isolate, obj, tag, wrappable);
}

// static
void Object::Wrap(v8::Isolate* isolate, const PersistentBase<Object>& wrapper,
                  v8::Object::Wrappable* wrappable, CppHeapPointerTag tag) {
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
  Wrap(isolate, obj, tag, wrappable);
}

// static
void Object::Wrap(v8::Isolate* isolate,
                  const BasicTracedReference<Object>& wrapper,
                  v8::Object::Wrappable* wrappable, CppHeapPointerTag tag) {
  auto obj =
      internal::ValueHelper::ValueAsAddress(wrapper.template value<Object>());
  Wrap(isolate, obj, tag, wrappable);
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
