#include <climits>  // INT_MAX
#include <cmath>
#include <algorithm>
#define NAPI_EXPERIMENTAL
#include "env-inl.h"
#include "js_native_api_v8.h"
#include "js_native_api.h"
#include "util-inl.h"

#define CHECK_MAYBE_NOTHING(env, maybe, status) \
  RETURN_STATUS_IF_FALSE((env), !((maybe).IsNothing()), (status))

#define CHECK_TO_NUMBER(env, context, result, src) \
  CHECK_TO_TYPE((env), Number, (context), (result), (src), napi_number_expected)

// n-api defines NAPI_AUTO_LENGHTH as the indicator that a string
// is null terminated. For V8 the equivalent is -1. The assert
// validates that our cast of NAPI_AUTO_LENGTH results in -1 as
// needed by V8.
#define CHECK_NEW_FROM_UTF8_LEN(env, result, str, len)                   \
  do {                                                                   \
    static_assert(static_cast<int>(NAPI_AUTO_LENGTH) == -1,              \
                  "Casting NAPI_AUTO_LENGTH to int must result in -1");  \
    RETURN_STATUS_IF_FALSE((env),                                        \
        (len == NAPI_AUTO_LENGTH) || len <= INT_MAX,                     \
        napi_invalid_arg);                                               \
    RETURN_STATUS_IF_FALSE((env),                                        \
        (str) != nullptr,                                                \
        napi_invalid_arg);                                               \
    auto str_maybe = v8::String::NewFromUtf8(                            \
        (env)->isolate, (str), v8::NewStringType::kInternalized,         \
        static_cast<int>(len));                                          \
    CHECK_MAYBE_EMPTY((env), str_maybe, napi_generic_failure);           \
    (result) = str_maybe.ToLocalChecked();                               \
  } while (0)

#define CHECK_NEW_FROM_UTF8(env, result, str) \
  CHECK_NEW_FROM_UTF8_LEN((env), (result), (str), NAPI_AUTO_LENGTH)

#define CREATE_TYPED_ARRAY(                                                    \
    env, type, size_of_element, buffer, byte_offset, length, out)              \
  do {                                                                         \
    if ((size_of_element) > 1) {                                               \
      THROW_RANGE_ERROR_IF_FALSE(                                              \
          (env), (byte_offset) % (size_of_element) == 0,                       \
          "ERR_NAPI_INVALID_TYPEDARRAY_ALIGNMENT",                             \
          "start offset of "#type" should be a multiple of "#size_of_element); \
    }                                                                          \
    THROW_RANGE_ERROR_IF_FALSE((env), (length) * (size_of_element) +           \
        (byte_offset) <= buffer->ByteLength(),                                 \
        "ERR_NAPI_INVALID_TYPEDARRAY_LENGTH",                                  \
        "Invalid typed array length");                                         \
    (out) = v8::type::New((buffer), (byte_offset), (length));                  \
  } while (0)

namespace v8impl {

namespace {

inline static napi_status
V8NameFromPropertyDescriptor(napi_env env,
                             const napi_property_descriptor* p,
                             v8::Local<v8::Name>* result) {
  if (p->utf8name != nullptr) {
    CHECK_NEW_FROM_UTF8(env, *result, p->utf8name);
  } else {
    v8::Local<v8::Value> property_value =
      v8impl::V8LocalValueFromJsValue(p->name);

    RETURN_STATUS_IF_FALSE(env, property_value->IsName(), napi_name_expected);
    *result = property_value.As<v8::Name>();
  }

  return napi_ok;
}

// convert from n-api property attributes to v8::PropertyAttribute
inline static v8::PropertyAttribute V8PropertyAttributesFromDescriptor(
    const napi_property_descriptor* descriptor) {
  unsigned int attribute_flags = v8::PropertyAttribute::None;

  // The napi_writable attribute is ignored for accessor descriptors, but
  // V8 would throw `TypeError`s on assignment with nonexistence of a setter.
  if ((descriptor->getter == nullptr && descriptor->setter == nullptr) &&
    (descriptor->attributes & napi_writable) == 0) {
    attribute_flags |= v8::PropertyAttribute::ReadOnly;
  }

  if ((descriptor->attributes & napi_enumerable) == 0) {
    attribute_flags |= v8::PropertyAttribute::DontEnum;
  }
  if ((descriptor->attributes & napi_configurable) == 0) {
    attribute_flags |= v8::PropertyAttribute::DontDelete;
  }

  return static_cast<v8::PropertyAttribute>(attribute_flags);
}

inline static napi_deferred
JsDeferredFromNodePersistent(v8impl::Persistent<v8::Value>* local) {
  return reinterpret_cast<napi_deferred>(local);
}

inline static v8impl::Persistent<v8::Value>*
NodePersistentFromJsDeferred(napi_deferred local) {
  return reinterpret_cast<v8impl::Persistent<v8::Value>*>(local);
}

class HandleScopeWrapper {
 public:
  explicit HandleScopeWrapper(v8::Isolate* isolate) : scope(isolate) {}

 private:
  v8::HandleScope scope;
};

// In node v0.10 version of v8, there is no EscapableHandleScope and the
// node v0.10 port use HandleScope::Close(Local<T> v) to mimic the behavior
// of a EscapableHandleScope::Escape(Local<T> v), but it is not the same
// semantics. This is an example of where the api abstraction fail to work
// across different versions.
class EscapableHandleScopeWrapper {
 public:
  explicit EscapableHandleScopeWrapper(v8::Isolate* isolate)
      : scope(isolate), escape_called_(false) {}
  bool escape_called() const {
    return escape_called_;
  }
  template <typename T>
  v8::Local<T> Escape(v8::Local<T> handle) {
    escape_called_ = true;
    return scope.Escape(handle);
  }

 private:
  v8::EscapableHandleScope scope;
  bool escape_called_;
};

inline static napi_handle_scope
JsHandleScopeFromV8HandleScope(HandleScopeWrapper* s) {
  return reinterpret_cast<napi_handle_scope>(s);
}

inline static HandleScopeWrapper*
V8HandleScopeFromJsHandleScope(napi_handle_scope s) {
  return reinterpret_cast<HandleScopeWrapper*>(s);
}

inline static napi_escapable_handle_scope
JsEscapableHandleScopeFromV8EscapableHandleScope(
    EscapableHandleScopeWrapper* s) {
  return reinterpret_cast<napi_escapable_handle_scope>(s);
}

inline static EscapableHandleScopeWrapper*
V8EscapableHandleScopeFromJsEscapableHandleScope(
    napi_escapable_handle_scope s) {
  return reinterpret_cast<EscapableHandleScopeWrapper*>(s);
}

inline static napi_status ConcludeDeferred(napi_env env,
                                           napi_deferred deferred,
                                           napi_value result,
                                           bool is_resolved) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->isolate->GetCurrentContext();
  v8impl::Persistent<v8::Value>* deferred_ref =
      NodePersistentFromJsDeferred(deferred);
  v8::Local<v8::Value> v8_deferred =
      v8::Local<v8::Value>::New(env->isolate, *deferred_ref);

  auto v8_resolver = v8::Local<v8::Promise::Resolver>::Cast(v8_deferred);

  v8::Maybe<bool> success = is_resolved ?
      v8_resolver->Resolve(context, v8impl::V8LocalValueFromJsValue(result)) :
      v8_resolver->Reject(context, v8impl::V8LocalValueFromJsValue(result));

  delete deferred_ref;

  RETURN_STATUS_IF_FALSE(env, success.FromMaybe(false), napi_generic_failure);

  return GET_RETURN_STATUS(env);
}

// Wrapper around v8impl::Persistent that implements reference counting.
class RefBase : protected Finalizer, RefTracker {
 protected:
  RefBase(napi_env env,
          uint32_t initial_refcount,
          bool delete_self,
          napi_finalize finalize_callback,
          void* finalize_data,
          void* finalize_hint)
       : Finalizer(env, finalize_callback, finalize_data, finalize_hint),
        _refcount(initial_refcount),
        _delete_self(delete_self) {
    Link(finalize_callback == nullptr
        ? &env->reflist
        : &env->finalizing_reflist);
  }

 public:
  static RefBase* New(napi_env env,
                      uint32_t initial_refcount,
                      bool delete_self,
                      napi_finalize finalize_callback,
                      void* finalize_data,
                      void* finalize_hint) {
    return new RefBase(env,
                       initial_refcount,
                       delete_self,
                       finalize_callback,
                       finalize_data,
                       finalize_hint);
  }

  inline void* Data() {
    return _finalize_data;
  }

  // Delete is called in 2 ways. Either from the finalizer or
  // from one of Unwrap or napi_delete_reference.
  //
  // When it is called from Unwrap or napi_delete_reference we only
  // want to do the delete if the finalizer has already run or
  // cannot have been queued to run (ie the reference count is > 0),
  // otherwise we may crash when the finalizer does run.
  // If the finalizer may have been queued and has not already run
  // delay the delete until the finalizer runs by not doing the delete
  // and setting _delete_self to true so that the finalizer will
  // delete it when it runs.
  //
  // The second way this is called is from
  // the finalizer and _delete_self is set. In this case we
  // know we need to do the deletion so just do it.
  static inline void Delete(RefBase* reference) {
    reference->Unlink();
    if ((reference->RefCount() != 0) ||
        (reference->_delete_self) ||
        (reference->_finalize_ran)) {
      delete reference;
    } else {
      // defer until finalizer runs as
      // it may alread be queued
      reference->_delete_self = true;
    }
  }

  inline uint32_t Ref() {
    return ++_refcount;
  }

  inline uint32_t Unref() {
    if (_refcount == 0) {
        return 0;
    }
    return --_refcount;
  }

  inline uint32_t RefCount() {
    return _refcount;
  }

 protected:
  inline void Finalize(bool is_env_teardown = false) override {
    if (_finalize_callback != nullptr) {
      _env->CallIntoModuleThrow([&](napi_env env) {
        _finalize_callback(
            env,
            _finalize_data,
            _finalize_hint);
      });
    }

    // this is safe because if a request to delete the reference
    // is made in the finalize_callback it will defer deletion
    // to this block and set _delete_self to true
    if (_delete_self || is_env_teardown) {
      Delete(this);
    } else {
      _finalize_ran = true;
    }
  }

 private:
  uint32_t _refcount;
  bool _delete_self;
};

class Reference : public RefBase {
 protected:
  template <typename... Args>
  Reference(napi_env env,
            v8::Local<v8::Value> value,
            Args&&... args)
      : RefBase(env, std::forward<Args>(args)...),
            _persistent(env->isolate, value) {
    if (RefCount() == 0) {
      _persistent.SetWeak(
          this, FinalizeCallback, v8::WeakCallbackType::kParameter);
    }
  }

 public:
  static inline Reference* New(napi_env env,
                             v8::Local<v8::Value> value,
                             uint32_t initial_refcount,
                             bool delete_self,
                             napi_finalize finalize_callback = nullptr,
                             void* finalize_data = nullptr,
                             void* finalize_hint = nullptr) {
    return new Reference(env,
                         value,
                         initial_refcount,
                         delete_self,
                         finalize_callback,
                         finalize_data,
                         finalize_hint);
  }

  inline uint32_t Ref() {
    uint32_t refcount = RefBase::Ref();
    if (refcount == 1) {
      _persistent.ClearWeak();
    }
    return refcount;
  }

  inline uint32_t Unref() {
    uint32_t old_refcount = RefCount();
    uint32_t refcount = RefBase::Unref();
    if (old_refcount == 1 && refcount == 0) {
      _persistent.SetWeak(
          this, FinalizeCallback, v8::WeakCallbackType::kParameter);
    }
    return refcount;
  }

  inline v8::Local<v8::Value> Get() {
    if (_persistent.IsEmpty()) {
      return v8::Local<v8::Value>();
    } else {
      return v8::Local<v8::Value>::New(_env->isolate, _persistent);
    }
  }

 private:
  // The N-API finalizer callback may make calls into the engine. V8's heap is
  // not in a consistent state during the weak callback, and therefore it does
  // not support calls back into it. However, it provides a mechanism for adding
  // a finalizer which may make calls back into the engine by allowing us to
  // attach such a second-pass finalizer from the first pass finalizer. Thus,
  // we do that here to ensure that the N-API finalizer callback is free to call
  // into the engine.
  static void FinalizeCallback(const v8::WeakCallbackInfo<Reference>& data) {
    Reference* reference = data.GetParameter();

    // The reference must be reset during the first pass.
    reference->_persistent.Reset();

    data.SetSecondPassCallback(SecondPassCallback);
  }

  static void SecondPassCallback(const v8::WeakCallbackInfo<Reference>& data) {
    data.GetParameter()->Finalize();
  }

  v8impl::Persistent<v8::Value> _persistent;
};

class ArrayBufferReference final : public Reference {
 public:
  // Same signatures for ctor and New() as Reference, except this only works
  // with ArrayBuffers:
  template <typename... Args>
  explicit ArrayBufferReference(napi_env env,
                                v8::Local<v8::ArrayBuffer> value,
                                Args&&... args)
    : Reference(env, value, std::forward<Args>(args)...) {}

  template <typename... Args>
  static ArrayBufferReference* New(napi_env env,
                                   v8::Local<v8::ArrayBuffer> value,
                                   Args&&... args) {
    return new ArrayBufferReference(env, value, std::forward<Args>(args)...);
  }

 private:
  inline void Finalize(bool is_env_teardown) override {
    if (is_env_teardown) {
      v8::HandleScope handle_scope(_env->isolate);
      v8::Local<v8::Value> ab = Get();
      CHECK(!ab.IsEmpty());
      CHECK(ab->IsArrayBuffer());
      ab.As<v8::ArrayBuffer>()->Detach();
    }

    Reference::Finalize(is_env_teardown);
  }
};

enum UnwrapAction {
  KeepWrap,
  RemoveWrap
};

inline static napi_status Unwrap(napi_env env,
                                 napi_value js_object,
                                 void** result,
                                 UnwrapAction action) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, js_object);
  if (action == KeepWrap) {
    CHECK_ARG(env, result);
  }

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(js_object);
  RETURN_STATUS_IF_FALSE(env, value->IsObject(), napi_invalid_arg);
  v8::Local<v8::Object> obj = value.As<v8::Object>();

  auto val = obj->GetPrivate(context, NAPI_PRIVATE_KEY(context, wrapper))
      .ToLocalChecked();
  RETURN_STATUS_IF_FALSE(env, val->IsExternal(), napi_invalid_arg);
  Reference* reference =
      static_cast<v8impl::Reference*>(val.As<v8::External>()->Value());

  if (result) {
    *result = reference->Data();
  }

  if (action == RemoveWrap) {
    CHECK(obj->DeletePrivate(context, NAPI_PRIVATE_KEY(context, wrapper))
        .FromJust());
    Reference::Delete(reference);
  }

  return GET_RETURN_STATUS(env);
}

