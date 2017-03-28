/******************************************************************************
 * Experimental prototype for demonstrating VM agnostic and ABI stable API
 * for native modules to use instead of using Nan and V8 APIs directly.
 *
 * The current status is "Experimental" and should not be used for
 * production applications.  The API is still subject to change
 * and as an experimental feature is NOT subject to semver.
 *
 ******************************************************************************/

#include <node_buffer.h>
#include <node_object_wrap.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "node_api.h"
#include "env-inl.h"
#include "node_api_backport.h"

static
napi_status napi_set_last_error(napi_env env, napi_status error_code,
                                uint32_t engine_error_code = 0,
                                void* engine_reserved = nullptr);
void napi_clear_last_error(napi_env env);

class napi_env__ {
 public:
  explicit napi_env__(v8::Isolate* _isolate): isolate(_isolate),
      has_instance_available(true), last_error() {}
  ~napi_env__() {
    last_exception.Reset();
    has_instance.Reset();
  }
  v8::Isolate* isolate;
  v8::Persistent<v8::Value> last_exception;
  v8::Persistent<v8::Value> has_instance;
  bool has_instance_available;
  napi_extended_error_info last_error;
};

#define RETURN_STATUS_IF_FALSE(env, condition, status)                  \
  do {                                                                  \
    if (!(condition)) {                                                 \
      return napi_set_last_error((env), (status));                      \
    }                                                                   \
  } while (0)

#define CHECK_ENV(env)                                               \
  if ((env) == nullptr) {                                            \
    node::FatalError(__func__, "environment(env) must not be null"); \
  }

#define CHECK_ARG(env, arg) \
  RETURN_STATUS_IF_FALSE((env), ((arg) != nullptr), napi_invalid_arg)

#define CHECK_MAYBE_EMPTY(env, maybe, status) \
  RETURN_STATUS_IF_FALSE((env), !((maybe).IsEmpty()), (status))

#define CHECK_MAYBE_NOTHING(env, maybe, status) \
  RETURN_STATUS_IF_FALSE((env), !((maybe).IsNothing()), (status))

// NAPI_PREAMBLE is not wrapped in do..while: try_catch must have function scope
#define NAPI_PREAMBLE(env)                                       \
  CHECK_ENV((env));                                              \
  RETURN_STATUS_IF_FALSE((env), (env)->last_exception.IsEmpty(), \
                         napi_pending_exception);                \
  napi_clear_last_error((env));                                  \
  v8impl::TryCatch try_catch((env))

#define CHECK_TO_TYPE(env, type, context, result, src, status)                \
  do {                                                                        \
    auto maybe = v8impl::V8LocalValueFromJsValue((src))->To##type((context)); \
    CHECK_MAYBE_EMPTY((env), maybe, (status));                                \
    (result) = maybe.ToLocalChecked();                                        \
  } while (0)

#define CHECK_TO_OBJECT(env, context, result, src) \
  CHECK_TO_TYPE((env), Object, (context), (result), (src), napi_object_expected)

#define CHECK_TO_STRING(env, context, result, src) \
  CHECK_TO_TYPE((env), String, (context), (result), (src), napi_string_expected)

#define CHECK_TO_NUMBER(env, context, result, src) \
  CHECK_TO_TYPE((env), Number, (context), (result), (src), napi_number_expected)

#define CHECK_TO_BOOL(env, context, result, src)            \
  CHECK_TO_TYPE((env), Boolean, (context), (result), (src), \
    napi_boolean_expected)

#define CHECK_NEW_FROM_UTF8_LEN(env, result, str, len)                   \
  do {                                                                   \
    auto str_maybe = v8::String::NewFromUtf8(                            \
        (env)->isolate, (str), v8::NewStringType::kInternalized, (len)); \
    CHECK_MAYBE_EMPTY((env), str_maybe, napi_generic_failure);           \
    (result) = str_maybe.ToLocalChecked();                               \
  } while (0)

#define CHECK_NEW_FROM_UTF8(env, result, str) \
  CHECK_NEW_FROM_UTF8_LEN((env), (result), (str), -1)

#define GET_RETURN_STATUS(env)      \
  (!try_catch.HasCaught() ? napi_ok \
                         : napi_set_last_error((env), napi_pending_exception))

