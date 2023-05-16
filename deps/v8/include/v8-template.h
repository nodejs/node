// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_TEMPLATE_H_
#define INCLUDE_V8_TEMPLATE_H_

#include "v8-data.h"               // NOLINT(build/include_directory)
#include "v8-function-callback.h"  // NOLINT(build/include_directory)
#include "v8-local-handle.h"       // NOLINT(build/include_directory)
#include "v8-memory-span.h"        // NOLINT(build/include_directory)
#include "v8-object.h"             // NOLINT(build/include_directory)
#include "v8config.h"              // NOLINT(build/include_directory)

namespace v8 {

class CFunction;
class FunctionTemplate;
class ObjectTemplate;
class Signature;

// --- Templates ---

#define V8_INTRINSICS_LIST(F)                                 \
  F(ArrayProto_entries, array_entries_iterator)               \
  F(ArrayProto_forEach, array_for_each_iterator)              \
  F(ArrayProto_keys, array_keys_iterator)                     \
  F(ArrayProto_values, array_values_iterator)                 \
  F(ArrayPrototype, initial_array_prototype)                  \
  F(AsyncIteratorPrototype, initial_async_iterator_prototype) \
  F(ErrorPrototype, initial_error_prototype)                  \
  F(IteratorPrototype, initial_iterator_prototype)            \
  F(MapIteratorPrototype, initial_map_iterator_prototype)     \
  F(ObjProto_valueOf, object_value_of_function)               \
  F(SetIteratorPrototype, initial_set_iterator_prototype)

enum Intrinsic {
#define V8_DECL_INTRINSIC(name, iname) k##name,
  V8_INTRINSICS_LIST(V8_DECL_INTRINSIC)
#undef V8_DECL_INTRINSIC
};

/**
 * The superclass of object and function templates.
 */
class V8_EXPORT Template : public Data {
 public:
  /**
   * Adds a property to each instance created by this template.
   *
   * The property must be defined either as a primitive value, or a template.
   */
  void Set(Local<Name> name, Local<Data> value,
           PropertyAttribute attributes = None);
  void SetPrivate(Local<Private> name, Local<Data> value,
                  PropertyAttribute attributes = None);
  V8_INLINE void Set(Isolate* isolate, const char* name, Local<Data> value,
                     PropertyAttribute attributes = None);

  void SetAccessorProperty(
      Local<Name> name,
      Local<FunctionTemplate> getter = Local<FunctionTemplate>(),
      Local<FunctionTemplate> setter = Local<FunctionTemplate>(),
      PropertyAttribute attribute = None, AccessControl settings = DEFAULT);