//=== Function napi_callback wrapper =================================

// Use this data structure to associate callback data with each N-API function
// exposed to JavaScript. The structure is stored in a v8::External which gets
// passed into our callback wrapper. This reduces the performance impact of
// calling through N-API.
// Ref: benchmark/misc/function_call
// Discussion (incl. perf. data): https://github.com/nodejs/node/pull/21072
struct CallbackBundle {
  napi_env       env;      // Necessary to invoke C++ NAPI callback
  void*          cb_data;  // The user provided callback data
  napi_callback  function_or_getter;
  napi_callback  setter;
};

// Base class extended by classes that wrap V8 function and property callback
// info.
class CallbackWrapper {
 public:
  CallbackWrapper(napi_value this_arg, size_t args_length, void* data)
      : _this(this_arg), _args_length(args_length), _data(data) {}

  virtual napi_value GetNewTarget() = 0;
  virtual void Args(napi_value* buffer, size_t bufferlength) = 0;
  virtual void SetReturnValue(napi_value value) = 0;

  napi_value This() { return _this; }

  size_t ArgsLength() { return _args_length; }

  void* Data() { return _data; }

 protected:
  const napi_value _this;
  const size_t _args_length;
  void* _data;
};

template <typename Info, napi_callback CallbackBundle::*FunctionField>
class CallbackWrapperBase : public CallbackWrapper {
 public:
  CallbackWrapperBase(const Info& cbinfo, const size_t args_length)
      : CallbackWrapper(JsValueFromV8LocalValue(cbinfo.This()),
                        args_length,
                        nullptr),
        _cbinfo(cbinfo) {
    _bundle = reinterpret_cast<CallbackBundle*>(
        v8::Local<v8::External>::Cast(cbinfo.Data())->Value());
    _data = _bundle->cb_data;
  }

  napi_value GetNewTarget() override { return nullptr; }

 protected:
  void InvokeCallback() {
    napi_callback_info cbinfo_wrapper = reinterpret_cast<napi_callback_info>(
        static_cast<CallbackWrapper*>(this));

    // All other pointers we need are stored in `_bundle`
    napi_env env = _bundle->env;
    napi_callback cb = _bundle->*FunctionField;

    napi_value result;
    env->CallIntoModuleThrow([&](napi_env env) {
      result = cb(env, cbinfo_wrapper);
    });

    if (result != nullptr) {
      this->SetReturnValue(result);
    }
  }

  const Info& _cbinfo;
  CallbackBundle* _bundle;
};

class FunctionCallbackWrapper
    : public CallbackWrapperBase<v8::FunctionCallbackInfo<v8::Value>,
                                 &CallbackBundle::function_or_getter> {
 public:
  static void Invoke(const v8::FunctionCallbackInfo<v8::Value>& info) {
    FunctionCallbackWrapper cbwrapper(info);
    cbwrapper.InvokeCallback();
  }

  explicit FunctionCallbackWrapper(
      const v8::FunctionCallbackInfo<v8::Value>& cbinfo)
      : CallbackWrapperBase(cbinfo, cbinfo.Length()) {}

  napi_value GetNewTarget() override {
    if (_cbinfo.IsConstructCall()) {
      return v8impl::JsValueFromV8LocalValue(_cbinfo.NewTarget());
    } else {
      return nullptr;
    }
  }

  /*virtual*/
  void Args(napi_value* buffer, size_t buffer_length) override {
    size_t i = 0;
    size_t min = std::min(buffer_length, _args_length);

    for (; i < min; i += 1) {
      buffer[i] = v8impl::JsValueFromV8LocalValue(_cbinfo[i]);
    }

    if (i < buffer_length) {
      napi_value undefined =
          v8impl::JsValueFromV8LocalValue(v8::Undefined(_cbinfo.GetIsolate()));
      for (; i < buffer_length; i += 1) {
        buffer[i] = undefined;
      }
    }
  }

  /*virtual*/
  void SetReturnValue(napi_value value) override {
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
    _cbinfo.GetReturnValue().Set(val);
  }
};

class GetterCallbackWrapper
    : public CallbackWrapperBase<v8::PropertyCallbackInfo<v8::Value>,
                                 &CallbackBundle::function_or_getter> {
 public:
  static void Invoke(v8::Local<v8::Name> property,
                     const v8::PropertyCallbackInfo<v8::Value>& info) {
    GetterCallbackWrapper cbwrapper(info);
    cbwrapper.InvokeCallback();
  }

  explicit GetterCallbackWrapper(
      const v8::PropertyCallbackInfo<v8::Value>& cbinfo)
      : CallbackWrapperBase(cbinfo, 0) {}

  /*virtual*/
  void Args(napi_value* buffer, size_t buffer_length) override {
    if (buffer_length > 0) {
      napi_value undefined =
          v8impl::JsValueFromV8LocalValue(v8::Undefined(_cbinfo.GetIsolate()));
      for (size_t i = 0; i < buffer_length; i += 1) {
        buffer[i] = undefined;
      }
    }
  }

  /*virtual*/
  void SetReturnValue(napi_value value) override {
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
    _cbinfo.GetReturnValue().Set(val);
  }
};

class SetterCallbackWrapper
    : public CallbackWrapperBase<v8::PropertyCallbackInfo<void>,
                                 &CallbackBundle::setter> {
 public:
  static void Invoke(v8::Local<v8::Name> property,
                     v8::Local<v8::Value> value,
                     const v8::PropertyCallbackInfo<void>& info) {
    SetterCallbackWrapper cbwrapper(info, value);
    cbwrapper.InvokeCallback();
  }

  SetterCallbackWrapper(const v8::PropertyCallbackInfo<void>& cbinfo,
                        const v8::Local<v8::Value>& value)
      : CallbackWrapperBase(cbinfo, 1), _value(value) {}

  /*virtual*/
  void Args(napi_value* buffer, size_t buffer_length) override {
    if (buffer_length > 0) {
      buffer[0] = v8impl::JsValueFromV8LocalValue(_value);

      if (buffer_length > 1) {
        napi_value undefined = v8impl::JsValueFromV8LocalValue(
            v8::Undefined(_cbinfo.GetIsolate()));
        for (size_t i = 1; i < buffer_length; i += 1) {
          buffer[i] = undefined;
        }
      }
    }
  }

  /*virtual*/
  void SetReturnValue(napi_value value) override {
    // Ignore any value returned from a setter callback.
  }

 private:
  const v8::Local<v8::Value>& _value;
};