namespace v8impl {

// convert from n-api property attributes to v8::PropertyAttribute
static inline v8::PropertyAttribute V8PropertyAttributesFromDescriptor(
    const napi_property_descriptor* descriptor) {
  unsigned int attribute_flags = v8::PropertyAttribute::None;

  if (descriptor->getter != nullptr || descriptor->setter != nullptr) {
    // The napi_writable attribute is ignored for accessor descriptors, but
    // V8 requires the ReadOnly attribute to match nonexistence of a setter.
    attribute_flags |= (descriptor->setter == nullptr ?
      v8::PropertyAttribute::ReadOnly : v8::PropertyAttribute::None);
  } else if ((descriptor->attributes & napi_writable) == 0) {
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

v8::Isolate* V8IsolateFromJsEnv(napi_env e) {
  return reinterpret_cast<v8::Isolate*>(e);
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
  explicit EscapableHandleScopeWrapper(v8::Isolate* isolate) : scope(isolate) {}
  template <typename T>
  v8::Local<T> Escape(v8::Local<T> handle) {
    return scope.Escape(handle);
  }

 private:
  v8::EscapableHandleScope scope;
};

napi_handle_scope JsHandleScopeFromV8HandleScope(HandleScopeWrapper* s) {
  return reinterpret_cast<napi_handle_scope>(s);
}

HandleScopeWrapper* V8HandleScopeFromJsHandleScope(napi_handle_scope s) {
  return reinterpret_cast<HandleScopeWrapper*>(s);
}

napi_escapable_handle_scope JsEscapableHandleScopeFromV8EscapableHandleScope(
    EscapableHandleScopeWrapper* s) {
  return reinterpret_cast<napi_escapable_handle_scope>(s);
}

EscapableHandleScopeWrapper*
V8EscapableHandleScopeFromJsEscapableHandleScope(
    napi_escapable_handle_scope s) {
  return reinterpret_cast<EscapableHandleScopeWrapper*>(s);
}

//=== Conversion between V8 Handles and napi_value ========================

// This asserts v8::Local<> will always be implemented with a single
// pointer field so that we can pass it around as a void*.
static_assert(sizeof(v8::Local<v8::Value>) == sizeof(napi_value),
  "Cannot convert between v8::Local<v8::Value> and napi_value");

napi_value JsValueFromV8LocalValue(v8::Local<v8::Value> local) {
  return reinterpret_cast<napi_value>(*local);
}

v8::Local<v8::Value> V8LocalValueFromJsValue(napi_value v) {
  v8::Local<v8::Value> local;
  memcpy(&local, &v, sizeof(v));
  return local;
}

static inline napi_status V8NameFromPropertyDescriptor(napi_env env,
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

// Adapter for napi_finalize callbacks.
class Finalizer {
 protected:
  Finalizer(napi_env env,
             napi_finalize finalize_callback,
             void* finalize_data,
             void* finalize_hint)
    : _env(env),
      _finalize_callback(finalize_callback),
      _finalize_data(finalize_data),
      _finalize_hint(finalize_hint) {
  }

  ~Finalizer() {
  }

 public:
  static Finalizer* New(napi_env env,
                        napi_finalize finalize_callback = nullptr,
                        void* finalize_data = nullptr,
                        void* finalize_hint = nullptr) {
    return new Finalizer(
      env, finalize_callback, finalize_data, finalize_hint);
  }

  static void Delete(Finalizer* finalizer) {
    delete finalizer;
  }

  // node::Buffer::FreeCallback
  static void FinalizeBufferCallback(char* data, void* hint) {
    Finalizer* finalizer = static_cast<Finalizer*>(hint);
    if (finalizer->_finalize_callback != nullptr) {
      finalizer->_finalize_callback(
        finalizer->_env,
        data,
        finalizer->_finalize_hint);
    }

    Delete(finalizer);
  }

 protected:
  napi_env _env;
  napi_finalize _finalize_callback;
  void* _finalize_data;
  void* _finalize_hint;
};

// Wrapper around v8::Persistent that implements reference counting.
class Reference : private Finalizer {
 private:
  Reference(napi_env env,
            v8::Local<v8::Value> value,
            uint32_t initial_refcount,
            bool delete_self,
            napi_finalize finalize_callback,
            void* finalize_data,
            void* finalize_hint)
       : Finalizer(env, finalize_callback, finalize_data, finalize_hint),
        _persistent(env->isolate, value),
        _refcount(initial_refcount),
        _delete_self(delete_self) {
    if (initial_refcount == 0) {
      _persistent.SetWeak(
          this, FinalizeCallback, v8::WeakCallbackType::kParameter);
      _persistent.MarkIndependent();
    }
  }

  ~Reference() {
    // The V8 Persistent class currently does not reset in its destructor:
    // see NonCopyablePersistentTraits::kResetInDestructor = false.
    // (Comments there claim that might change in the future.)
    // To avoid memory leaks, it is better to reset at this time, however
    // care must be taken to avoid attempting this after the Isolate has
    // shut down, for example via a static (atexit) destructor.
    _persistent.Reset();
  }

 public:
  static Reference* New(napi_env env,
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

  static void Delete(Reference* reference) {
    delete reference;
  }

  uint32_t Ref() {
    if (++_refcount == 1) {
      _persistent.ClearWeak();
    }

    return _refcount;
  }

  uint32_t Unref() {
    if (_refcount == 0) {
        return 0;
    }
    if (--_refcount == 0) {
      _persistent.SetWeak(
          this, FinalizeCallback, v8::WeakCallbackType::kParameter);
      _persistent.MarkIndependent();
    }

    return _refcount;
  }

  uint32_t RefCount() {
    return _refcount;
  }

  v8::Local<v8::Value> Get() {
    if (_persistent.IsEmpty()) {
      return v8::Local<v8::Value>();
    } else {
      return v8::Local<v8::Value>::New(_env->isolate, _persistent);
    }
  }

 private:
  static void FinalizeCallback(const v8::WeakCallbackInfo<Reference>& data) {
    Reference* reference = data.GetParameter();
    reference->_persistent.Reset();

    // Check before calling the finalize callback, because the callback might
    // delete it.
    bool delete_self = reference->_delete_self;

    if (reference->_finalize_callback != nullptr) {
      reference->_finalize_callback(
          reference->_env,
          reference->_finalize_data,
          reference->_finalize_hint);
    }

    if (delete_self) {
      Delete(reference);
    }
  }

  v8::Persistent<v8::Value> _persistent;
  uint32_t _refcount;
  bool _delete_self;
};

class TryCatch : public v8::TryCatch {
 public:
  explicit TryCatch(napi_env env)
      : v8::TryCatch(env->isolate), _env(env) {}

  ~TryCatch() {
    if (HasCaught()) {
      _env->last_exception.Reset(_env->isolate, Exception());
    }
  }

 private:
  napi_env _env;
};

//=== Function napi_callback wrapper =================================

static const int kDataIndex = 0;
static const int kEnvIndex = 1;

static const int kFunctionIndex = 2;
static const int kFunctionFieldCount = 3;

static const int kGetterIndex = 2;
static const int kSetterIndex = 3;
static const int kAccessorFieldCount = 4;

// Base class extended by classes that wrap V8 function and property callback
// info.
class CallbackWrapper {
 public:
  CallbackWrapper(napi_value this_arg, size_t args_length, void* data)
      : _this(this_arg), _args_length(args_length), _data(data) {}

  virtual bool IsConstructCall() = 0;
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

template <typename Info, int kInternalFieldIndex>
class CallbackWrapperBase : public CallbackWrapper {
 public:
  CallbackWrapperBase(const Info& cbinfo, const size_t args_length)
      : CallbackWrapper(JsValueFromV8LocalValue(cbinfo.This()),
                        args_length,
                        nullptr),
        _cbinfo(cbinfo),
        _cbdata(v8::Local<v8::Object>::Cast(cbinfo.Data())) {
    _data = v8::Local<v8::External>::Cast(_cbdata->GetInternalField(kDataIndex))
                ->Value();
  }

  /*virtual*/
  bool IsConstructCall() override { return false; }

 protected:
  void InvokeCallback() {
    napi_callback_info cbinfo_wrapper = reinterpret_cast<napi_callback_info>(
        static_cast<CallbackWrapper*>(this));
    napi_callback cb = reinterpret_cast<napi_callback>(
        v8::Local<v8::External>::Cast(
            _cbdata->GetInternalField(kInternalFieldIndex))->Value());
    v8::Isolate* isolate = _cbinfo.GetIsolate();

    napi_env env = static_cast<napi_env>(
        v8::Local<v8::External>::Cast(
            _cbdata->GetInternalField(kEnvIndex))->Value());

    // Make sure any errors encountered last time we were in N-API are gone.
    napi_clear_last_error(env);

    napi_value result = cb(env, cbinfo_wrapper);

    if (result != nullptr) {
      this->SetReturnValue(result);
    }

    if (!env->last_exception.IsEmpty()) {
      isolate->ThrowException(
          v8::Local<v8::Value>::New(isolate, env->last_exception));
      env->last_exception.Reset();
    }
  }

  const Info& _cbinfo;
  const v8::Local<v8::Object> _cbdata;
};

class FunctionCallbackWrapper
    : public CallbackWrapperBase<v8::FunctionCallbackInfo<v8::Value>,
                                 kFunctionIndex> {
 public:
  static void Invoke(const v8::FunctionCallbackInfo<v8::Value>& info) {
    FunctionCallbackWrapper cbwrapper(info);
    cbwrapper.InvokeCallback();
  }

  explicit FunctionCallbackWrapper(
      const v8::FunctionCallbackInfo<v8::Value>& cbinfo)
      : CallbackWrapperBase(cbinfo, cbinfo.Length()) {}

  /*virtual*/
  bool IsConstructCall() override { return _cbinfo.IsConstructCall(); }

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
                                 kGetterIndex> {
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
    : public CallbackWrapperBase<v8::PropertyCallbackInfo<void>, kSetterIndex> {
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
    node::FatalError("napi_set_return_value",
      "Cannot return a value from a setter callback.");
  }

 private:
  const v8::Local<v8::Value>& _value;
};

// Creates an object to be made available to the static function callback
// wrapper, used to retrieve the native callback function and data pointer.
v8::Local<v8::Object> CreateFunctionCallbackData(napi_env env,
                                                 napi_callback cb,
                                                 void* data) {
  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::ObjectTemplate> otpl = v8::ObjectTemplate::New(isolate);
  otpl->SetInternalFieldCount(v8impl::kFunctionFieldCount);
  v8::Local<v8::Object> cbdata = otpl->NewInstance(context).ToLocalChecked();

  cbdata->SetInternalField(
      v8impl::kEnvIndex,
      v8::External::New(isolate, static_cast<void*>(env)));
  cbdata->SetInternalField(
      v8impl::kFunctionIndex,
      v8::External::New(isolate, reinterpret_cast<void*>(cb)));
  cbdata->SetInternalField(
      v8impl::kDataIndex,
      v8::External::New(isolate, data));
  return cbdata;
}

// Creates an object to be made available to the static getter/setter
// callback wrapper, used to retrieve the native getter/setter callback
// function and data pointer.
v8::Local<v8::Object> CreateAccessorCallbackData(napi_env env,
                                                 napi_callback getter,
                                                 napi_callback setter,
                                                 void* data) {
  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::ObjectTemplate> otpl = v8::ObjectTemplate::New(isolate);
  otpl->SetInternalFieldCount(v8impl::kAccessorFieldCount);
  v8::Local<v8::Object> cbdata = otpl->NewInstance(context).ToLocalChecked();

  cbdata->SetInternalField(
      v8impl::kEnvIndex,
      v8::External::New(isolate, static_cast<void*>(env)));

  if (getter != nullptr) {
    cbdata->SetInternalField(
        v8impl::kGetterIndex,
        v8::External::New(isolate, reinterpret_cast<void*>(getter)));
  }

  if (setter != nullptr) {
    cbdata->SetInternalField(
        v8impl::kSetterIndex,
        v8::External::New(isolate, reinterpret_cast<void*>(setter)));
  }

  cbdata->SetInternalField(
      v8impl::kDataIndex,
      v8::External::New(isolate, data));
  return cbdata;
}

}  // end of namespace v8impl

// Intercepts the Node-V8 module registration callback. Converts parameters
// to NAPI equivalents and then calls the registration callback specified
// by the NAPI module.
void napi_module_register_cb(v8::Local<v8::Object> exports,
                             v8::Local<v8::Value> module,
                             v8::Local<v8::Context> context,
                             void* priv) {
  napi_module* mod = static_cast<napi_module*>(priv);

  // Create a new napi_env for this module. Once module unloading is supported
  // we shall have to call delete on this object from there.
  napi_env env = new napi_env__(context->GetIsolate());

  mod->nm_register_func(
    env,
    v8impl::JsValueFromV8LocalValue(exports),
    v8impl::JsValueFromV8LocalValue(module),
    mod->nm_priv);
}

#ifndef EXTERNAL_NAPI
namespace node {
  // Indicates whether NAPI was enabled/disabled via the node command-line.
  extern bool load_napi_modules;
}
#endif  // EXTERNAL_NAPI

// Registers a NAPI module.
void napi_module_register(napi_module* mod) {
  // NAPI modules always work with the current node version.
  int module_version = NODE_MODULE_VERSION;

#ifndef EXTERNAL_NAPI
  if (!node::load_napi_modules) {
    // NAPI is disabled, so set the module version to -1 to cause the module
    // to be unloaded.
    module_version = -1;
  }
#endif  // EXTERNAL_NAPI

  node::node_module* nm = new node::node_module {
    module_version,
    mod->nm_flags,
    nullptr,
    mod->nm_filename,
    nullptr,
    napi_module_register_cb,
    mod->nm_modname,
    mod,  // priv
    nullptr,
  };
  node::node_module_register(nm);
}

// Warning: Keep in-sync with napi_status enum
const char* error_messages[] = {nullptr,
                                "Invalid pointer passed as argument",
                                "An object was expected",
                                "A string was expected",
                                "A string or symbol was expected",
                                "A function was expected",
                                "A number was expected",
                                "A boolean was expected",
                                "An array was expected",
                                "Unknown failure",
                                "An exception is pending",
                                "The async work item was cancelled"};

void napi_clear_last_error(napi_env env) {
  env->last_error.error_code = napi_ok;

  // TODO(boingoing): Should this be a callback?
  env->last_error.engine_error_code = 0;
  env->last_error.engine_reserved = nullptr;
}

napi_status napi_set_last_error(napi_env env, napi_status error_code,
                                uint32_t engine_error_code,
                                void* engine_reserved) {
  env->last_error.error_code = error_code;
  env->last_error.engine_error_code = engine_error_code;
  env->last_error.engine_reserved = engine_reserved;

  return error_code;
}

napi_status napi_get_last_error_info(napi_env env,
                                     const napi_extended_error_info** result) {
  CHECK_ENV(env);

  static_assert(node::arraysize(error_messages) == napi_status_last,
      "Count of error messages must match count of error values");
  assert(env->last_error.error_code < napi_status_last);

  // Wait until someone requests the last error information to fetch the error
  // message string
  env->last_error.error_message =
      error_messages[env->last_error.error_code];

  *result = &(env->last_error);
  return napi_ok;
}

napi_status napi_create_function(napi_env env,
                                 const char* utf8name,
                                 napi_callback cb,
                                 void* callback_data,
                                 napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Function> return_value;
  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Object> cbdata =
      v8impl::CreateFunctionCallbackData(env, cb, callback_data);

  RETURN_STATUS_IF_FALSE(env, !cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
      isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MaybeLocal<v8::Function> maybe_function = tpl->GetFunction(context);
  CHECK_MAYBE_EMPTY(env, maybe_function, napi_generic_failure);

  return_value = scope.Escape(maybe_function.ToLocalChecked());

  if (utf8name != nullptr) {
    v8::Local<v8::String> name_string;
    CHECK_NEW_FROM_UTF8(env, name_string, utf8name);
    return_value->SetName(name_string);
  }

  *result = v8impl::JsValueFromV8LocalValue(return_value);

  return GET_RETURN_STATUS(env);
}

napi_status napi_define_class(napi_env env,
                              const char* utf8name,
                              napi_callback constructor,
                              void* callback_data,
                              size_t property_count,
                              const napi_property_descriptor* properties,
                              napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Object> cbdata =
      v8impl::CreateFunctionCallbackData(env, constructor, callback_data);

  RETURN_STATUS_IF_FALSE(env, !cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
      isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  // we need an internal field to stash the wrapped object
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  v8::Local<v8::String> name_string;
  CHECK_NEW_FROM_UTF8(env, name_string, utf8name);
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
    // difference is it applies to a template instead of an object.
    if (p->getter != nullptr || p->setter != nullptr) {
      v8::Local<v8::Object> cbdata = v8impl::CreateAccessorCallbackData(
        env, p->getter, p->setter, p->data);

      tpl->PrototypeTemplate()->SetAccessor(
        property_name,
        p->getter ? v8impl::GetterCallbackWrapper::Invoke : nullptr,
        p->setter ? v8impl::SetterCallbackWrapper::Invoke : nullptr,
        cbdata,
        v8::AccessControl::DEFAULT,
        attributes);
    } else if (p->method != nullptr) {
      v8::Local<v8::Object> cbdata =
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

  *result = v8impl::JsValueFromV8LocalValue(scope.Escape(tpl->GetFunction()));

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
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(env, context, obj, object);

  auto maybe_propertynames = obj->GetPropertyNames(context);

  CHECK_MAYBE_EMPTY(env, maybe_propertynames, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(
      maybe_propertynames.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_set_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              napi_value value) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
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

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
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
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  auto get_maybe = obj->Get(context, k);

  CHECK_MAYBE_EMPTY(env, get_maybe, napi_generic_failure);

  v8::Local<v8::Value> val = get_maybe.ToLocalChecked();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS(env);
}

napi_status napi_set_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8name,
                                    napi_value value) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
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

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
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

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

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

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
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

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
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

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(env, context, obj, object);

  auto get_maybe = obj->Get(context, index);

  CHECK_MAYBE_EMPTY(env, get_maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(get_maybe.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_define_properties(napi_env env,
                                   napi_value object,
                                   size_t property_count,
                                   const napi_property_descriptor* properties) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj =
      v8impl::V8LocalValueFromJsValue(object).As<v8::Object>();

  for (size_t i = 0; i < property_count; i++) {
    const napi_property_descriptor* p = &properties[i];

    v8::Local<v8::Name> property_name;
    napi_status status =
        v8impl::V8NameFromPropertyDescriptor(env, p, &property_name);

    if (status != napi_ok) {
      return napi_set_last_error(env, status);
    }

    v8::PropertyAttribute attributes =
        v8impl::V8PropertyAttributesFromDescriptor(p);

    if (p->getter != nullptr || p->setter != nullptr) {
      v8::Local<v8::Object> cbdata = v8impl::CreateAccessorCallbackData(
        env,
        p->getter,
        p->setter,
        p->data);

      auto set_maybe = obj->SetAccessor(
        context,
        property_name,
        p->getter ? v8impl::GetterCallbackWrapper::Invoke : nullptr,
        p->setter ? v8impl::SetterCallbackWrapper::Invoke : nullptr,
        cbdata,
        v8::AccessControl::DEFAULT,
        attributes);

      if (!set_maybe.FromMaybe(false)) {
        return napi_set_last_error(env, napi_invalid_arg);
      }
    } else if (p->method != nullptr) {
      v8::Local<v8::Object> cbdata =
          v8impl::CreateFunctionCallbackData(env, p->method, p->data);

      RETURN_STATUS_IF_FALSE(env, !cbdata.IsEmpty(), napi_generic_failure);

      v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(
          isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

      auto define_maybe = obj->DefineOwnProperty(
        context, property_name, t->GetFunction(), attributes);

      if (!define_maybe.FromMaybe(false)) {
        return napi_set_last_error(env, napi_generic_failure);
      }
    } else {
      v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);

      auto define_maybe =
          obj->DefineOwnProperty(context, property_name, value, attributes);

      if (!define_maybe.FromMaybe(false)) {
        return napi_set_last_error(env, napi_invalid_arg);
      }
    }
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_is_array(napi_env env, napi_value value, bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  *result = val->IsArray();
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_array_length(napi_env env,
                                  napi_value value,
                                  uint32_t* result) {
  NAPI_PREAMBLE(env);
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

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(env, context, obj, object);

  v8::Local<v8::Value> val = obj->GetPrototype();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS(env);
}

napi_status napi_create_object(napi_env env, napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Object::New(env->isolate));

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_array(napi_env env, napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Array::New(env->isolate));

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_array_with_length(napi_env env,
                                          size_t length,
                                          napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Array::New(env->isolate, length));

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_string_utf8(napi_env env,
                                    const char* str,
                                    size_t length,
                                    napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::String> s;
  CHECK_NEW_FROM_UTF8_LEN(env, s, str, length);

  *result = v8impl::JsValueFromV8LocalValue(s);
  return GET_RETURN_STATUS(env);
}

napi_status napi_create_string_utf16(napi_env env,
                                     const char16_t* str,
                                     size_t length,
                                     napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  auto isolate = env->isolate;
  auto str_maybe =
      v8::String::NewFromTwoByte(isolate,
                                 reinterpret_cast<const uint16_t*>(str),
                                 v8::NewStringType::kInternalized,
                                 length);
  CHECK_MAYBE_EMPTY(env, str_maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(str_maybe.ToLocalChecked());
  return GET_RETURN_STATUS(env);
}

napi_status napi_create_number(napi_env env,
                               double value,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Number::New(env->isolate, value));

  return GET_RETURN_STATUS(env);
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

  return napi_ok;
}

napi_status napi_create_symbol(napi_env env,
                               napi_value description,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
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

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_error(napi_env env,
                              napi_value msg,
                              napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> message_value = v8impl::V8LocalValueFromJsValue(msg);
  RETURN_STATUS_IF_FALSE(env, message_value->IsString(), napi_string_expected);

  *result = v8impl::JsValueFromV8LocalValue(v8::Exception::Error(
      message_value.As<v8::String>()));

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_type_error(napi_env env,
                                   napi_value msg,
                                   napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> message_value = v8impl::V8LocalValueFromJsValue(msg);
  RETURN_STATUS_IF_FALSE(env, message_value->IsString(), napi_string_expected);

  *result = v8impl::JsValueFromV8LocalValue(v8::Exception::TypeError(
      message_value.As<v8::String>()));

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_range_error(napi_env env,
                                    napi_value msg,
                                    napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> message_value = v8impl::V8LocalValueFromJsValue(msg);
  RETURN_STATUS_IF_FALSE(env, message_value->IsString(), napi_string_expected);

  *result = v8impl::JsValueFromV8LocalValue(v8::Exception::RangeError(
      message_value.As<v8::String>()));

  return GET_RETURN_STATUS(env);
}

napi_status napi_typeof(napi_env env,
                        napi_value value,
                        napi_valuetype* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(value);

  if (v->IsNumber()) {
    *result = napi_number;
  } else if (v->IsString()) {
    *result = napi_string;
  } else if (v->IsFunction()) {
    // This test has to come before IsObject because IsFunction
    // implies IsObject
    *result = napi_function;
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
  } else if (v->IsExternal()) {
    *result = napi_external;
  } else {
    // Should not get here unless V8 has added some new kind of value.
    return napi_set_last_error(env, napi_invalid_arg);
  }

  return napi_ok;
}

napi_status napi_get_undefined(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Undefined(env->isolate));

  return napi_ok;
}

napi_status napi_get_null(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsValueFromV8LocalValue(
        v8::Null(env->isolate));

  return napi_ok;
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

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  if (argv != nullptr) {
    CHECK_ARG(env, argc);
    info->Args(argv, std::min(*argc, info->ArgsLength()));
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

  return napi_ok;
}

napi_status napi_is_construct_call(napi_env env,
                                   napi_callback_info cbinfo,
                                   bool* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because no V8 APIs are called.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->IsConstructCall();
  return napi_ok;
}

napi_status napi_call_function(napi_env env,
                               napi_value recv,
                               napi_value func,
                               size_t argc,
                               const napi_value* argv,
                               napi_value* result) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Value> v8recv = v8impl::V8LocalValueFromJsValue(recv);

  v8::Local<v8::Value> v8value = v8impl::V8LocalValueFromJsValue(func);
  RETURN_STATUS_IF_FALSE(env, v8value->IsFunction(), napi_invalid_arg);

  v8::Local<v8::Function> v8func = v8value.As<v8::Function>();
  auto maybe = v8func->Call(context, v8recv, argc,
    reinterpret_cast<v8::Local<v8::Value>*>(const_cast<napi_value*>(argv)));

  if (try_catch.HasCaught()) {
    return napi_set_last_error(env, napi_pending_exception);
  } else {
    if (result != nullptr) {
      CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);
      *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
    }
    return napi_ok;
  }
}

napi_status napi_get_global(napi_env env, napi_value* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  // TODO(ianhall): what if we need the global object from a different
  // context in the same isolate?
  // Should napi_env be the current context rather than the current isolate?
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  *result = v8impl::JsValueFromV8LocalValue(context->Global());

  return napi_ok;
}

napi_status napi_throw(napi_env env, napi_value error) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;

  isolate->ThrowException(v8impl::V8LocalValueFromJsValue(error));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_error(napi_env env, const char* msg) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(env, str, msg);

  isolate->ThrowException(v8::Exception::Error(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_type_error(napi_env env, const char* msg) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(env, str, msg);

  isolate->ThrowException(v8::Exception::TypeError(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_range_error(napi_env env, const char* msg) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(env, str, msg);

  isolate->ThrowException(v8::Exception::RangeError(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_is_error(napi_env env, napi_value value, bool* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot
  // throw JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsNativeError();

  return napi_ok;
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

  return napi_ok;
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
  RETURN_STATUS_IF_FALSE(env, val->IsNumber(), napi_number_expected);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  *result = val->Int32Value(context).ToChecked();

  return napi_ok;
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
  RETURN_STATUS_IF_FALSE(env, val->IsNumber(), napi_number_expected);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  *result = val->Uint32Value(context).ToChecked();

  return napi_ok;
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
  RETURN_STATUS_IF_FALSE(env, val->IsNumber(), napi_number_expected);

  // v8::Value::IntegerValue() converts NaN to INT64_MIN, inconsistent with
  // v8::Value::Int32Value() that converts NaN to 0. So special-case NaN here.
  double doubleValue = val.As<v8::Number>()->Value();
  if (std::isnan(doubleValue)) {
    *result = 0;
  } else {
    v8::Isolate* isolate = env->isolate;
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    *result = val->IntegerValue(context).ToChecked();
  }

  return napi_ok;
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

  return napi_ok;
}

// Gets the number of CHARACTERS in the string.
napi_status napi_get_value_string_length(napi_env env,
                                         napi_value value,
                                         size_t* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsString(), napi_string_expected);

  *result = val.As<v8::String>()->Length();

  return GET_RETURN_STATUS(env);
}

// Copies a JavaScript string into a UTF-8 string buffer. The result is the
// number of bytes copied into buf, including the null terminator. If bufsize
// is insufficient, the string will be truncated, including a null terminator.
// If buf is NULL, this method returns the length of the string (in bytes)
// via the result parameter.
// The result argument is optional unless buf is NULL.
napi_status napi_get_value_string_utf8(napi_env env,
                                       napi_value value,
                                       char* buf,
                                       size_t bufsize,
                                       size_t* result) {
  NAPI_PREAMBLE(env);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsString(), napi_string_expected);

  if (!buf) {
    CHECK_ARG(env, result);
    *result = val.As<v8::String>()->Utf8Length();
  } else {
    int copied = val.As<v8::String>()->WriteUtf8(
      buf, bufsize, nullptr, v8::String::REPLACE_INVALID_UTF8);

    if (result != nullptr) {
      *result = copied;
    }
  }

  return GET_RETURN_STATUS(env);
}

// Copies a JavaScript string into a UTF-16 string buffer. The result is the
// number of 2-byte code units copied into buf, including the null terminator.
// If bufsize is insufficient, the string will be truncated, including a null
// terminator. If buf is NULL, this method returns the length of the string
// (in 2-byte code units) via the result parameter.
// The result argument is optional unless buf is NULL.
napi_status napi_get_value_string_utf16(napi_env env,
                                        napi_value value,
                                        char16_t* buf,
                                        size_t bufsize,
                                        size_t* result) {
  NAPI_PREAMBLE(env);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsString(), napi_string_expected);

  if (!buf) {
    CHECK_ARG(env, result);
    // V8 assumes UTF-16 length is the same as the number of characters.
    *result = val.As<v8::String>()->Length();
  } else {
    int copied = val.As<v8::String>()->Write(
      reinterpret_cast<uint16_t*>(buf), 0, bufsize, v8::String::NO_OPTIONS);

    if (result != nullptr) {
      *result = copied;
    }
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_coerce_to_object(napi_env env,
                                  napi_value value,
                                  napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(env, context, obj, value);

  *result = v8impl::JsValueFromV8LocalValue(obj);
  return GET_RETURN_STATUS(env);
}

napi_status napi_coerce_to_bool(napi_env env,
                                napi_value value,
                                napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Boolean> b;

  CHECK_TO_BOOL(env, context, b, value);

  *result = v8impl::JsValueFromV8LocalValue(b);
  return GET_RETURN_STATUS(env);
}

napi_status napi_coerce_to_number(napi_env env,
                                  napi_value value,
                                  napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Number> num;

  CHECK_TO_NUMBER(env, context, num, value);

  *result = v8impl::JsValueFromV8LocalValue(num);
  return GET_RETURN_STATUS(env);
}

napi_status napi_coerce_to_string(napi_env env,
                                  napi_value value,
                                  napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::String> str;

  CHECK_TO_STRING(env, context, str, value);

  *result = v8impl::JsValueFromV8LocalValue(str);
  return GET_RETURN_STATUS(env);
}

napi_status napi_wrap(napi_env env,
                      napi_value js_object,
                      void* native_object,
                      napi_finalize finalize_cb,
                      void* finalize_hint,
                      napi_ref* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, js_object);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Object> obj =
      v8impl::V8LocalValueFromJsValue(js_object).As<v8::Object>();

  // Only objects that were created from a NAPI constructor's prototype
  // via napi_define_class() can be (un)wrapped.
  RETURN_STATUS_IF_FALSE(env, obj->InternalFieldCount() > 0, napi_invalid_arg);

  obj->SetInternalField(0, v8::External::New(isolate, native_object));

  if (result != nullptr) {
    // The returned reference should be deleted via napi_delete_reference()
    // ONLY in response to the finalize callback invocation. (If it is deleted
    // before then, then the finalize callback will never be invoked.)
    // Therefore a finalize callback is required when returning a reference.
    CHECK_ARG(env, finalize_cb);
    v8impl::Reference* reference = v8impl::Reference::New(
        env, obj, 0, false, finalize_cb, native_object, finalize_hint);
    *result = reinterpret_cast<napi_ref>(reference);
  } else if (finalize_cb != nullptr) {
    // Create a self-deleting reference just for the finalize callback.
    v8impl::Reference::New(
        env, obj, 0, true, finalize_cb, native_object, finalize_hint);
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_unwrap(napi_env env, napi_value js_object, void** result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, js_object);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(js_object);
  RETURN_STATUS_IF_FALSE(env, value->IsObject(), napi_invalid_arg);
  v8::Local<v8::Object> obj = value.As<v8::Object>();

  // Only objects that were created from a NAPI constructor's prototype
  // via napi_define_class() can be (un)wrapped.
  RETURN_STATUS_IF_FALSE(env, obj->InternalFieldCount() > 0, napi_invalid_arg);

  v8::Local<v8::Value> unwrappedValue = obj->GetInternalField(0);
  RETURN_STATUS_IF_FALSE(env, unwrappedValue->IsExternal(), napi_invalid_arg);

  *result = unwrappedValue.As<v8::External>()->Value();

  return napi_ok;
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

  return GET_RETURN_STATUS(env);
}

napi_status napi_get_value_external(napi_env env,
                                    napi_value value,
                                    void** result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(env, val->IsExternal(), napi_invalid_arg);

  v8::Local<v8::External> external_value = val.As<v8::External>();
  *result = external_value->Value();

  return GET_RETURN_STATUS(env);
}

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
napi_status napi_create_reference(napi_env env,
                                  napi_value value,
                                  uint32_t initial_refcount,
                                  napi_ref* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8impl::Reference* reference = v8impl::Reference::New(
      env, v8impl::V8LocalValueFromJsValue(value), initial_refcount, false);

  *result = reinterpret_cast<napi_ref>(reference);
  return GET_RETURN_STATUS(env);
}

// Deletes a reference. The referenced value is released, and may be GC'd unless
// there are other references to it.
napi_status napi_delete_reference(napi_env env, napi_ref ref) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, ref);

  v8impl::Reference::Delete(reinterpret_cast<v8impl::Reference*>(ref));

  return napi_ok;
}

// Increments the reference count, optionally returning the resulting count.
// After this call the reference will be a strong reference because its
// refcount is >0, and the referenced object is effectively "pinned".
// Calling this when the refcount is 0 and the object is unavailable
// results in an error.
napi_status napi_reference_ref(napi_env env, napi_ref ref, uint32_t* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, ref);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  uint32_t count = reference->Ref();

  if (result != nullptr) {
    *result = count;
  }

  return GET_RETURN_STATUS(env);
}

// Decrements the reference count, optionally returning the resulting count. If
// the result is 0 the reference is now weak and the object may be GC'd at any
// time if there are no other references. Calling this when the refcount is
// already 0 results in an error.
napi_status napi_reference_unref(napi_env env, napi_ref ref, uint32_t* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, ref);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);

  if (reference->RefCount() == 0) {
    return napi_set_last_error(env, napi_generic_failure);
  }

  uint32_t count = reference->Unref();

  if (result != nullptr) {
    *result = count;
  }

  return GET_RETURN_STATUS(env);
}

// Attempts to get a referenced value. If the reference is weak, the value might
// no longer be available, in that case the call is still successful but the
// result is NULL.
napi_status napi_get_reference_value(napi_env env,
                                     napi_ref ref,
                                     napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, ref);
  CHECK_ARG(env, result);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  *result = v8impl::JsValueFromV8LocalValue(reference->Get());

  return GET_RETURN_STATUS(env);
}

napi_status napi_open_handle_scope(napi_env env, napi_handle_scope* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsHandleScopeFromV8HandleScope(
      new v8impl::HandleScopeWrapper(env->isolate));
  return GET_RETURN_STATUS(env);
}

napi_status napi_close_handle_scope(napi_env env, napi_handle_scope scope) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, scope);

  delete v8impl::V8HandleScopeFromJsHandleScope(scope);
  return GET_RETURN_STATUS(env);
}

napi_status napi_open_escapable_handle_scope(
    napi_env env,
    napi_escapable_handle_scope* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  *result = v8impl::JsEscapableHandleScopeFromV8EscapableHandleScope(
      new v8impl::EscapableHandleScopeWrapper(env->isolate));
  return GET_RETURN_STATUS(env);
}

napi_status napi_close_escapable_handle_scope(
    napi_env env,
    napi_escapable_handle_scope scope) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, scope);

  delete v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  return GET_RETURN_STATUS(env);
}

napi_status napi_escape_handle(napi_env env,
                               napi_escapable_handle_scope scope,
                               napi_value escapee,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, scope);
  CHECK_ARG(env, result);

  v8impl::EscapableHandleScopeWrapper* s =
      v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  *result = v8impl::JsValueFromV8LocalValue(
      s->Escape(v8impl::V8LocalValueFromJsValue(escapee)));
  return GET_RETURN_STATUS(env);
}

napi_status napi_new_instance(napi_env env,
                              napi_value constructor,
                              size_t argc,
                              const napi_value* argv,
                              napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Value> v8value = v8impl::V8LocalValueFromJsValue(constructor);
  RETURN_STATUS_IF_FALSE(env, v8value->IsFunction(), napi_invalid_arg);

  v8::Local<v8::Function> ctor = v8value.As<v8::Function>();

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
  CHECK_ARG(env, result);

  *result = false;

  v8::Local<v8::Object> ctor;
  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  CHECK_TO_OBJECT(env, context, ctor, constructor);

  if (!ctor->IsFunction()) {
    napi_throw_type_error(env, "constructor must be a function");

    return napi_set_last_error(env, napi_function_expected);
  }

  if (env->has_instance_available) {
    napi_value value, js_result, has_instance = nullptr;
    napi_status status = napi_generic_failure;
    napi_valuetype value_type;

    // Get "Symbol" from the global object
    if (env->has_instance.IsEmpty()) {
      status = napi_get_global(env, &value);
      if (status != napi_ok) return status;
      status = napi_get_named_property(env, value, "Symbol", &value);
      if (status != napi_ok) return status;
      status = napi_typeof(env, value, &value_type);
      if (status != napi_ok) return status;

      // Get "hasInstance" from Symbol
      if (value_type == napi_function) {
        status = napi_get_named_property(env, value, "hasInstance", &value);
        if (status != napi_ok) return status;
        status = napi_typeof(env, value, &value_type);
        if (status != napi_ok) return status;

        // Store Symbol.hasInstance in a global persistent reference
        if (value_type == napi_symbol) {
          env->has_instance.Reset(env->isolate,
              v8impl::V8LocalValueFromJsValue(value));
          has_instance = value;
        }
      }
    } else {
      has_instance = v8impl::JsValueFromV8LocalValue(
          v8::Local<v8::Value>::New(env->isolate, env->has_instance));
    }

    if (has_instance) {
      status = napi_get_property(env, constructor, has_instance, &value);
      if (status != napi_ok) return status;
      status = napi_typeof(env, value, &value_type);
      if (status != napi_ok) return status;

      // Call the function to determine whether the object is an instance of the
      // constructor
      if (value_type == napi_function) {
        status = napi_call_function(env, constructor, value, 1, &object,
          &js_result);
        if (status != napi_ok) return status;
        return napi_get_value_bool(env, js_result, result);
      }
    }

    env->has_instance_available = false;
  }

  // If running constructor[Symbol.hasInstance](object) did not work, we perform
  // a traditional instanceof (early Node.js 6.x).

  v8::Local<v8::String> prototype_string;
  CHECK_NEW_FROM_UTF8(env, prototype_string, "prototype");

  auto maybe_prototype = ctor->Get(context, prototype_string);
  CHECK_MAYBE_EMPTY(env, maybe_prototype, napi_generic_failure);

  v8::Local<v8::Value> prototype_property = maybe_prototype.ToLocalChecked();
  if (!prototype_property->IsObject()) {
    napi_throw_type_error(env, "constructor.prototype must be an object");

    return napi_set_last_error(env, napi_object_expected);
  }

  auto maybe_ctor = prototype_property->ToObject(context);
  CHECK_MAYBE_EMPTY(env, maybe_ctor, napi_generic_failure);
  ctor = maybe_ctor.ToLocalChecked();

  v8::Local<v8::Value> current_obj = v8impl::V8LocalValueFromJsValue(object);
  if (!current_obj->StrictEquals(ctor)) {
    for (v8::Local<v8::Value> original_obj = current_obj;
         !(current_obj->IsNull() || current_obj->IsUndefined());) {
      if (current_obj->StrictEquals(ctor)) {
        *result = !(original_obj->IsNumber() ||
                    original_obj->IsBoolean() ||
                    original_obj->IsString());
        break;
      }
      v8::Local<v8::Object> obj;
      CHECK_TO_OBJECT(env, context, obj, v8impl::JsValueFromV8LocalValue(
        current_obj));
      current_obj = obj->GetPrototype();
    }
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_make_callback(napi_env env,
                               napi_value recv,
                               napi_value func,
                               size_t argc,
                               const napi_value* argv,
                               napi_value* result) {
  NAPI_PREAMBLE(env);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Object> v8recv =
      v8impl::V8LocalValueFromJsValue(recv).As<v8::Object>();
  v8::Local<v8::Function> v8func =
      v8impl::V8LocalValueFromJsValue(func).As<v8::Function>();

  v8::Local<v8::Value> callback_result = node::MakeCallback(
      isolate, v8recv, v8func, argc,
      reinterpret_cast<v8::Local<v8::Value>*>(const_cast<napi_value*>(argv)));

  if (result != nullptr) {
    *result = v8impl::JsValueFromV8LocalValue(callback_result);
  }

  return GET_RETURN_STATUS(env);
}

// Methods to support catching exceptions
napi_status napi_is_exception_pending(napi_env env, bool* result) {
  // NAPI_PREAMBLE is not used here: this function must execute when there is a
  // pending exception.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  *result = !env->last_exception.IsEmpty();
  return napi_ok;
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

  return napi_ok;
}

napi_status napi_create_buffer(napi_env env,
                               size_t length,
                               void** data,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, data);
  CHECK_ARG(env, result);

  auto maybe = node::Buffer::New(env->isolate, length);

  CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);

  v8::Local<v8::Object> buffer = maybe.ToLocalChecked();

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  *data = node::Buffer::Data(buffer);

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_external_buffer(napi_env env,
                                        size_t length,
                                        void* data,
                                        napi_finalize finalize_cb,
                                        void* finalize_hint,
                                        napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;

  // The finalizer object will delete itself after invoking the callback.
  v8impl::Finalizer* finalizer = v8impl::Finalizer::New(
    env, finalize_cb, nullptr, finalize_hint);

  auto maybe = node::Buffer::New(isolate,
                                 static_cast<char*>(data),
                                 length,
                                 v8impl::Finalizer::FinalizeBufferCallback,
                                 finalizer);

  CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return GET_RETURN_STATUS(env);
  // Tell coverity that 'finalizer' should not be freed when we return
  // as it will be deleted when the buffer to which it is associated
  // is finalized.
  // coverity[leaked_storage]
}

napi_status napi_create_buffer_copy(napi_env env,
                                    size_t length,
                                    const void* data,
                                    void** result_data,
                                    napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  auto maybe = node::Buffer::Copy(env->isolate,
    static_cast<const char*>(data), length);

  CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);

  v8::Local<v8::Object> buffer = maybe.ToLocalChecked();
  *result = v8impl::JsValueFromV8LocalValue(buffer);

  if (result_data != nullptr) {
    *result_data = node::Buffer::Data(buffer);
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_is_buffer(napi_env env, napi_value value, bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  *result = node::Buffer::HasInstance(v8impl::V8LocalValueFromJsValue(value));
  return GET_RETURN_STATUS(env);
}

napi_status napi_get_buffer_info(napi_env env,
                                 napi_value value,
                                 void** data,
                                 size_t* length) {
  NAPI_PREAMBLE(env);

  v8::Local<v8::Object> buffer =
      v8impl::V8LocalValueFromJsValue(value).As<v8::Object>();

  if (data != nullptr) {
    *data = node::Buffer::Data(buffer);
  }
  if (length != nullptr) {
    *length = node::Buffer::Length(buffer);
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_is_arraybuffer(napi_env env, napi_value value, bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsArrayBuffer();

  return GET_RETURN_STATUS(env);
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
  // retreive it.
  if (data != nullptr) {
    *data = buffer->GetContents().Data();
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
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, external_data, byte_length);

  if (finalize_cb != nullptr) {
    // Create a self-deleting weak reference that invokes the finalizer
    // callback.
    v8impl::Reference::New(env,
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
  NAPI_PREAMBLE(env);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(env, value->IsArrayBuffer(), napi_invalid_arg);

  v8::ArrayBuffer::Contents contents =
      value.As<v8::ArrayBuffer>()->GetContents();

  if (data != nullptr) {
    *data = contents.Data();
  }

  if (byte_length != nullptr) {
    *byte_length = contents.ByteLength();
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_is_typedarray(napi_env env, napi_value value, bool* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsTypedArray();

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_typedarray(napi_env env,
                                   napi_typedarray_type type,
                                   size_t length,
                                   napi_value arraybuffer,
                                   size_t byte_offset,
                                   napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(env, value->IsArrayBuffer(), napi_invalid_arg);

  v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
  v8::Local<v8::TypedArray> typedArray;

  switch (type) {
    case napi_int8_array:
      typedArray = v8::Int8Array::New(buffer, byte_offset, length);
      break;
    case napi_uint8_array:
      typedArray = v8::Uint8Array::New(buffer, byte_offset, length);
      break;
    case napi_uint8_clamped_array:
      typedArray = v8::Uint8ClampedArray::New(buffer, byte_offset, length);
      break;
    case napi_int16_array:
      typedArray = v8::Int16Array::New(buffer, byte_offset, length);
      break;
    case napi_uint16_array:
      typedArray = v8::Uint16Array::New(buffer, byte_offset, length);
      break;
    case napi_int32_array:
      typedArray = v8::Int32Array::New(buffer, byte_offset, length);
      break;
    case napi_uint32_array:
      typedArray = v8::Uint32Array::New(buffer, byte_offset, length);
      break;
    case napi_float32_array:
      typedArray = v8::Float32Array::New(buffer, byte_offset, length);
      break;
    case napi_float64_array:
      typedArray = v8::Float64Array::New(buffer, byte_offset, length);
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
  NAPI_PREAMBLE(env);

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
    }
  }

  if (length != nullptr) {
    *length = array->Length();
  }

  v8::Local<v8::ArrayBuffer> buffer = array->Buffer();
  if (data != nullptr) {
    *data = static_cast<uint8_t*>(buffer->GetContents().Data()) +
            array->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = v8impl::JsValueFromV8LocalValue(buffer);
  }

  if (byte_offset != nullptr) {
    *byte_offset = array->ByteOffset();
  }

  return GET_RETURN_STATUS(env);
}

namespace uvimpl {

napi_status ConvertUVErrorCode(int code) {
  switch (code) {
  case 0:
    return napi_ok;
  case UV_EINVAL:
    return napi_invalid_arg;
  case UV_ECANCELED:
    return napi_cancelled;
  }

  return napi_generic_failure;
}

// Wrapper around uv_work_t which calls user-provided callbacks.
class Work {
 private:
  explicit Work(napi_env env,
                napi_async_execute_callback execute = nullptr,
                napi_async_complete_callback complete = nullptr,
                void* data = nullptr)
    : _env(env),
    _data(data),
    _execute(execute),
    _complete(complete) {
    _request.data = this;
  }

  ~Work() { }

 public:
  static Work* New(napi_env env,
                   napi_async_execute_callback execute,
                   napi_async_complete_callback complete,
                   void* data) {
    return new Work(env, execute, complete, data);
  }

  static void Delete(Work* work) {
    delete work;
  }

  static void ExecuteCallback(uv_work_t* req) {
    Work* work = static_cast<Work*>(req->data);
    work->_execute(work->_env, work->_data);
  }

  static void CompleteCallback(uv_work_t* req, int status) {
    Work* work = static_cast<Work*>(req->data);

    if (work->_complete != nullptr) {
      work->_complete(work->_env, ConvertUVErrorCode(status), work->_data);
    }
  }

  uv_work_t* Request() {
    return &_request;
  }

 private:
  napi_env _env;
  void* _data;
  uv_work_t _request;
  napi_async_execute_callback _execute;
  napi_async_complete_callback _complete;
};

}  // end of namespace uvimpl

#define CALL_UV(env, condition)                                         \
  do {                                                                  \
    int result = (condition);                                           \
    napi_status status = uvimpl::ConvertUVErrorCode(result);            \
    if (status != napi_ok) {                                            \
      return napi_set_last_error(env, status, result);                  \
    }                                                                   \
  } while (0)

napi_status napi_create_async_work(napi_env env,
                                   napi_async_execute_callback execute,
                                   napi_async_complete_callback complete,
                                   void* data,
                                   napi_async_work* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, execute);
  CHECK_ARG(env, result);

  uvimpl::Work* work = uvimpl::Work::New(env, execute, complete, data);

  *result = reinterpret_cast<napi_async_work>(work);

  return napi_ok;
}

napi_status napi_delete_async_work(napi_env env, napi_async_work work) {
  CHECK_ENV(env);
  CHECK_ARG(env, work);

  uvimpl::Work::Delete(reinterpret_cast<uvimpl::Work*>(work));

  return napi_ok;
}

napi_status napi_queue_async_work(napi_env env, napi_async_work work) {
  CHECK_ENV(env);
  CHECK_ARG(env, work);

  // Consider: Encapsulate the uv_loop_t into an opaque pointer parameter
  uv_loop_t* event_loop =
    node::Environment::GetCurrent(env->isolate)->event_loop();

  uvimpl::Work* w = reinterpret_cast<uvimpl::Work*>(work);

  CALL_UV(env, uv_queue_work(event_loop,
                             w->Request(),
                             uvimpl::Work::ExecuteCallback,
                             uvimpl::Work::CompleteCallback));

  return napi_ok;
}

napi_status napi_cancel_async_work(napi_env env, napi_async_work work) {
  CHECK_ENV(env);
  CHECK_ARG(env, work);

  uvimpl::Work* w = reinterpret_cast<uvimpl::Work*>(work);

  CALL_UV(env, uv_cancel(reinterpret_cast<uv_req_t*>(w->Request())));

  return napi_ok;
}