  /**
   * Whenever the property with the given name is accessed on objects
   * created from this Template the getter and setter callbacks
   * are called instead of getting and setting the property directly
   * on the JavaScript object.
   *
   * \param name The name of the property for which an accessor is added.
   * \param getter The callback to invoke when getting the property.
   * \param setter The callback to invoke when setting the property.
   * \param data A piece of data that will be passed to the getter and setter
   *   callbacks whenever they are invoked.
   * \param settings Access control settings for the accessor. This is a bit
   *   field consisting of one of more of
   *   DEFAULT = 0, ALL_CAN_READ = 1, or ALL_CAN_WRITE = 2.
   *   The default is to not allow cross-context access.
   *   ALL_CAN_READ means that all cross-context reads are allowed.
   *   ALL_CAN_WRITE means that all cross-context writes are allowed.
   *   The combination ALL_CAN_READ | ALL_CAN_WRITE can be used to allow all
   *   cross-context access.
   * \param attribute The attributes of the property for which an accessor
   *   is added.
   */
  void SetNativeDataProperty(
      Local<String> name, AccessorGetterCallback getter,
      AccessorSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      AccessControl settings = DEFAULT,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);
  void SetNativeDataProperty(
      Local<Name> name, AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      AccessControl settings = DEFAULT,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Like SetNativeDataProperty, but V8 will replace the native data property
   * with a real data property on first access.
   */
  void SetLazyDataProperty(
      Local<Name> name, AccessorNameGetterCallback getter,
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * During template instantiation, sets the value with the intrinsic property
   * from the correct context.
   */
  void SetIntrinsicDataProperty(Local<Name> name, Intrinsic intrinsic,
                                PropertyAttribute attribute = None);

 private:
  Template();

  friend class ObjectTemplate;
  friend class FunctionTemplate;
};

// TODO(dcarney): Replace GenericNamedPropertyFooCallback with just
// NamedPropertyFooCallback.

/**
 * Interceptor for get requests on an object.
 *
 * Use `info.GetReturnValue().Set()` to set the return value of the
 * intercepted get request. If the property does not exist the callback should
 * not set the result and must not produce side effects.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict`' mode.
 * See `PropertyCallbackInfo`.
 *
 * \code
 *  void GetterCallback(
 *    Local<Name> name,
 *    const v8::PropertyCallbackInfo<v8::Value>& info) {
 *      info.GetReturnValue().Set(v8_num(42));
 *  }
 *
 *  v8::Local<v8::FunctionTemplate> templ =
 *      v8::FunctionTemplate::New(isolate);
 *  templ->InstanceTemplate()->SetHandler(
 *      v8::NamedPropertyHandlerConfiguration(GetterCallback));
 *  LocalContext env;
 *  env->Global()
 *      ->Set(env.local(), v8_str("obj"), templ->GetFunction(env.local())
 *                                             .ToLocalChecked()
 *                                             ->NewInstance(env.local())
 *                                             .ToLocalChecked())
 *      .FromJust();
 *  v8::Local<v8::Value> result = CompileRun("obj.a = 17; obj.a");
 *  CHECK(v8_num(42)->Equals(env.local(), result).FromJust());
 * \endcode
 *
 * See also `ObjectTemplate::SetHandler`.
 */
using GenericNamedPropertyGetterCallback =
    void (*)(Local<Name> property, const PropertyCallbackInfo<Value>& info);

/**
 * Interceptor for set requests on an object.
 *
 * Use `info.GetReturnValue()` to indicate whether the request was intercepted
 * or not. If the setter successfully intercepts the request, i.e., if the
 * request should not be further executed, call
 * `info.GetReturnValue().Set(value)`. If the setter did not intercept the
 * request, i.e., if the request should be handled as if no interceptor is
 * present, do not not call `Set()` and do not produce side effects.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param value The value which the property will have if the request
 * is not intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * See also
 * `ObjectTemplate::SetHandler.`
 */
using GenericNamedPropertySetterCallback =
    void (*)(Local<Name> property, Local<Value> value,
             const PropertyCallbackInfo<Value>& info);

/**
 * Intercepts all requests that query the attributes of the
 * property, e.g., getOwnPropertyDescriptor(), propertyIsEnumerable(), and
 * defineProperty().
 *
 * Use `info.GetReturnValue().Set(value)` to set the property attributes. The
 * value is an integer encoding a `v8::PropertyAttribute`. If the property does
 * not exist the callback should not set the result and must not produce side
 * effects.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * \note Some functions query the property attributes internally, even though
 * they do not return the attributes. For example, `hasOwnProperty()` can
 * trigger this interceptor depending on the state of the object.
 *
 * See also
 * `ObjectTemplate::SetHandler.`
 */
using GenericNamedPropertyQueryCallback =
    void (*)(Local<Name> property, const PropertyCallbackInfo<Integer>& info);

/**
 * Interceptor for delete requests on an object.
 *
 * Use `info.GetReturnValue()` to indicate whether the request was intercepted
 * or not. If the deleter successfully intercepts the request, i.e., if the
 * request should not be further executed, call
 * `info.GetReturnValue().Set(value)` with a boolean `value`. The `value` is
 * used as the return value of `delete`. If the deleter does not intercept the
 * request then it should not set the result and must not produce side effects.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * \note If you need to mimic the behavior of `delete`, i.e., throw in strict
 * mode instead of returning false, use `info.ShouldThrowOnError()` to determine
 * if you are in strict mode.
 *
 * See also `ObjectTemplate::SetHandler.`
 */
using GenericNamedPropertyDeleterCallback =
    void (*)(Local<Name> property, const PropertyCallbackInfo<Boolean>& info);

/**
 * Returns an array containing the names of the properties the named
 * property getter intercepts.
 *
 * Note: The values in the array must be of type v8::Name.
 */
using GenericNamedPropertyEnumeratorCallback =
    void (*)(const PropertyCallbackInfo<Array>& info);

/**
 * Interceptor for defineProperty requests on an object.
 *
 * Use `info.GetReturnValue()` to indicate whether the request was intercepted
 * or not. If the definer successfully intercepts the request, i.e., if the
 * request should not be further executed, call
 * `info.GetReturnValue().Set(value)`. If the definer did not intercept the
 * request, i.e., if the request should be handled as if no interceptor is
 * present, do not not call `Set()` and do not produce side effects.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param desc The property descriptor which is used to define the
 * property if the request is not intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * See also `ObjectTemplate::SetHandler`.
 */
using GenericNamedPropertyDefinerCallback =
    void (*)(Local<Name> property, const PropertyDescriptor& desc,
             const PropertyCallbackInfo<Value>& info);

/**
 * Interceptor for getOwnPropertyDescriptor requests on an object.
 *
 * Use `info.GetReturnValue().Set()` to set the return value of the
 * intercepted request. The return value must be an object that
 * can be converted to a PropertyDescriptor, e.g., a `v8::value` returned from
 * `v8::Object::getOwnPropertyDescriptor`.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * \note If GetOwnPropertyDescriptor is intercepted, it will
 * always return true, i.e., indicate that the property was found.
 *
 * See also `ObjectTemplate::SetHandler`.
 */
using GenericNamedPropertyDescriptorCallback =
    void (*)(Local<Name> property, const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertyGetterCallback`.
 */
using IndexedPropertyGetterCallback =
    void (*)(uint32_t index, const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertySetterCallback`.
 */
using IndexedPropertySetterCallback =
    void (*)(uint32_t index, Local<Value> value,
             const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertyQueryCallback`.
 */
using IndexedPropertyQueryCallback =
    void (*)(uint32_t index, const PropertyCallbackInfo<Integer>& info);

/**
 * See `v8::GenericNamedPropertyDeleterCallback`.
 */
using IndexedPropertyDeleterCallback =
    void (*)(uint32_t index, const PropertyCallbackInfo<Boolean>& info);

/**
 * Returns an array containing the indices of the properties the indexed
 * property getter intercepts.
 *
 * Note: The values in the array must be uint32_t.
 */
using IndexedPropertyEnumeratorCallback =
    void (*)(const PropertyCallbackInfo<Array>& info);

/**
 * See `v8::GenericNamedPropertyDefinerCallback`.
 */
using IndexedPropertyDefinerCallback =
    void (*)(uint32_t index, const PropertyDescriptor& desc,
             const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertyDescriptorCallback`.
 */
using IndexedPropertyDescriptorCallback =
    void (*)(uint32_t index, const PropertyCallbackInfo<Value>& info);

/**
 * Returns true if the given context should be allowed to access the given
 * object.
 */
using AccessCheckCallback = bool (*)(Local<Context> accessing_context,
                                     Local<Object> accessed_object,
                                     Local<Value> data);

enum class ConstructorBehavior { kThrow, kAllow };

/**
 * A FunctionTemplate is used to create functions at runtime. There
 * can only be one function created from a FunctionTemplate in a
 * context.  The lifetime of the created function is equal to the
 * lifetime of the context.  So in case the embedder needs to create
 * temporary functions that can be collected using Scripts is
 * preferred.
 *
 * Any modification of a FunctionTemplate after first instantiation will trigger
 * a crash.
 *
 * A FunctionTemplate can have properties, these properties are added to the
 * function object when it is created.
 *
 * A FunctionTemplate has a corresponding instance template which is
 * used to create object instances when the function is used as a
 * constructor. Properties added to the instance template are added to
 * each object instance.
 *
 * A FunctionTemplate can have a prototype template. The prototype template
 * is used to create the prototype object of the function.
 *
 * The following example shows how to use a FunctionTemplate:
 *
 * \code
 *    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(isolate);
 *    t->Set(isolate, "func_property", v8::Number::New(isolate, 1));
 *
 *    v8::Local<v8::Template> proto_t = t->PrototypeTemplate();
 *    proto_t->Set(isolate,
 *                 "proto_method",
 *                 v8::FunctionTemplate::New(isolate, InvokeCallback));
 *    proto_t->Set(isolate, "proto_const", v8::Number::New(isolate, 2));
 *
 *    v8::Local<v8::ObjectTemplate> instance_t = t->InstanceTemplate();
 *    instance_t->SetAccessor(
          String::NewFromUtf8Literal(isolate, "instance_accessor"),
 *        InstanceAccessorCallback);
 *    instance_t->SetHandler(
 *        NamedPropertyHandlerConfiguration(PropertyHandlerCallback));
 *    instance_t->Set(String::NewFromUtf8Literal(isolate, "instance_property"),
 *                    Number::New(isolate, 3));
 *
 *    v8::Local<v8::Function> function = t->GetFunction();
 *    v8::Local<v8::Object> instance = function->NewInstance();
 * \endcode
 *
 * Let's use "function" as the JS variable name of the function object
 * and "instance" for the instance object created above.  The function
 * and the instance will have the following properties:
 *
 * \code
 *   func_property in function == true;
 *   function.func_property == 1;
 *
 *   function.prototype.proto_method() invokes 'InvokeCallback'
 *   function.prototype.proto_const == 2;
 *
 *   instance instanceof function == true;
 *   instance.instance_accessor calls 'InstanceAccessorCallback'
 *   instance.instance_property == 3;
 * \endcode
 *
 * A FunctionTemplate can inherit from another one by calling the
 * FunctionTemplate::Inherit method.  The following graph illustrates
 * the semantics of inheritance:
 *
 * \code
 *   FunctionTemplate Parent  -> Parent() . prototype -> { }
 *     ^                                                  ^
 *     | Inherit(Parent)                                  | .__proto__
 *     |                                                  |
 *   FunctionTemplate Child   -> Child()  . prototype -> { }
 * \endcode
 *
 * A FunctionTemplate 'Child' inherits from 'Parent', the prototype
 * object of the Child() function has __proto__ pointing to the
 * Parent() function's prototype object. An instance of the Child
 * function has all properties on Parent's instance templates.
 *
 * Let Parent be the FunctionTemplate initialized in the previous
 * section and create a Child FunctionTemplate by:
 *
 * \code
 *   Local<FunctionTemplate> parent = t;
 *   Local<FunctionTemplate> child = FunctionTemplate::New();
 *   child->Inherit(parent);
 *
 *   Local<Function> child_function = child->GetFunction();
 *   Local<Object> child_instance = child_function->NewInstance();
 * \endcode
 *
 * The Child function and Child instance will have the following
 * properties:
 *
 * \code
 *   child_func.prototype.__proto__ == function.prototype;
 *   child_instance.instance_accessor calls 'InstanceAccessorCallback'
 *   child_instance.instance_property == 3;
 * \endcode
 *
 * The additional 'c_function' parameter refers to a fast API call, which
 * must not trigger GC or JavaScript execution, or call into V8 in other
 * ways. For more information how to define them, see
 * include/v8-fast-api-calls.h. Please note that this feature is still
 * experimental.
 */
class V8_EXPORT FunctionTemplate : public Template {
 public:
  /** Creates a function template.*/
  static Local<FunctionTemplate> New(
      Isolate* isolate, FunctionCallback callback = nullptr,
      Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0,
      ConstructorBehavior behavior = ConstructorBehavior::kAllow,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect,
      const CFunction* c_function = nullptr, uint16_t instance_type = 0,
      uint16_t allowed_receiver_instance_type_range_start = 0,
      uint16_t allowed_receiver_instance_type_range_end = 0);

  /** Creates a function template for multiple overloaded fast API calls.*/
  static Local<FunctionTemplate> NewWithCFunctionOverloads(
      Isolate* isolate, FunctionCallback callback = nullptr,
      Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0,
      ConstructorBehavior behavior = ConstructorBehavior::kAllow,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect,
      const MemorySpan<const CFunction>& c_function_overloads = {});

  /**
   * Creates a function template backed/cached by a private property.
   */
  static Local<FunctionTemplate> NewWithCache(
      Isolate* isolate, FunctionCallback callback,
      Local<Private> cache_property, Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect);

  /** Returns the unique function instance in the current execution context.*/
  V8_WARN_UNUSED_RESULT MaybeLocal<Function> GetFunction(
      Local<Context> context);

  /**
   * Similar to Context::NewRemoteContext, this creates an instance that
   * isn't backed by an actual object.
   *
   * The InstanceTemplate of this FunctionTemplate must have access checks with
   * handlers installed.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewRemoteInstance();

  /**
   * Set the call-handler callback for a FunctionTemplate.  This
   * callback is called whenever the function created from this
   * FunctionTemplate is called. The 'c_function' represents a fast
   * API call, see the comment above the class declaration.
   */
  void SetCallHandler(
      FunctionCallback callback, Local<Value> data = Local<Value>(),
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect,
      const MemorySpan<const CFunction>& c_function_overloads = {});

  /** Set the predefined length property for the FunctionTemplate. */
  void SetLength(int length);

  /** Get the InstanceTemplate. */
  Local<ObjectTemplate> InstanceTemplate();

  /**
   * Causes the function template to inherit from a parent function template.
   * This means the function's prototype.__proto__ is set to the parent
   * function's prototype.
   **/
  void Inherit(Local<FunctionTemplate> parent);

  /**
   * A PrototypeTemplate is the template used to create the prototype object
   * of the function created by this template.
   */
  Local<ObjectTemplate> PrototypeTemplate();

  /**
   * A PrototypeProviderTemplate is another function template whose prototype
   * property is used for this template. This is mutually exclusive with setting
   * a prototype template indirectly by calling PrototypeTemplate() or using
   * Inherit().
   **/
  void SetPrototypeProviderTemplate(Local<FunctionTemplate> prototype_provider);

  /**
   * Set the class name of the FunctionTemplate.  This is used for
   * printing objects created with the function created from the
   * FunctionTemplate as its constructor.
   */
  void SetClassName(Local<String> name);

  /**
   * When set to true, no access check will be performed on the receiver of a
   * function call.  Currently defaults to true, but this is subject to change.
   */
  void SetAcceptAnyReceiver(bool value);

  /**
   * Sets the ReadOnly flag in the attributes of the 'prototype' property
   * of functions created from this FunctionTemplate to true.
   */
  void ReadOnlyPrototype();

  /**
   * Removes the prototype property from functions created from this
   * FunctionTemplate.
   */
  void RemovePrototype();

  /**
   * Returns true if the given object is an instance of this function
   * template.
   */
  bool HasInstance(Local<Value> object);

  /**
   * Returns true if the given value is an API object that was constructed by an
   * instance of this function template (without checking for inheriting
   * function templates).
   *
   * This is an experimental feature and may still change significantly.
   */
  bool IsLeafTemplateForApiObject(v8::Local<v8::Value> value) const;

  V8_INLINE static FunctionTemplate* Cast(Data* data);

 private:
  FunctionTemplate();

  static void CheckCast(Data* that);
  friend class Context;
  friend class ObjectTemplate;
};

/**
 * Configuration flags for v8::NamedPropertyHandlerConfiguration or
 * v8::IndexedPropertyHandlerConfiguration.
 */
enum class PropertyHandlerFlags {
  /**
   * None.
   */
  kNone = 0,

  /**
   * See ALL_CAN_READ above.
   */
  kAllCanRead = 1,

  /** Will not call into interceptor for properties on the receiver or prototype
   * chain, i.e., only call into interceptor for properties that do not exist.
   * Currently only valid for named interceptors.
   */
  kNonMasking = 1 << 1,

  /**
   * Will not call into interceptor for symbol lookup.  Only meaningful for
   * named interceptors.
   */
  kOnlyInterceptStrings = 1 << 2,

  /**
   * The getter, query, enumerator callbacks do not produce side effects.
   */
  kHasNoSideEffect = 1 << 3,
};

struct NamedPropertyHandlerConfiguration {
  NamedPropertyHandlerConfiguration(
      GenericNamedPropertyGetterCallback getter,
      GenericNamedPropertySetterCallback setter,
      GenericNamedPropertyQueryCallback query,
      GenericNamedPropertyDeleterCallback deleter,
      GenericNamedPropertyEnumeratorCallback enumerator,
      GenericNamedPropertyDefinerCallback definer,
      GenericNamedPropertyDescriptorCallback descriptor,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  NamedPropertyHandlerConfiguration(
      /** Note: getter is required */
      GenericNamedPropertyGetterCallback getter = nullptr,
      GenericNamedPropertySetterCallback setter = nullptr,
      GenericNamedPropertyQueryCallback query = nullptr,
      GenericNamedPropertyDeleterCallback deleter = nullptr,
      GenericNamedPropertyEnumeratorCallback enumerator = nullptr,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(nullptr),
        descriptor(nullptr),
        data(data),
        flags(flags) {}

  NamedPropertyHandlerConfiguration(
      GenericNamedPropertyGetterCallback getter,
      GenericNamedPropertySetterCallback setter,
      GenericNamedPropertyDescriptorCallback descriptor,
      GenericNamedPropertyDeleterCallback deleter,
      GenericNamedPropertyEnumeratorCallback enumerator,
      GenericNamedPropertyDefinerCallback definer,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(nullptr),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  GenericNamedPropertyGetterCallback getter;
  GenericNamedPropertySetterCallback setter;
  GenericNamedPropertyQueryCallback query;
  GenericNamedPropertyDeleterCallback deleter;
  GenericNamedPropertyEnumeratorCallback enumerator;
  GenericNamedPropertyDefinerCallback definer;
  GenericNamedPropertyDescriptorCallback descriptor;
  Local<Value> data;
  PropertyHandlerFlags flags;
};

struct IndexedPropertyHandlerConfiguration {
  IndexedPropertyHandlerConfiguration(
      IndexedPropertyGetterCallback getter,
      IndexedPropertySetterCallback setter, IndexedPropertyQueryCallback query,
      IndexedPropertyDeleterCallback deleter,
      IndexedPropertyEnumeratorCallback enumerator,
      IndexedPropertyDefinerCallback definer,
      IndexedPropertyDescriptorCallback descriptor,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  IndexedPropertyHandlerConfiguration(
      /** Note: getter is required */
      IndexedPropertyGetterCallback getter = nullptr,
      IndexedPropertySetterCallback setter = nullptr,
      IndexedPropertyQueryCallback query = nullptr,
      IndexedPropertyDeleterCallback deleter = nullptr,
      IndexedPropertyEnumeratorCallback enumerator = nullptr,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(nullptr),
        descriptor(nullptr),
        data(data),
        flags(flags) {}

  IndexedPropertyHandlerConfiguration(
      IndexedPropertyGetterCallback getter,
      IndexedPropertySetterCallback setter,
      IndexedPropertyDescriptorCallback descriptor,
      IndexedPropertyDeleterCallback deleter,
      IndexedPropertyEnumeratorCallback enumerator,
      IndexedPropertyDefinerCallback definer,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(nullptr),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  IndexedPropertyGetterCallback getter;
  IndexedPropertySetterCallback setter;
  IndexedPropertyQueryCallback query;
  IndexedPropertyDeleterCallback deleter;
  IndexedPropertyEnumeratorCallback enumerator;
  IndexedPropertyDefinerCallback definer;
  IndexedPropertyDescriptorCallback descriptor;
  Local<Value> data;
  PropertyHandlerFlags flags;
};

/**
 * An ObjectTemplate is used to create objects at runtime.
 *
 * Properties added to an ObjectTemplate are added to each object
 * created from the ObjectTemplate.
 */
class V8_EXPORT ObjectTemplate : public Template {
 public:
  /** Creates an ObjectTemplate. */
  static Local<ObjectTemplate> New(
      Isolate* isolate,
      Local<FunctionTemplate> constructor = Local<FunctionTemplate>());

  /** Creates a new instance of this template.*/
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(Local<Context> context);

  /**
   * Sets an accessor on the object template.
   *
   * Whenever the property with the given name is accessed on objects
   * created from this ObjectTemplate the getter and setter callbacks
   * are called instead of getting and setting the property directly
   * on the JavaScript object.
   *
   * \param name The name of the property for which an accessor is added.
   * \param getter The callback to invoke when getting the property.
   * \param setter The callback to invoke when setting the property.
   * \param data A piece of data that will be passed to the getter and setter
   *   callbacks whenever they are invoked.
   * \param settings Access control settings for the accessor. This is a bit
   *   field consisting of one of more of
   *   DEFAULT = 0, ALL_CAN_READ = 1, or ALL_CAN_WRITE = 2.
   *   The default is to not allow cross-context access.
   *   ALL_CAN_READ means that all cross-context reads are allowed.
   *   ALL_CAN_WRITE means that all cross-context writes are allowed.
   *   The combination ALL_CAN_READ | ALL_CAN_WRITE can be used to allow all
   *   cross-context access.
   * \param attribute The attributes of the property for which an accessor
   *   is added.
   */
  void SetAccessor(
      Local<String> name, AccessorGetterCallback getter,
      AccessorSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), AccessControl settings = DEFAULT,
      PropertyAttribute attribute = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);
  void SetAccessor(
      Local<Name> name, AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), AccessControl settings = DEFAULT,
      PropertyAttribute attribute = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Sets a named property handler on the object template.
   *
   * Whenever a property whose name is a string or a symbol is accessed on
   * objects created from this object template, the provided callback is
   * invoked instead of accessing the property directly on the JavaScript
   * object.
   *
   * @param configuration The NamedPropertyHandlerConfiguration that defines the
   * callbacks to invoke when accessing a property.
   */
  void SetHandler(const NamedPropertyHandlerConfiguration& configuration);

  /**
   * Sets an indexed property handler on the object template.
   *
   * Whenever an indexed property is accessed on objects created from
   * this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * \param getter The callback to invoke when getting a property.
   * \param setter The callback to invoke when setting a property.
   * \param query The callback to invoke to check if an object has a property.
   * \param deleter The callback to invoke when deleting a property.
   * \param enumerator The callback to invoke to enumerate all the indexed
   *   properties of an object.
   * \param data A piece of data that will be passed to the callbacks
   *   whenever they are invoked.
   */
  // TODO(dcarney): deprecate
  void SetIndexedPropertyHandler(
      IndexedPropertyGetterCallback getter,
      IndexedPropertySetterCallback setter = nullptr,
      IndexedPropertyQueryCallback query = nullptr,
      IndexedPropertyDeleterCallback deleter = nullptr,
      IndexedPropertyEnumeratorCallback enumerator = nullptr,
      Local<Value> data = Local<Value>()) {
    SetHandler(IndexedPropertyHandlerConfiguration(getter, setter, query,
                                                   deleter, enumerator, data));
  }

  /**
   * Sets an indexed property handler on the object template.
   *
   * Whenever an indexed property is accessed on objects created from
   * this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * @param configuration The IndexedPropertyHandlerConfiguration that defines
   * the callbacks to invoke when accessing a property.
   */
  void SetHandler(const IndexedPropertyHandlerConfiguration& configuration);

  /**
   * Sets the callback to be used when calling instances created from
   * this template as a function.  If no callback is set, instances
   * behave like normal JavaScript objects that cannot be called as a
   * function.
   */
  void SetCallAsFunctionHandler(FunctionCallback callback,
                                Local<Value> data = Local<Value>());

  /**
   * Mark object instances of the template as undetectable.
   *
   * In many ways, undetectable objects behave as though they are not
   * there.  They behave like 'undefined' in conditionals and when
   * printed.  However, properties can be accessed and called as on
   * normal objects.
   */
  void MarkAsUndetectable();

  /**
   * Sets access check callback on the object template and enables access
   * checks.
   *
   * When accessing properties on instances of this object template,
   * the access check callback will be called to determine whether or
   * not to allow cross-context access to the properties.
   */
  void SetAccessCheckCallback(AccessCheckCallback callback,
                              Local<Value> data = Local<Value>());

  /**
   * Like SetAccessCheckCallback but invokes an interceptor on failed access
   * checks instead of looking up all-can-read properties. You can only use
   * either this method or SetAccessCheckCallback, but not both at the same
   * time.
   */
  void SetAccessCheckCallbackAndHandler(
      AccessCheckCallback callback,
      const NamedPropertyHandlerConfiguration& named_handler,
      const IndexedPropertyHandlerConfiguration& indexed_handler,
      Local<Value> data = Local<Value>());

  /**
   * Gets the number of internal fields for objects generated from
   * this template.
   */
  int InternalFieldCount() const;

  /**
   * Sets the number of internal fields for objects generated from
   * this template.
   */
  void SetInternalFieldCount(int value);

  /**
   * Returns true if the object will be an immutable prototype exotic object.
   */
  bool IsImmutableProto() const;

  /**
   * Makes the ObjectTemplate for an immutable prototype exotic object, with an
   * immutable __proto__.
   */
  void SetImmutableProto();

  /**
   * Support for TC39 "dynamic code brand checks" proposal.
   *
   * This API allows to mark (& query) objects as "code like", which causes
   * them to be treated like Strings in the context of eval and function
   * constructor.
   *
   * Reference: https://github.com/tc39/proposal-dynamic-code-brand-checks
   */
  void SetCodeLike();
  bool IsCodeLike() const;

  V8_INLINE static ObjectTemplate* Cast(Data* data);

 private:
  ObjectTemplate();
  static Local<ObjectTemplate> New(internal::Isolate* isolate,
                                   Local<FunctionTemplate> constructor);
  static void CheckCast(Data* that);
  friend class FunctionTemplate;
};

/**
 * A Signature specifies which receiver is valid for a function.
 *
 * A receiver matches a given signature if the receiver (or any of its
 * hidden prototypes) was created from the signature's FunctionTemplate, or
 * from a FunctionTemplate that inherits directly or indirectly from the
 * signature's FunctionTemplate.
 */
class V8_EXPORT Signature : public Data {
 public:
  static Local<Signature> New(
      Isolate* isolate,
      Local<FunctionTemplate> receiver = Local<FunctionTemplate>());

  V8_INLINE static Signature* Cast(Data* data);

 private:
  Signature();

  static void CheckCast(Data* that);
};

// --- Implementation ---

void Template::Set(Isolate* isolate, const char* name, Local<Data> value,
                   PropertyAttribute attributes) {
  Set(String::NewFromUtf8(isolate, name, NewStringType::kInternalized)
          .ToLocalChecked(),
      value, attributes);
}

FunctionTemplate* FunctionTemplate::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<FunctionTemplate*>(data);
}

ObjectTemplate* ObjectTemplate::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<ObjectTemplate*>(data);
}

Signature* Signature::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<Signature*>(data);
}

}  // namespace v8

#endif  // INCLUDE_V8_TEMPLATE_H_