static void DeleteCallbackBundle(napi_env env, void* data, void* hint) {
  CallbackBundle* bundle = static_cast<CallbackBundle*>(data);
  delete bundle;
}

// Creates an object to be made available to the static function callback
// wrapper, used to retrieve the native callback function and data pointer.
static
v8::Local<v8::Value> CreateFunctionCallbackData(napi_env env,
                                                napi_callback cb,
                                                void* data) {
  CallbackBundle* bundle = new CallbackBundle();
  bundle->function_or_getter = cb;
  bundle->cb_data = data;
  bundle->env = env;
  v8::Local<v8::Value> cbdata = v8::External::New(env->isolate, bundle);
  Reference::New(env, cbdata, 0, true, DeleteCallbackBundle, bundle, nullptr);

  return cbdata;
}

enum WrapType {
  retrievable,
  anonymous
};

template <WrapType wrap_type>
inline napi_status Wrap(napi_env env,
                        napi_value js_object,
                        void* native_object,
                        napi_finalize finalize_cb,
                        void* finalize_hint,
                        napi_ref* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, js_object);

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(js_object);
  RETURN_STATUS_IF_FALSE(env, value->IsObject(), napi_invalid_arg);
  v8::Local<v8::Object> obj = value.As<v8::Object>();

  if (wrap_type == retrievable) {
    // If we've already wrapped this object, we error out.
    RETURN_STATUS_IF_FALSE(env,
        !obj->HasPrivate(context, NAPI_PRIVATE_KEY(context, wrapper))
            .FromJust(),
        napi_invalid_arg);
  } else if (wrap_type == anonymous) {
    // If no finalize callback is provided, we error out.
    CHECK_ARG(env, finalize_cb);
  }

  v8impl::Reference* reference = nullptr;
  if (result != nullptr) {
    // The returned reference should be deleted via napi_delete_reference()
    // ONLY in response to the finalize callback invocation. (If it is deleted
    // before then, then the finalize callback will never be invoked.)
    // Therefore a finalize callback is required when returning a reference.
    CHECK_ARG(env, finalize_cb);
    reference = v8impl::Reference::New(
        env, obj, 0, false, finalize_cb, native_object, finalize_hint);
    *result = reinterpret_cast<napi_ref>(reference);
  } else {
    // Create a self-deleting reference.
    reference = v8impl::Reference::New(env, obj, 0, true, finalize_cb,
        native_object, finalize_cb == nullptr ? nullptr : finalize_hint);
  }

  if (wrap_type == retrievable) {
    CHECK(obj->SetPrivate(context, NAPI_PRIVATE_KEY(context, wrapper),
          v8::External::New(env->isolate, reference)).FromJust());
  }

  return GET_RETURN_STATUS(env);
}

}  // end of anonymous namespace

}  // end of namespace v8impl

// Warning: Keep in-sync with napi_status enum
static
const char* error_messages[] = {nullptr,
                                "Invalid argument",
                                "An object was expected",
                                "A string was expected",
                                "A string or symbol was expected",
                                "A function was expected",
                                "A number was expected",
                                "A boolean was expected",
                                "An array was expected",
                                "Unknown failure",
                                "An exception is pending",
                                "The async work item was cancelled",
                                "napi_escape_handle already called on scope",
                                "Invalid handle scope usage",
                                "Invalid callback scope usage",
                                "Thread-safe function queue is full",
                                "Thread-safe function handle is closing",
                                "A bigint was expected",
                                "A date was expected",
                                "An arraybuffer was expected",
                                "A detachable arraybuffer was expected",
};

napi_status napi_get_last_error_info(napi_env env,
                                     const napi_extended_error_info** result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  // The value of the constant below must be updated to reference the last
  // message in the `napi_status` enum each time a new error message is added.
  // We don't have a napi_status_last as this would result in an ABI
  // change each time a message was added.
  const int last_status = napi_detachable_arraybuffer_expected;

  static_assert(
      NAPI_ARRAYSIZE(error_messages) == last_status + 1,
      "Count of error messages must match count of error values");
  CHECK_LE(env->last_error.error_code, last_status);

  // Wait until someone requests the last error information to fetch the error
  // message string
  env->last_error.error_message =
      error_messages[env->last_error.error_code];

  *result = &(env->last_error);
  return napi_ok;
}

napi_status napi_create_function(napi_env env,
                                 const char* utf8name,
                                 size_t length,
                                 napi_callback cb,
                                 void* callback_data,
                                 napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);
  CHECK_ARG(env, cb);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Function> return_value;
  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Value> cbdata =
      v8impl::CreateFunctionCallbackData(env, cb, callback_data);

  RETURN_STATUS_IF_FALSE(env, !cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::Context> context = env->context();
  v8::MaybeLocal<v8::Function> maybe_function =
      v8::Function::New(context,
                        v8impl::FunctionCallbackWrapper::Invoke,
                        cbdata);
  CHECK_MAYBE_EMPTY(env, maybe_function, napi_generic_failure);

  return_value = scope.Escape(maybe_function.ToLocalChecked());

  if (utf8name != nullptr) {
    v8::Local<v8::String> name_string;
    CHECK_NEW_FROM_UTF8_LEN(env, name_string, utf8name, length);
    return_value->SetName(name_string);
  }

  *result = v8impl::JsValueFromV8LocalValue(return_value);

  return GET_RETURN_STATUS(env);
}

napi_status napi_define_class(napi_env env,
                              const char* utf8name,
                              size_t length,
                              napi_callback constructor,
                              void* callback_data,
                              size_t property_count,
                              const napi_property_descriptor* properties,
                              napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);
  CHECK_ARG(env, constructor);

  if (property_count > 0) {
    CHECK_ARG(env, properties);
  }

  v8::Isolate* isolate = env->isolate;

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Value> cbdata =
      v8impl::CreateFunctionCallbackData(env, constructor, callback_data);

  RETURN_STATUS_IF_FALSE(env, !cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
      isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  v8::Local<v8::String> name_string;
  CHECK_NEW_FROM_UTF8_LEN(env, name_string, utf8name, length);
  tpl->SetClassName(name_string);

  size_t static_property_count = 0;
  for (size_t i = 0; i < property_count; i++) {
    const napi_property_descriptor* p = properties + i;

    if ((p->attributes & napi_static) != 0) {
      // Static properties are handled separately below.
      static_property_count++;
      continue;
    }

    v8::Local<v8::Name> property_name;
    napi_status status =
        v8impl::V8NameFromPropertyDescriptor(env, p, &property_name);

    if (status != napi_ok) {
      return napi_set_last_error(env, status);
    }

    v8::PropertyAttribute attributes =
        v8impl::V8PropertyAttributesFromDescriptor(p);

    // This code is similar to that in napi_define_properties(); the
    // difference is it applies to a template instead of an object,
    // and preferred PropertyAttribute for lack of PropertyDescriptor
    // support on ObjectTemplate.
    if (p->getter != nullptr || p->setter != nullptr) {
      v8::Local<v8::FunctionTemplate> getter_tpl;
      v8::Local<v8::FunctionTemplate> setter_tpl;
      if (p->getter != nullptr) {
        v8::Local<v8::Value> getter_data =
            v8impl::CreateFunctionCallbackData(env, p->getter, p->data);

        getter_tpl = v8::FunctionTemplate::New(
            isolate, v8impl::FunctionCallbackWrapper::Invoke, getter_data);
      }
      if (p->setter != nullptr) {
        v8::Local<v8::Value> setter_data =
            v8impl::CreateFunctionCallbackData(env, p->setter, p->data);

        setter_tpl = v8::FunctionTemplate::New(
            isolate, v8impl::FunctionCallbackWrapper::Invoke, setter_data);
      }

      tpl->PrototypeTemplate()->SetAccessorProperty(
        property_name,
        getter_tpl,
        setter_tpl,
        attributes,
        v8::AccessControl::DEFAULT);
    } else if (p->method != nullptr) {
      v8::Local<v8::Value> cbdata =
          v8impl::CreateFunctionCallbackData(env, p->method, p->data);

      RETURN_STATUS_IF_FALSE(env, !cbdata.IsEmpty(), napi_generic_failure);

      v8::Local<v8::FunctionTemplate> t =
        v8::FunctionTemplate::New(isolate,
          v8impl::FunctionCallbackWrapper::Invoke,
          cbdata,
          v8::Signature::New(isolate, tpl));

      tpl->PrototypeTemplate()->Set(property_name, t, attributes);
    } else {
      v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);
      tpl->PrototypeTemplate()->Set(property_name, value, attributes);
    }
  }

  v8::Local<v8::Context> context = env->context();
  *result = v8impl::JsValueFromV8LocalValue(
      scope.Escape(tpl->GetFunction(context).ToLocalChecked()));

  if (static_property_count > 0) {
    std::vector<napi_property_descriptor> static_descriptors;
    static_descriptors.reserve(static_property_count);

    for (size_t i = 0; i < property_count; i++) {
      const napi_property_descriptor* p = properties + i;
      if ((p->attributes & napi_static) != 0) {
        static_descriptors.push_back(*p);
      }
    }

    napi_status status =
        napi_define_properties(env,
                               *result,
                               static_descriptors.size(),
                               static_descriptors.data());
    if (status != napi_ok) return status;
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_get_property_names(napi_env env,
                                    napi_value object,
                                    napi_value* result) {
  return napi_get_all_property_names(
      env,
      object,
      napi_key_include_prototypes,
      static_cast<napi_key_filter>(napi_key_enumerable |
                                   napi_key_skip_symbols),
      napi_key_numbers_to_strings,
      result);
}

napi_status napi_get_all_property_names(napi_env env,
                                        napi_value object,
                                        napi_key_collection_mode key_mode,
                                        napi_key_filter key_filter,
                                        napi_key_conversion key_conversion,
                                        napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(env, context, obj, object);

  v8::PropertyFilter filter = v8::PropertyFilter::ALL_PROPERTIES;
  if (key_filter & napi_key_writable) {
    filter =
        static_cast<v8::PropertyFilter>(filter |
                                        v8::PropertyFilter::ONLY_WRITABLE);
  }
  if (key_filter & napi_key_enumerable) {
    filter =
        static_cast<v8::PropertyFilter>(filter |
                                        v8::PropertyFilter::ONLY_ENUMERABLE);
  }
  if (key_filter & napi_key_configurable) {
    filter =
        static_cast<v8::PropertyFilter>(filter |
                                        v8::PropertyFilter::ONLY_WRITABLE);
  }
  if (key_filter & napi_key_skip_strings) {
    filter =
        static_cast<v8::PropertyFilter>(filter |
                                        v8::PropertyFilter::SKIP_STRINGS);
  }
  if (key_filter & napi_key_skip_symbols) {
    filter =
        static_cast<v8::PropertyFilter>(filter |
                                        v8::PropertyFilter::SKIP_SYMBOLS);
  }
  v8::KeyCollectionMode collection_mode;
  v8::KeyConversionMode conversion_mode;

  switch (key_mode) {
    case napi_key_include_prototypes:
      collection_mode = v8::KeyCollectionMode::kIncludePrototypes;
      break;
    case napi_key_own_only:
      collection_mode = v8::KeyCollectionMode::kOwnOnly;
      break;
    default:
      return napi_set_last_error(env, napi_invalid_arg);
  }

  switch (key_conversion) {
    case napi_key_keep_numbers:
      conversion_mode = v8::KeyConversionMode::kKeepNumbers;
      break;
    case napi_key_numbers_to_strings:
      conversion_mode = v8::KeyConversionMode::kConvertToString;
      break;
    default:
      return napi_set_last_error(env, napi_invalid_arg);
  }

  v8::MaybeLocal<v8::Array> maybe_all_propertynames =
      obj->GetPropertyNames(context,
                            collection_mode,
                            filter,
                            v8::IndexFilter::kIncludeIndices,
                            conversion_mode);

  CHECK_MAYBE_EMPTY_WITH_PREAMBLE(
      env, maybe_all_propertynames, napi_generic_failure);

  *result =
      v8impl::JsValueFromV8LocalValue(maybe_all_propertynames.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_set_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              napi_value value) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, key);
  CHECK_ARG(env, value);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  v8::Maybe<bool> set_maybe = obj->Set(context, k, val);

  RETURN_STATUS_IF_FALSE(env, set_maybe.FromMaybe(false), napi_generic_failure);
  return GET_RETURN_STATUS(env);
}

napi_status napi_has_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);
  CHECK_ARG(env, key);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Maybe<bool> has_maybe = obj->Has(context, k);

  CHECK_MAYBE_NOTHING(env, has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, key);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  auto get_maybe = obj->Get(context, k);

  CHECK_MAYBE_EMPTY(env, get_maybe, napi_generic_failure);

  v8::Local<v8::Value> val = get_maybe.ToLocalChecked();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS(env);
}

napi_status napi_delete_property(napi_env env,
                                 napi_value object,
                                 napi_value key,
                                 bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, key);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);
  v8::Maybe<bool> delete_maybe = obj->Delete(context, k);
  CHECK_MAYBE_NOTHING(env, delete_maybe, napi_generic_failure);

  if (result != nullptr)
    *result = delete_maybe.FromMaybe(false);

  return GET_RETURN_STATUS(env);
}

napi_status napi_has_own_property(napi_env env,
                                  napi_value object,
                                  napi_value key,
                                  bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, key);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);
  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  RETURN_STATUS_IF_FALSE(env, k->IsName(), napi_name_expected);
  v8::Maybe<bool> has_maybe = obj->HasOwnProperty(context, k.As<v8::Name>());
  CHECK_MAYBE_NOTHING(env, has_maybe, napi_generic_failure);
  *result = has_maybe.FromMaybe(false);

  return GET_RETURN_STATUS(env);
}

napi_status napi_set_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8name,
                                    napi_value value) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, value);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Local<v8::Name> key;
  CHECK_NEW_FROM_UTF8(env, key, utf8name);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  v8::Maybe<bool> set_maybe = obj->Set(context, key, val);

  RETURN_STATUS_IF_FALSE(env, set_maybe.FromMaybe(false), napi_generic_failure);
  return GET_RETURN_STATUS(env);
}

napi_status napi_has_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8name,
                                    bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Local<v8::Name> key;
  CHECK_NEW_FROM_UTF8(env, key, utf8name);

  v8::Maybe<bool> has_maybe = obj->Has(context, key);

  CHECK_MAYBE_NOTHING(env, has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8name,
                                    napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Name> key;
  CHECK_NEW_FROM_UTF8(env, key, utf8name);

  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  auto get_maybe = obj->Get(context, key);

  CHECK_MAYBE_EMPTY(env, get_maybe, napi_generic_failure);

  v8::Local<v8::Value> val = get_maybe.ToLocalChecked();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS(env);
}

napi_status napi_set_element(napi_env env,
                             napi_value object,
                             uint32_t index,
                             napi_value value) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, value);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  auto set_maybe = obj->Set(context, index, val);

  RETURN_STATUS_IF_FALSE(env, set_maybe.FromMaybe(false), napi_generic_failure);

  return GET_RETURN_STATUS(env);
}

napi_status napi_has_element(napi_env env,
                             napi_value object,
                             uint32_t index,
                             bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Maybe<bool> has_maybe = obj->Has(context, index);

  CHECK_MAYBE_NOTHING(env, has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_element(napi_env env,
                             napi_value object,
                             uint32_t index,
                             napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  auto get_maybe = obj->Get(context, index);

  CHECK_MAYBE_EMPTY(env, get_maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(get_maybe.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_delete_element(napi_env env,
                                napi_value object,
                                uint32_t index,
                                bool* result) {
  NAPI_PREAMBLE(env);

  v8::Local<v8::Context> context = env->context();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);
  v8::Maybe<bool> delete_maybe = obj->Delete(context, index);
  CHECK_MAYBE_NOTHING(env, delete_maybe, napi_generic_failure);

  if (result != nullptr)
    *result = delete_maybe.FromMaybe(false);

  return GET_RETURN_STATUS(env);
}

napi_status napi_define_properties(napi_env env,
                                   napi_value object,
                                   size_t property_count,
                                   const napi_property_descriptor* properties) {
  NAPI_PREAMBLE(env);
  if (property_count > 0) {
    CHECK_ARG(env, properties);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(env, context, obj, object);

  for (size_t i = 0; i < property_count; i++) {
    const napi_property_descriptor* p = &properties[i];

    v8::Local<v8::Name> property_name;
    napi_status status =
        v8impl::V8NameFromPropertyDescriptor(env, p, &property_name);

    if (status != napi_ok) {
      return napi_set_last_error(env, status);
    }

    if (p->getter != nullptr || p->setter != nullptr) {
      v8::Local<v8::Value> local_getter;
      v8::Local<v8::Value> local_setter;

      if (p->getter != nullptr) {
        v8::Local<v8::Value> getter_data =
            v8impl::CreateFunctionCallbackData(env, p->getter, p->data);
        CHECK_MAYBE_EMPTY(env, getter_data, napi_generic_failure);

        v8::MaybeLocal<v8::Function> maybe_getter =
            v8::Function::New(context,
                              v8impl::FunctionCallbackWrapper::Invoke,
                              getter_data);
        CHECK_MAYBE_EMPTY(env, maybe_getter, napi_generic_failure);

        local_getter = maybe_getter.ToLocalChecked();
      }
      if (p->setter != nullptr) {
        v8::Local<v8::Value> setter_data =
            v8impl::CreateFunctionCallbackData(env, p->setter, p->data);
        CHECK_MAYBE_EMPTY(env, setter_data, napi_generic_failure);

        v8::MaybeLocal<v8::Function> maybe_setter =
            v8::Function::New(context,
                              v8impl::FunctionCallbackWrapper::Invoke,
                              setter_data);
        CHECK_MAYBE_EMPTY(env, maybe_setter, napi_generic_failure);
        local_setter = maybe_setter.ToLocalChecked();
      }

      v8::PropertyDescriptor descriptor(local_getter, local_setter);
      descriptor.set_enumerable((p->attributes & napi_enumerable) != 0);
      descriptor.set_configurable((p->attributes & napi_configurable) != 0);

      auto define_maybe = obj->DefineProperty(context,
                                              property_name,
                                              descriptor);

      if (!define_maybe.FromMaybe(false)) {
        return napi_set_last_error(env, napi_invalid_arg);
      }
    } else if (p->method != nullptr) {
      v8::Local<v8::Value> cbdata =
          v8impl::CreateFunctionCallbackData(env, p->method, p->data);

      CHECK_MAYBE_EMPTY(env, cbdata, napi_generic_failure);

      v8::MaybeLocal<v8::Function> maybe_fn =
          v8::Function::New(context,
                            v8impl::FunctionCallbackWrapper::Invoke,
                            cbdata);

      CHECK_MAYBE_EMPTY(env, maybe_fn, napi_generic_failure);

      v8::PropertyDescriptor descriptor(maybe_fn.ToLocalChecked(),
                                        (p->attributes & napi_writable) != 0);
      descriptor.set_enumerable((p->attributes & napi_enumerable) != 0);
      descriptor.set_configurable((p->attributes & napi_configurable) != 0);

      auto define_maybe = obj->DefineProperty(context,
                                              property_name,
                                              descriptor);

      if (!define_maybe.FromMaybe(false)) {
        return napi_set_last_error(env, napi_generic_failure);
      }
    } else {
      v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);

      v8::PropertyDescriptor descriptor(value,
                                        (p->attributes & napi_writable) != 0);
      descriptor.set_enumerable((p->attributes & napi_enumerable) != 0);
      descriptor.set_configurable((p->attributes & napi_configurable) != 0);

      auto define_maybe =
          obj->DefineProperty(context, property_name, descriptor);

      if (!define_maybe.FromMaybe(false)) {
        return napi_set_last_error(env, napi_invalid_arg);
      }
    }
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_is_array(napi_env env, napi_value value, bool* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  *result = val->IsArray();
  return napi_clear_last_error(env);
}

napi_status napi_get_array_length(napi_env env,
                                  napi_value value,
                                  uint32_t* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsArray(), napi_array_expected);

  v8::Local<v8::Array> arr = val.As<v8::Array>();
  *result = arr->Length();

  return GET_RETURN_STATUS(env);
}

napi_status napi_strict_equals(napi_env env,
                               napi_value lhs,
                               napi_value rhs,
                               bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, lhs);
  CHECK_ARG(env, rhs);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> a = v8impl::V8LocalValueFromJsValue(lhs);
  v8::Local<v8::Value> b = v8impl::V8LocalValueFromJsValue(rhs);

  *result = a->StrictEquals(b);
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_prototype(napi_env env,
                               napi_value object,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Local<v8::Value> val = obj->GetPrototype();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS(env);
}

napi_status napi_create_object(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Object::New(env->isolate));

  return napi_clear_last_error(env);
}

napi_status napi_create_array(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Array::New(env->isolate));

  return napi_clear_last_error(env);
}

napi_status napi_create_array_with_length(napi_env env,
                                          size_t length,
                                          napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Array::New(env->isolate, length));

  return napi_clear_last_error(env);
}

napi_status napi_create_string_latin1(napi_env env,
                                      const char* str,
                                      size_t length,
                                      napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);
  RETURN_STATUS_IF_FALSE(env,
      (length == NAPI_AUTO_LENGTH) || length <= INT_MAX,
      napi_invalid_arg);

  auto isolate = env->isolate;
  auto str_maybe =
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>(str),
                                 v8::NewStringType::kNormal,
                                 length);
  CHECK_MAYBE_EMPTY(env, str_maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(str_maybe.ToLocalChecked());
  return napi_clear_last_error(env);
}

napi_status napi_create_string_utf8(napi_env env,
                                    const char* str,
                                    size_t length,
                                    napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);
  RETURN_STATUS_IF_FALSE(env,
      (length == NAPI_AUTO_LENGTH) || length <= INT_MAX,
      napi_invalid_arg);

  auto isolate = env->isolate;
  auto str_maybe =
      v8::String::NewFromUtf8(isolate,
                              str,
                              v8::NewStringType::kNormal,
                              static_cast<int>(length));
  CHECK_MAYBE_EMPTY(env, str_maybe, napi_generic_failure);
  *result = v8impl::JsValueFromV8LocalValue(str_maybe.ToLocalChecked());
  return napi_clear_last_error(env);
}

napi_status napi_create_string_utf16(napi_env env,
                                     const char16_t* str,
                                     size_t length,
                                     napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);
  RETURN_STATUS_IF_FALSE(env,
      (length == NAPI_AUTO_LENGTH) || length <= INT_MAX,
      napi_invalid_arg);

  auto isolate = env->isolate;
  auto str_maybe =
      v8::String::NewFromTwoByte(isolate,
                                 reinterpret_cast<const uint16_t*>(str),
                                 v8::NewStringType::kNormal,
                                 length);
  CHECK_MAYBE_EMPTY(env, str_maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(str_maybe.ToLocalChecked());
  return napi_clear_last_error(env);
}

napi_status napi_create_double(napi_env env,
                               double value,
                               napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Number::New(env->isolate, value));

  return napi_clear_last_error(env);
}

napi_status napi_create_int32(napi_env env,
                              int32_t value,
                              napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Integer::New(env->isolate, value));

  return napi_clear_last_error(env);
}

napi_status napi_create_uint32(napi_env env,
                               uint32_t value,
                               napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Integer::NewFromUnsigned(env->isolate, value));

  return napi_clear_last_error(env);
}

napi_status napi_create_int64(napi_env env,
                              int64_t value,
                              napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Number::New(env->isolate, static_cast<double>(value)));

  return napi_clear_last_error(env);
}

napi_status napi_create_bigint_int64(napi_env env,
                                     int64_t value,
                                     napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::BigInt::New(env->isolate, value));

  return napi_clear_last_error(env);
}

napi_status napi_create_bigint_uint64(napi_env env,
                                      uint64_t value,
                                      napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::BigInt::NewFromUnsigned(env->isolate, value));

  return napi_clear_last_error(env);
}

napi_status napi_create_bigint_words(napi_env env,
                                     int sign_bit,
                                     size_t word_count,
                                     const uint64_t* words,
                                     napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, words);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();

  RETURN_STATUS_IF_FALSE(
      env, word_count <= INT_MAX, napi_invalid_arg);

  v8::MaybeLocal<v8::BigInt> b = v8::BigInt::NewFromWords(
      context, sign_bit, word_count, words);

  if (try_catch.HasCaught()) {
    return napi_set_last_error(env, napi_pending_exception);
  } else {
    CHECK_MAYBE_EMPTY(env, b, napi_generic_failure);
    *result = v8impl::JsValueFromV8LocalValue(b.ToLocalChecked());
    return napi_clear_last_error(env);
  }
}

napi_status napi_get_boolean(napi_env env, bool value, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;

  if (value) {
    *result = v8impl::JsValueFromV8LocalValue(v8::True(isolate));
  } else {
    *result = v8impl::JsValueFromV8LocalValue(v8::False(isolate));
  }

  return napi_clear_last_error(env);
}

napi_status napi_create_symbol(napi_env env,
                               napi_value description,
                               napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;

  if (description == nullptr) {
    *result = v8impl::JsValueFromV8LocalValue(v8::Symbol::New(isolate));
  } else {
    v8::Local<v8::Value> desc = v8impl::V8LocalValueFromJsValue(description);
    RETURN_STATUS_IF_FALSE(env, desc->IsString(), napi_string_expected);

    *result = v8impl::JsValueFromV8LocalValue(
      v8::Symbol::New(isolate, desc.As<v8::String>()));
  }

  return napi_clear_last_error(env);
}

static inline napi_status set_error_code(napi_env env,
                                         v8::Local<v8::Value> error,
                                         napi_value code,
                                         const char* code_cstring) {
  if ((code != nullptr) || (code_cstring != nullptr)) {
    v8::Local<v8::Context> context = env->context();
    v8::Local<v8::Object> err_object = error.As<v8::Object>();

    v8::Local<v8::Value> code_value = v8impl::V8LocalValueFromJsValue(code);
    if (code != nullptr) {
      code_value = v8impl::V8LocalValueFromJsValue(code);
      RETURN_STATUS_IF_FALSE(env, code_value->IsString(), napi_string_expected);
    } else {
      CHECK_NEW_FROM_UTF8(env, code_value, code_cstring);
    }

    v8::Local<v8::Name> code_key;
    CHECK_NEW_FROM_UTF8(env, code_key, "code");

    v8::Maybe<bool> set_maybe = err_object->Set(context, code_key, code_value);
    RETURN_STATUS_IF_FALSE(env,
                           set_maybe.FromMaybe(false),
                           napi_generic_failure);
  }
  return napi_ok;
}

napi_status napi_create_error(napi_env env,
                              napi_value code,
                              napi_value msg,
                              napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, msg);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> message_value = v8impl::V8LocalValueFromJsValue(msg);
  RETURN_STATUS_IF_FALSE(env, message_value->IsString(), napi_string_expected);

  v8::Local<v8::Value> error_obj =
      v8::Exception::Error(message_value.As<v8::String>());
  napi_status status = set_error_code(env, error_obj, code, nullptr);
  if (status != napi_ok) return status;

  *result = v8impl::JsValueFromV8LocalValue(error_obj);

  return napi_clear_last_error(env);
}

napi_status napi_create_type_error(napi_env env,
                                   napi_value code,
                                   napi_value msg,
                                   napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, msg);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> message_value = v8impl::V8LocalValueFromJsValue(msg);
  RETURN_STATUS_IF_FALSE(env, message_value->IsString(), napi_string_expected);

  v8::Local<v8::Value> error_obj =
      v8::Exception::TypeError(message_value.As<v8::String>());
  napi_status status = set_error_code(env, error_obj, code, nullptr);
  if (status != napi_ok) return status;

  *result = v8impl::JsValueFromV8LocalValue(error_obj);

  return napi_clear_last_error(env);
}

napi_status napi_create_range_error(napi_env env,
                                    napi_value code,
                                    napi_value msg,
                                    napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, msg);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> message_value = v8impl::V8LocalValueFromJsValue(msg);
  RETURN_STATUS_IF_FALSE(env, message_value->IsString(), napi_string_expected);

  v8::Local<v8::Value> error_obj =
      v8::Exception::RangeError(message_value.As<v8::String>());
  napi_status status = set_error_code(env, error_obj, code, nullptr);
  if (status != napi_ok) return status;

  *result = v8impl::JsValueFromV8LocalValue(error_obj);

  return napi_clear_last_error(env);
}

napi_status napi_typeof(napi_env env,
                        napi_value value,
                        napi_valuetype* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(value);

  if (v->IsNumber()) {
    *result = napi_number;
  } else if (v->IsBigInt()) {
    *result = napi_bigint;
  } else if (v->IsString()) {
    *result = napi_string;
  } else if (v->IsFunction()) {
    // This test has to come before IsObject because IsFunction
    // implies IsObject
    *result = napi_function;
  } else if (v->IsExternal()) {
    // This test has to come before IsObject because IsExternal
    // implies IsObject
    *result = napi_external;
  } else if (v->IsObject()) {
    *result = napi_object;
  } else if (v->IsBoolean()) {
    *result = napi_boolean;
  } else if (v->IsUndefined()) {
    *result = napi_undefined;
  } else if (v->IsSymbol()) {
    *result = napi_symbol;
  } else if (v->IsNull()) {
    *result = napi_null;
  } else {
    // Should not get here unless V8 has added some new kind of value.
    return napi_set_last_error(env, napi_invalid_arg);
  }

  return napi_clear_last_error(env);
}

napi_status napi_get_undefined(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Undefined(env->isolate));

  return napi_clear_last_error(env);
}

napi_status napi_get_null(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
        v8::Null(env->isolate));

  return napi_clear_last_error(env);
}

// Gets all callback info in a single call. (Ugly, but faster.)
napi_status napi_get_cb_info(
    napi_env env,               // [in] NAPI environment handle
    napi_callback_info cbinfo,  // [in] Opaque callback-info handle
    size_t* argc,      // [in-out] Specifies the size of the provided argv array
                       // and receives the actual count of args.
    napi_value* argv,  // [out] Array of values
    napi_value* this_arg,  // [out] Receives the JS 'this' arg for the call
    void** data) {         // [out] Receives the data pointer for the callback.
  CHECK_ENV(env);
  CHECK_ARG(env, cbinfo);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  if (argv != nullptr) {
    CHECK_ARG(env, argc);
    info->Args(argv, *argc);
  }
  if (argc != nullptr) {
    *argc = info->ArgsLength();
  }
  if (this_arg != nullptr) {
    *this_arg = info->This();
  }
  if (data != nullptr) {
    *data = info->Data();
  }

  return napi_clear_last_error(env);
}

napi_status napi_get_new_target(napi_env env,
                                napi_callback_info cbinfo,
                                napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, cbinfo);
  CHECK_ARG(env, result);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->GetNewTarget();
  return napi_clear_last_error(env);
}

napi_status napi_call_function(napi_env env,
                               napi_value recv,
                               napi_value func,
                               size_t argc,
                               const napi_value* argv,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, recv);
  if (argc > 0) {
    CHECK_ARG(env, argv);
  }

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Value> v8recv = v8impl::V8LocalValueFromJsValue(recv);

  v8::Local<v8::Function> v8func;
  CHECK_TO_FUNCTION(env, v8func, func);

  auto maybe = v8func->Call(context, v8recv, argc,
    reinterpret_cast<v8::Local<v8::Value>*>(const_cast<napi_value*>(argv)));

  if (try_catch.HasCaught()) {
    return napi_set_last_error(env, napi_pending_exception);
  } else {
    if (result != nullptr) {
      CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);
      *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
    }
    return napi_clear_last_error(env);
  }
}

napi_status napi_get_global(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(env->context()->Global());

  return napi_clear_last_error(env);
}

napi_status napi_throw(napi_env env, napi_value error) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, error);

  v8::Isolate* isolate = env->isolate;

  isolate->ThrowException(v8impl::V8LocalValueFromJsValue(error));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_clear_last_error(env);
}

napi_status napi_throw_error(napi_env env,
                             const char* code,
                             const char* msg) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(env, str, msg);

  v8::Local<v8::Value> error_obj = v8::Exception::Error(str);
  napi_status status = set_error_code(env, error_obj, nullptr, code);
  if (status != napi_ok) return status;

  isolate->ThrowException(error_obj);
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_clear_last_error(env);
}

napi_status napi_throw_type_error(napi_env env,
                                  const char* code,
                                  const char* msg) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(env, str, msg);

  v8::Local<v8::Value> error_obj = v8::Exception::TypeError(str);
  napi_status status = set_error_code(env, error_obj, nullptr, code);
  if (status != napi_ok) return status;

  isolate->ThrowException(error_obj);
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_clear_last_error(env);
}

napi_status napi_throw_range_error(napi_env env,
                                   const char* code,
                                   const char* msg) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(env, str, msg);

  v8::Local<v8::Value> error_obj = v8::Exception::RangeError(str);
  napi_status status = set_error_code(env, error_obj, nullptr, code);
  if (status != napi_ok) return status;

  isolate->ThrowException(error_obj);
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_clear_last_error(env);
}

napi_status napi_is_error(napi_env env, napi_value value, bool* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot
  // throw JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsNativeError();

  return napi_clear_last_error(env);
}

napi_status napi_get_value_double(napi_env env,
                                  napi_value value,
                                  double* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsNumber(), napi_number_expected);

  *result = val.As<v8::Number>()->Value();

  return napi_clear_last_error(env);
}

napi_status napi_get_value_int32(napi_env env,
                                 napi_value value,
                                 int32_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  if (val->IsInt32()) {
    *result = val.As<v8::Int32>()->Value();
  } else {
    RETURN_STATUS_IF_FALSE(env, val->IsNumber(), napi_number_expected);

    // Empty context: https://github.com/nodejs/node/issues/14379
    v8::Local<v8::Context> context;
    *result = val->Int32Value(context).FromJust();
  }

  return napi_clear_last_error(env);
}

napi_status napi_get_value_uint32(napi_env env,
                                  napi_value value,
                                  uint32_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  if (val->IsUint32()) {
    *result = val.As<v8::Uint32>()->Value();
  } else {
    RETURN_STATUS_IF_FALSE(env, val->IsNumber(), napi_number_expected);

    // Empty context: https://github.com/nodejs/node/issues/14379
    v8::Local<v8::Context> context;
    *result = val->Uint32Value(context).FromJust();
  }

  return napi_clear_last_error(env);
}

napi_status napi_get_value_int64(napi_env env,
                                 napi_value value,
                                 int64_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  // This is still a fast path very likely to be taken.
  if (val->IsInt32()) {
    *result = val.As<v8::Int32>()->Value();
    return napi_clear_last_error(env);
  }

  RETURN_STATUS_IF_FALSE(env, val->IsNumber(), napi_number_expected);

  // v8::Value::IntegerValue() converts NaN, +Inf, and -Inf to INT64_MIN,
  // inconsistent with v8::Value::Int32Value() which converts those values to 0.
  // Special-case all non-finite values to match that behavior.
  double doubleValue = val.As<v8::Number>()->Value();
  if (std::isfinite(doubleValue)) {
    // Empty context: https://github.com/nodejs/node/issues/14379
    v8::Local<v8::Context> context;
    *result = val->IntegerValue(context).FromJust();
  } else {
    *result = 0;
  }

  return napi_clear_last_error(env);
}

napi_status napi_get_value_bigint_int64(napi_env env,
                                        napi_value value,
                                        int64_t* result,
                                        bool* lossless) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);
  CHECK_ARG(env, lossless);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  RETURN_STATUS_IF_FALSE(env, val->IsBigInt(), napi_bigint_expected);

  *result = val.As<v8::BigInt>()->Int64Value(lossless);

  return napi_clear_last_error(env);
}

napi_status napi_get_value_bigint_uint64(napi_env env,
                                         napi_value value,
                                         uint64_t* result,
                                         bool* lossless) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);
  CHECK_ARG(env, lossless);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  RETURN_STATUS_IF_FALSE(env, val->IsBigInt(), napi_bigint_expected);

  *result = val.As<v8::BigInt>()->Uint64Value(lossless);

  return napi_clear_last_error(env);
}

napi_status napi_get_value_bigint_words(napi_env env,
                                        napi_value value,
                                        int* sign_bit,
                                        size_t* word_count,
                                        uint64_t* words) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, word_count);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  RETURN_STATUS_IF_FALSE(env, val->IsBigInt(), napi_bigint_expected);

  v8::Local<v8::BigInt> big = val.As<v8::BigInt>();

  int word_count_int = *word_count;

  if (sign_bit == nullptr && words == nullptr) {
    word_count_int = big->WordCount();
  } else {
    CHECK_ARG(env, sign_bit);
    CHECK_ARG(env, words);
    big->ToWordsArray(sign_bit, &word_count_int, words);
  }

  *word_count = word_count_int;

  return napi_clear_last_error(env);
}

napi_status napi_get_value_bool(napi_env env, napi_value value, bool* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsBoolean(), napi_boolean_expected);

  *result = val.As<v8::Boolean>()->Value();

  return napi_clear_last_error(env);
}

// Copies a JavaScript string into a LATIN-1 string buffer. The result is the
// number of bytes (excluding the null terminator) copied into buf.
// A sufficient buffer size should be greater than the length of string,
// reserving space for null terminator.
// If bufsize is insufficient, the string will be truncated and null terminated.
// If buf is NULL, this method returns the length of the string (in bytes)
// via the result parameter.
// The result argument is optional unless buf is NULL.
napi_status napi_get_value_string_latin1(napi_env env,
                                         napi_value value,
                                         char* buf,
                                         size_t bufsize,
                                         size_t* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsString(), napi_string_expected);

  if (!buf) {
    CHECK_ARG(env, result);
    *result = val.As<v8::String>()->Length();
  } else {
    int copied =
        val.As<v8::String>()->WriteOneByte(env->isolate,
                                           reinterpret_cast<uint8_t*>(buf),
                                           0,
                                           bufsize - 1,
                                           v8::String::NO_NULL_TERMINATION);

    buf[copied] = '\0';
    if (result != nullptr) {
      *result = copied;
    }
  }

  return napi_clear_last_error(env);
}

// Copies a JavaScript string into a UTF-8 string buffer. The result is the
// number of bytes (excluding the null terminator) copied into buf.
// A sufficient buffer size should be greater than the length of string,
// reserving space for null terminator.
// If bufsize is insufficient, the string will be truncated and null terminated.
// If buf is NULL, this method returns the length of the string (in bytes)
// via the result parameter.
// The result argument is optional unless buf is NULL.
napi_status napi_get_value_string_utf8(napi_env env,
                                       napi_value value,
                                       char* buf,
                                       size_t bufsize,
                                       size_t* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsString(), napi_string_expected);

  if (!buf) {
    CHECK_ARG(env, result);
    *result = val.As<v8::String>()->Utf8Length(env->isolate);
  } else {
    int copied = val.As<v8::String>()->WriteUtf8(
        env->isolate,
        buf,
        bufsize - 1,
        nullptr,
        v8::String::REPLACE_INVALID_UTF8 | v8::String::NO_NULL_TERMINATION);

    buf[copied] = '\0';
    if (result != nullptr) {
      *result = copied;
    }
  }

  return napi_clear_last_error(env);
}

// Copies a JavaScript string into a UTF-16 string buffer. The result is the
// number of 2-byte code units (excluding the null terminator) copied into buf.
// A sufficient buffer size should be greater than the length of string,
// reserving space for null terminator.
// If bufsize is insufficient, the string will be truncated and null terminated.
// If buf is NULL, this method returns the length of the string (in 2-byte
// code units) via the result parameter.
// The result argument is optional unless buf is NULL.
napi_status napi_get_value_string_utf16(napi_env env,
                                        napi_value value,
                                        char16_t* buf,
                                        size_t bufsize,
                                        size_t* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsString(), napi_string_expected);

  if (!buf) {
    CHECK_ARG(env, result);
    // V8 assumes UTF-16 length is the same as the number of characters.
    *result = val.As<v8::String>()->Length();
  } else {
    int copied = val.As<v8::String>()->Write(env->isolate,
                                             reinterpret_cast<uint16_t*>(buf),
                                             0,
                                             bufsize - 1,
                                             v8::String::NO_NULL_TERMINATION);

    buf[copied] = '\0';
    if (result != nullptr) {
      *result = copied;
    }
  }

  return napi_clear_last_error(env);
}

napi_status napi_coerce_to_bool(napi_env env,
                                napi_value value,
                                napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Boolean> b =
    v8impl::V8LocalValueFromJsValue(value)->ToBoolean(isolate);
  *result = v8impl::JsValueFromV8LocalValue(b);
  return GET_RETURN_STATUS(env);
}

#define GEN_COERCE_FUNCTION(UpperCaseName, MixedCaseName, LowerCaseName)      \
  napi_status napi_coerce_to_##LowerCaseName(napi_env env,                    \
                                             napi_value value,                \
                                             napi_value* result) {            \
    NAPI_PREAMBLE(env);                                                       \
    CHECK_ARG(env, value);                                                    \
    CHECK_ARG(env, result);                                                   \
                                                                              \
    v8::Local<v8::Context> context = env->context();                          \
    v8::Local<v8::MixedCaseName> str;                                         \
                                                                              \
    CHECK_TO_##UpperCaseName(env, context, str, value);                       \
                                                                              \
    *result = v8impl::JsValueFromV8LocalValue(str);                           \
    return GET_RETURN_STATUS(env);                                            \
  }

GEN_COERCE_FUNCTION(NUMBER, Number, number)
GEN_COERCE_FUNCTION(OBJECT, Object, object)
GEN_COERCE_FUNCTION(STRING, String, string)

#undef GEN_COERCE_FUNCTION

napi_status napi_wrap(napi_env env,
                      napi_value js_object,
                      void* native_object,
                      napi_finalize finalize_cb,
                      void* finalize_hint,
                      napi_ref* result) {
  return v8impl::Wrap<v8impl::retrievable>(env,
                                           js_object,
                                           native_object,
                                           finalize_cb,
                                           finalize_hint,
                                           result);
}

napi_status napi_unwrap(napi_env env, napi_value obj, void** result) {
  return v8impl::Unwrap(env, obj, result, v8impl::KeepWrap);
}

napi_status napi_remove_wrap(napi_env env, napi_value obj, void** result) {
  return v8impl::Unwrap(env, obj, result, v8impl::RemoveWrap);
}

napi_status napi_create_external(napi_env env,
                                 void* data,
                                 napi_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;

  v8::Local<v8::Value> external_value = v8::External::New(isolate, data);

  // The Reference object will delete itself after invoking the finalizer
  // callback.
  v8impl::Reference::New(env,
      external_value,
      0,
      true,
      finalize_cb,
      data,
      finalize_hint);

  *result = v8impl::JsValueFromV8LocalValue(external_value);

  return napi_clear_last_error(env);
}

napi_status napi_get_value_external(napi_env env,
                                    napi_value value,
                                    void** result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsExternal(), napi_invalid_arg);

  v8::Local<v8::External> external_value = val.As<v8::External>();
  *result = external_value->Value();

  return napi_clear_last_error(env);
}

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
napi_status napi_create_reference(napi_env env,
                                  napi_value value,
                                  uint32_t initial_refcount,
                                  napi_ref* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_value = v8impl::V8LocalValueFromJsValue(value);

  if (!(v8_value->IsObject() || v8_value->IsFunction())) {
    return napi_set_last_error(env, napi_object_expected);
  }

  v8impl::Reference* reference =
      v8impl::Reference::New(env, v8_value, initial_refcount, false);

  *result = reinterpret_cast<napi_ref>(reference);
  return napi_clear_last_error(env);
}

// Deletes a reference. The referenced value is released, and may be GC'd unless
// there are other references to it.
napi_status napi_delete_reference(napi_env env, napi_ref ref) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);

  v8impl::Reference::Delete(reinterpret_cast<v8impl::Reference*>(ref));

  return napi_clear_last_error(env);
}

// Increments the reference count, optionally returning the resulting count.
// After this call the reference will be a strong reference because its
// refcount is >0, and the referenced object is effectively "pinned".
// Calling this when the refcount is 0 and the object is unavailable
// results in an error.
napi_status napi_reference_ref(napi_env env, napi_ref ref, uint32_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  uint32_t count = reference->Ref();

  if (result != nullptr) {
    *result = count;
  }

  return napi_clear_last_error(env);
}

// Decrements the reference count, optionally returning the resulting count. If
// the result is 0 the reference is now weak and the object may be GC'd at any
// time if there are no other references. Calling this when the refcount is
// already 0 results in an error.
napi_status napi_reference_unref(napi_env env, napi_ref ref, uint32_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);

  if (reference->RefCount() == 0) {
    return napi_set_last_error(env, napi_generic_failure);
  }

  uint32_t count = reference->Unref();

  if (result != nullptr) {
    *result = count;
  }

  return napi_clear_last_error(env);
}

// Attempts to get a referenced value. If the reference is weak, the value might
// no longer be available, in that case the call is still successful but the
// result is NULL.
napi_status napi_get_reference_value(napi_env env,
                                     napi_ref ref,
                                     napi_value* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);
  CHECK_ARG(env, result);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  *result = v8impl::JsValueFromV8LocalValue(reference->Get());

  return napi_clear_last_error(env);
}

napi_status napi_open_handle_scope(napi_env env, napi_handle_scope* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsHandleScopeFromV8HandleScope(
      new v8impl::HandleScopeWrapper(env->isolate));
  env->open_handle_scopes++;
  return napi_clear_last_error(env);
}

napi_status napi_close_handle_scope(napi_env env, napi_handle_scope scope) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, scope);
  if (env->open_handle_scopes == 0) {
    return napi_handle_scope_mismatch;
  }

  env->open_handle_scopes--;
  delete v8impl::V8HandleScopeFromJsHandleScope(scope);
  return napi_clear_last_error(env);
}

napi_status napi_open_escapable_handle_scope(
    napi_env env,
    napi_escapable_handle_scope* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsEscapableHandleScopeFromV8EscapableHandleScope(
      new v8impl::EscapableHandleScopeWrapper(env->isolate));
  env->open_handle_scopes++;
  return napi_clear_last_error(env);
}

napi_status napi_close_escapable_handle_scope(
    napi_env env,
    napi_escapable_handle_scope scope) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, scope);
  if (env->open_handle_scopes == 0) {
    return napi_handle_scope_mismatch;
  }

  delete v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  env->open_handle_scopes--;
  return napi_clear_last_error(env);
}

napi_status napi_escape_handle(napi_env env,
                               napi_escapable_handle_scope scope,
                               napi_value escapee,
                               napi_value* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, scope);
  CHECK_ARG(env, escapee);
  CHECK_ARG(env, result);

  v8impl::EscapableHandleScopeWrapper* s =
      v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  if (!s->escape_called()) {
    *result = v8impl::JsValueFromV8LocalValue(
        s->Escape(v8impl::V8LocalValueFromJsValue(escapee)));
    return napi_clear_last_error(env);
  }
  return napi_set_last_error(env, napi_escape_called_twice);
}

napi_status napi_new_instance(napi_env env,
                              napi_value constructor,
                              size_t argc,
                              const napi_value* argv,
                              napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, constructor);
  if (argc > 0) {
    CHECK_ARG(env, argv);
  }
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Function> ctor;
  CHECK_TO_FUNCTION(env, ctor, constructor);

  auto maybe = ctor->NewInstance(context, argc,
    reinterpret_cast<v8::Local<v8::Value>*>(const_cast<napi_value*>(argv)));

  CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_instanceof(napi_env env,
                            napi_value object,
                            napi_value constructor,
                            bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, object);
  CHECK_ARG(env, result);

  *result = false;

  v8::Local<v8::Object> ctor;
  v8::Local<v8::Context> context = env->context();

  CHECK_TO_OBJECT(env, context, ctor, constructor);

  if (!ctor->IsFunction()) {
    napi_throw_type_error(env,
                          "ERR_NAPI_CONS_FUNCTION",
                          "Constructor must be a function");

    return napi_set_last_error(env, napi_function_expected);
  }

  napi_status status = napi_generic_failure;

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(object);
  auto maybe_result = val->InstanceOf(context, ctor);
  CHECK_MAYBE_NOTHING(env, maybe_result, status);
  *result = maybe_result.FromJust();
  return GET_RETURN_STATUS(env);
}

// Methods to support catching exceptions
napi_status napi_is_exception_pending(napi_env env, bool* result) {
  // NAPI_PREAMBLE is not used here: this function must execute when there is a
  // pending exception.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = !env->last_exception.IsEmpty();
  return napi_clear_last_error(env);
}

napi_status napi_get_and_clear_last_exception(napi_env env,
                                              napi_value* result) {
  // NAPI_PREAMBLE is not used here: this function must execute when there is a
  // pending exception.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  if (env->last_exception.IsEmpty()) {
    return napi_get_undefined(env, result);
  } else {
    *result = v8impl::JsValueFromV8LocalValue(
      v8::Local<v8::Value>::New(env->isolate, env->last_exception));
    env->last_exception.Reset();
  }

  return napi_clear_last_error(env);
}

napi_status napi_is_arraybuffer(napi_env env, napi_value value, bool* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsArrayBuffer();

  return napi_clear_last_error(env);
}

napi_status napi_create_arraybuffer(napi_env env,
                                    size_t byte_length,
                                    void** data,
                                    napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, byte_length);

  // Optionally return a pointer to the buffer's data, to avoid another call to
  // retrieve it.
  if (data != nullptr) {
    *data = buffer->GetBackingStore()->Data();
  }

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  return GET_RETURN_STATUS(env);
}

napi_status napi_create_external_arraybuffer(napi_env env,
                                             void* external_data,
                                             size_t byte_length,
                                             napi_finalize finalize_cb,
                                             void* finalize_hint,
                                             napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  // The buffer will be freed with v8impl::ArrayBufferReference::New()
  // below, hence this BackingStore does not need to free the buffer.
  std::unique_ptr<v8::BackingStore> backing =
      v8::ArrayBuffer::NewBackingStore(external_data,
                                       byte_length,
                                       [](void*, size_t, void*){},
                                       nullptr);
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, std::move(backing));
  v8::Maybe<bool> marked = env->mark_arraybuffer_as_untransferable(buffer);
  CHECK_MAYBE_NOTHING(env, marked, napi_generic_failure);

  if (finalize_cb != nullptr) {
    // Create a self-deleting weak reference that invokes the finalizer
    // callback and detaches the ArrayBuffer if it still exists on Environment
    // teardown.
    v8impl::ArrayBufferReference::New(env,
        buffer,
        0,
        true,
        finalize_cb,
        external_data,
        finalize_hint);
  }

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_arraybuffer_info(napi_env env,
                                      napi_value arraybuffer,
                                      void** data,
                                      size_t* byte_length) {
  CHECK_ENV(env);
  CHECK_ARG(env, arraybuffer);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(env, value->IsArrayBuffer(), napi_invalid_arg);

  std::shared_ptr<v8::BackingStore> backing_store =
      value.As<v8::ArrayBuffer>()->GetBackingStore();

  if (data != nullptr) {
    *data = backing_store->Data();
  }

  if (byte_length != nullptr) {
    *byte_length = backing_store->ByteLength();
  }

  return napi_clear_last_error(env);
}

napi_status napi_is_typedarray(napi_env env, napi_value value, bool* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsTypedArray();

  return napi_clear_last_error(env);
}

napi_status napi_create_typedarray(napi_env env,
                                   napi_typedarray_type type,
                                   size_t length,
                                   napi_value arraybuffer,
                                   size_t byte_offset,
                                   napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, arraybuffer);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(env, value->IsArrayBuffer(), napi_invalid_arg);

  v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
  v8::Local<v8::TypedArray> typedArray;

  switch (type) {
    case napi_int8_array:
      CREATE_TYPED_ARRAY(
          env, Int8Array, 1, buffer, byte_offset, length, typedArray);
      break;
    case napi_uint8_array:
      CREATE_TYPED_ARRAY(
          env, Uint8Array, 1, buffer, byte_offset, length, typedArray);
      break;
    case napi_uint8_clamped_array:
      CREATE_TYPED_ARRAY(
          env, Uint8ClampedArray, 1, buffer, byte_offset, length, typedArray);
      break;
    case napi_int16_array:
      CREATE_TYPED_ARRAY(
          env, Int16Array, 2, buffer, byte_offset, length, typedArray);
      break;
    case napi_uint16_array:
      CREATE_TYPED_ARRAY(
          env, Uint16Array, 2, buffer, byte_offset, length, typedArray);
      break;
    case napi_int32_array:
      CREATE_TYPED_ARRAY(
          env, Int32Array, 4, buffer, byte_offset, length, typedArray);
      break;
    case napi_uint32_array:
      CREATE_TYPED_ARRAY(
          env, Uint32Array, 4, buffer, byte_offset, length, typedArray);
      break;
    case napi_float32_array:
      CREATE_TYPED_ARRAY(
          env, Float32Array, 4, buffer, byte_offset, length, typedArray);
      break;
    case napi_float64_array:
      CREATE_TYPED_ARRAY(
          env, Float64Array, 8, buffer, byte_offset, length, typedArray);
      break;
    case napi_bigint64_array:
      CREATE_TYPED_ARRAY(
          env, BigInt64Array, 8, buffer, byte_offset, length, typedArray);
      break;
    case napi_biguint64_array:
      CREATE_TYPED_ARRAY(
          env, BigUint64Array, 8, buffer, byte_offset, length, typedArray);
      break;
    default:
      return napi_set_last_error(env, napi_invalid_arg);
  }

  *result = v8impl::JsValueFromV8LocalValue(typedArray);
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_typedarray_info(napi_env env,
                                     napi_value typedarray,
                                     napi_typedarray_type* type,
                                     size_t* length,
                                     void** data,
                                     napi_value* arraybuffer,
                                     size_t* byte_offset) {
  CHECK_ENV(env);
  CHECK_ARG(env, typedarray);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(typedarray);
  RETURN_STATUS_IF_FALSE(env, value->IsTypedArray(), napi_invalid_arg);

  v8::Local<v8::TypedArray> array = value.As<v8::TypedArray>();

  if (type != nullptr) {
    if (value->IsInt8Array()) {
      *type = napi_int8_array;
    } else if (value->IsUint8Array()) {
      *type = napi_uint8_array;
    } else if (value->IsUint8ClampedArray()) {
      *type = napi_uint8_clamped_array;
    } else if (value->IsInt16Array()) {
      *type = napi_int16_array;
    } else if (value->IsUint16Array()) {
      *type = napi_uint16_array;
    } else if (value->IsInt32Array()) {
      *type = napi_int32_array;
    } else if (value->IsUint32Array()) {
      *type = napi_uint32_array;
    } else if (value->IsFloat32Array()) {
      *type = napi_float32_array;
    } else if (value->IsFloat64Array()) {
      *type = napi_float64_array;
    } else if (value->IsBigInt64Array()) {
      *type = napi_bigint64_array;
    } else if (value->IsBigUint64Array()) {
      *type = napi_biguint64_array;
    }
  }

  if (length != nullptr) {
    *length = array->Length();
  }

  v8::Local<v8::ArrayBuffer> buffer;
  if (data != nullptr || arraybuffer != nullptr) {
    // Calling Buffer() may have the side effect of allocating the buffer,
    // so only do this when it’s needed.
    buffer = array->Buffer();
  }

  if (data != nullptr) {
    *data = static_cast<uint8_t*>(buffer->GetBackingStore()->Data()) +
            array->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = v8impl::JsValueFromV8LocalValue(buffer);
  }

  if (byte_offset != nullptr) {
    *byte_offset = array->ByteOffset();
  }

  return napi_clear_last_error(env);
}

napi_status napi_create_dataview(napi_env env,
                                 size_t byte_length,
                                 napi_value arraybuffer,
                                 size_t byte_offset,
                                 napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, arraybuffer);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(env, value->IsArrayBuffer(), napi_invalid_arg);

  v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
  if (byte_length + byte_offset > buffer->ByteLength()) {
    napi_throw_range_error(
        env,
        "ERR_NAPI_INVALID_DATAVIEW_ARGS",
        "byte_offset + byte_length should be less than or "
        "equal to the size in bytes of the array passed in");
    return napi_set_last_error(env, napi_pending_exception);
  }
  v8::Local<v8::DataView> DataView = v8::DataView::New(buffer, byte_offset,
                                                       byte_length);

  *result = v8impl::JsValueFromV8LocalValue(DataView);
  return GET_RETURN_STATUS(env);
}

napi_status napi_is_dataview(napi_env env, napi_value value, bool* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsDataView();

  return napi_clear_last_error(env);
}

napi_status napi_get_dataview_info(napi_env env,
                                   napi_value dataview,
                                   size_t* byte_length,
                                   void** data,
                                   napi_value* arraybuffer,
                                   size_t* byte_offset) {
  CHECK_ENV(env);
  CHECK_ARG(env, dataview);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(dataview);
  RETURN_STATUS_IF_FALSE(env, value->IsDataView(), napi_invalid_arg);

  v8::Local<v8::DataView> array = value.As<v8::DataView>();

  if (byte_length != nullptr) {
    *byte_length = array->ByteLength();
  }

  v8::Local<v8::ArrayBuffer> buffer;
  if (data != nullptr || arraybuffer != nullptr) {
    // Calling Buffer() may have the side effect of allocating the buffer,
    // so only do this when it’s needed.
    buffer = array->Buffer();
  }

  if (data != nullptr) {
    *data = static_cast<uint8_t*>(buffer->GetBackingStore()->Data()) +
            array->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = v8impl::JsValueFromV8LocalValue(buffer);
  }

  if (byte_offset != nullptr) {
    *byte_offset = array->ByteOffset();
  }

  return napi_clear_last_error(env);
}

napi_status napi_get_version(napi_env env, uint32_t* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);
  *result = NAPI_VERSION;
  return napi_clear_last_error(env);
}

napi_status napi_create_promise(napi_env env,
                                napi_deferred* deferred,
                                napi_value* promise) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, deferred);
  CHECK_ARG(env, promise);

  auto maybe = v8::Promise::Resolver::New(env->context());
  CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);

  auto v8_resolver = maybe.ToLocalChecked();
  auto v8_deferred = new v8impl::Persistent<v8::Value>();
  v8_deferred->Reset(env->isolate, v8_resolver);

  *deferred = v8impl::JsDeferredFromNodePersistent(v8_deferred);
  *promise = v8impl::JsValueFromV8LocalValue(v8_resolver->GetPromise());
  return GET_RETURN_STATUS(env);
}

napi_status napi_resolve_deferred(napi_env env,
                                  napi_deferred deferred,
                                  napi_value resolution) {
  return v8impl::ConcludeDeferred(env, deferred, resolution, true);
}

napi_status napi_reject_deferred(napi_env env,
                                 napi_deferred deferred,
                                 napi_value resolution) {
  return v8impl::ConcludeDeferred(env, deferred, resolution, false);
}

napi_status napi_is_promise(napi_env env,
                            napi_value value,
                            bool* is_promise) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, is_promise);

  *is_promise = v8impl::V8LocalValueFromJsValue(value)->IsPromise();

  return napi_clear_last_error(env);
}

napi_status napi_create_date(napi_env env,
                             double time,
                             napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::MaybeLocal<v8::Value> maybe_date = v8::Date::New(env->context(), time);
  CHECK_MAYBE_EMPTY(env, maybe_date, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe_date.ToLocalChecked());

  return GET_RETURN_STATUS(env);
}

napi_status napi_is_date(napi_env env,
                         napi_value value,
                         bool* is_date) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, is_date);

  *is_date = v8impl::V8LocalValueFromJsValue(value)->IsDate();

  return napi_clear_last_error(env);
}

napi_status napi_get_date_value(napi_env env,
                                napi_value value,
                                double* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsDate(), napi_date_expected);

  v8::Local<v8::Date> date = val.As<v8::Date>();
  *result = date->ValueOf();

  return GET_RETURN_STATUS(env);
}

napi_status napi_run_script(napi_env env,
                            napi_value script,
                            napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, script);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v8_script = v8impl::V8LocalValueFromJsValue(script);

  if (!v8_script->IsString()) {
    return napi_set_last_error(env, napi_string_expected);
  }

  v8::Local<v8::Context> context = env->context();

  auto maybe_script = v8::Script::Compile(context,
      v8::Local<v8::String>::Cast(v8_script));
  CHECK_MAYBE_EMPTY(env, maybe_script, napi_generic_failure);

  auto script_result =
      maybe_script.ToLocalChecked()->Run(context);
  CHECK_MAYBE_EMPTY(env, script_result, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(script_result.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_add_finalizer(napi_env env,
                               napi_value js_object,
                               void* native_object,
                               napi_finalize finalize_cb,
                               void* finalize_hint,
                               napi_ref* result) {
  return v8impl::Wrap<v8impl::anonymous>(env,
                                         js_object,
                                         native_object,
                                         finalize_cb,
                                         finalize_hint,
                                         result);
}

napi_status napi_adjust_external_memory(napi_env env,
                                        int64_t change_in_bytes,
                                        int64_t* adjusted_value) {
  CHECK_ENV(env);
  CHECK_ARG(env, adjusted_value);

  *adjusted_value = env->isolate->AdjustAmountOfExternalAllocatedMemory(
      change_in_bytes);

  return napi_clear_last_error(env);
}

napi_status napi_set_instance_data(napi_env env,
                                   void* data,
                                   napi_finalize finalize_cb,
                                   void* finalize_hint) {
  CHECK_ENV(env);

  v8impl::RefBase* old_data = static_cast<v8impl::RefBase*>(env->instance_data);
  if (old_data != nullptr) {
    // Our contract so far has been to not finalize any old data there may be.
    // So we simply delete it.
    v8impl::RefBase::Delete(old_data);
  }

  env->instance_data = v8impl::RefBase::New(env,
                                            0,
                                            true,
                                            finalize_cb,
                                            data,
                                            finalize_hint);

  return napi_clear_last_error(env);
}

napi_status napi_get_instance_data(napi_env env,
                                   void** data) {
  CHECK_ENV(env);
  CHECK_ARG(env, data);

  v8impl::RefBase* idata = static_cast<v8impl::RefBase*>(env->instance_data);

  *data = (idata == nullptr ? nullptr : idata->Data());

  return napi_clear_last_error(env);
}

napi_status napi_detach_arraybuffer(napi_env env, napi_value arraybuffer) {
  CHECK_ENV(env);
  CHECK_ARG(env, arraybuffer);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(
      env, value->IsArrayBuffer(), napi_arraybuffer_expected);

  v8::Local<v8::ArrayBuffer> it = value.As<v8::ArrayBuffer>();
  RETURN_STATUS_IF_FALSE(
      env, it->IsDetachable(), napi_detachable_arraybuffer_expected);

  it->Detach();

  return napi_clear_last_error(env);
}

napi_status napi_is_detached_arraybuffer(napi_env env,
                                         napi_value arraybuffer,
                                         bool* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, arraybuffer);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);

  *result = value->IsArrayBuffer() &&
            value.As<v8::ArrayBuffer>()->GetBackingStore()->Data() == nullptr;

  return napi_clear_last_error(env);
}
