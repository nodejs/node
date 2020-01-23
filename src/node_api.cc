#include "env-inl.h"
#define NAPI_EXPERIMENTAL
#include "js_native_api_v8.h"
#include "node_api.h"
#include "node_binding.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_internals.h"
#include "threadpoolwork-inl.h"
#include "util-inl.h"

#include <memory>

struct node_napi_env__ : public napi_env__ {
  explicit node_napi_env__(v8::Local<v8::Context> context):
      napi_env__(context) {
    CHECK_NOT_NULL(node_env());
  }

  inline node::Environment* node_env() const {
    return node::Environment::GetCurrent(context());
  }

  bool can_call_into_js() const override {
    return node_env()->can_call_into_js();
  }

  v8::Maybe<bool> mark_arraybuffer_as_untransferable(
      v8::Local<v8::ArrayBuffer> ab) const override {
    return ab->SetPrivate(
        context(),
        node_env()->arraybuffer_untransferable_private_symbol(),
        v8::True(isolate));
  }
};

typedef node_napi_env__* node_napi_env;

namespace v8impl {

namespace {

class BufferFinalizer : private Finalizer {
 public:
  // node::Buffer::FreeCallback
  static void FinalizeBufferCallback(char* data, void* hint) {
    std::unique_ptr<BufferFinalizer, Deleter> finalizer{
        static_cast<BufferFinalizer*>(hint)};
    finalizer->_finalize_data = data;

    node::Environment* node_env =
        static_cast<node_napi_env>(finalizer->_env)->node_env();
    node_env->SetImmediate(
        [finalizer = std::move(finalizer)](node::Environment* env) {
      if (finalizer->_finalize_callback == nullptr) return;

      v8::HandleScope handle_scope(finalizer->_env->isolate);
      v8::Context::Scope context_scope(finalizer->_env->context());

      finalizer->_env->CallIntoModuleThrow([&](napi_env env) {
        finalizer->_finalize_callback(
            env,
            finalizer->_finalize_data,
            finalizer->_finalize_hint);
      });
    });
  }

  struct Deleter {
    void operator()(BufferFinalizer* finalizer) {
      Finalizer::Delete(finalizer);
    }
  };
};

static inline napi_env NewEnv(v8::Local<v8::Context> context) {
  node_napi_env result;

  result = new node_napi_env__(context);
  // TODO(addaleax): There was previously code that tried to delete the
  // napi_env when its v8::Context was garbage collected;
  // However, as long as N-API addons using this napi_env are in place,
  // the Context needs to be accessible and alive.
  // Ideally, we'd want an on-addon-unload hook that takes care of this
  // once all N-API addons using this napi_env are unloaded.
  // For now, a per-Environment cleanup hook is the best we can do.
  result->node_env()->AddCleanupHook(
      [](void* arg) {
        static_cast<napi_env>(arg)->Unref();
      },
      static_cast<void*>(result));

  return result;
}

static inline napi_callback_scope
JsCallbackScopeFromV8CallbackScope(node::CallbackScope* s) {
  return reinterpret_cast<napi_callback_scope>(s);
}

static inline node::CallbackScope*
V8CallbackScopeFromJsCallbackScope(napi_callback_scope s) {
  return reinterpret_cast<node::CallbackScope*>(s);
}

static inline void trigger_fatal_exception(
    napi_env env, v8::Local<v8::Value> local_err) {
  v8::Local<v8::Message> local_msg =
    v8::Exception::CreateMessage(env->isolate, local_err);
  node::errors::TriggerUncaughtException(env->isolate, local_err, local_msg);
}

class ThreadSafeFunction : public node::AsyncResource {
 public:
  ThreadSafeFunction(v8::Local<v8::Function> func,
                     v8::Local<v8::Object> resource,
                     v8::Local<v8::String> name,
                     size_t thread_count_,
                     void* context_,
                     size_t max_queue_size_,
                     node_napi_env env_,
                     void* finalize_data_,
                     napi_finalize finalize_cb_,
                     napi_threadsafe_function_call_js call_js_cb_):
                     AsyncResource(env_->isolate,
                                   resource,
                                   *v8::String::Utf8Value(env_->isolate, name)),
      thread_count(thread_count_),
      is_closing(false),
      context(context_),
      max_queue_size(max_queue_size_),
      env(env_),
      finalize_data(finalize_data_),
      finalize_cb(finalize_cb_),
      call_js_cb(call_js_cb_ == nullptr ? CallJs : call_js_cb_),
      handles_closing(false) {
    ref.Reset(env->isolate, func);
    node::AddEnvironmentCleanupHook(env->isolate, Cleanup, this);
    env->Ref();
  }

  ~ThreadSafeFunction() override {
    node::RemoveEnvironmentCleanupHook(env->isolate, Cleanup, this);
    env->Unref();
  }

  // These methods can be called from any thread.

  napi_status Push(void* data, napi_threadsafe_function_call_mode mode) {
    node::Mutex::ScopedLock lock(this->mutex);

    while (queue.size() >= max_queue_size &&
        max_queue_size > 0 &&
        !is_closing) {
      if (mode == napi_tsfn_nonblocking) {
        return napi_queue_full;
      }
      cond->Wait(lock);
    }

    if (is_closing) {
      if (thread_count == 0) {
        return napi_invalid_arg;
      } else {
        thread_count--;
        return napi_closing;
      }
    } else {
      if (uv_async_send(&async) != 0) {
        return napi_generic_failure;
      }
      queue.push(data);
      return napi_ok;
    }
  }

  napi_status Acquire() {
    node::Mutex::ScopedLock lock(this->mutex);

    if (is_closing) {
      return napi_closing;
    }

    thread_count++;

    return napi_ok;
  }

  napi_status Release(napi_threadsafe_function_release_mode mode) {
    node::Mutex::ScopedLock lock(this->mutex);

    if (thread_count == 0) {
      return napi_invalid_arg;
    }

    thread_count--;

    if (thread_count == 0 || mode == napi_tsfn_abort) {
      if (!is_closing) {
        is_closing = (mode == napi_tsfn_abort);
        if (is_closing && max_queue_size > 0) {
          cond->Signal(lock);
        }
        if (uv_async_send(&async) != 0) {
          return napi_generic_failure;
        }
      }
    }

    return napi_ok;
  }

  void EmptyQueueAndDelete() {
    for (; !queue.empty() ; queue.pop()) {
      call_js_cb(nullptr, nullptr, context, queue.front());
    }
    delete this;
  }

  // These methods must only be called from the loop thread.

  napi_status Init() {
    ThreadSafeFunction* ts_fn = this;
    uv_loop_t* loop = env->node_env()->event_loop();

    if (uv_async_init(loop, &async, AsyncCb) == 0) {
      if (max_queue_size > 0) {
        cond = std::make_unique<node::ConditionVariable>();
      }
      if ((max_queue_size == 0 || cond) &&
          uv_idle_init(loop, &idle) == 0) {
        return napi_ok;
      }

      env->node_env()->CloseHandle(
          reinterpret_cast<uv_handle_t*>(&async),
          [](uv_handle_t* handle) -> void {
            ThreadSafeFunction* ts_fn =
                node::ContainerOf(&ThreadSafeFunction::async,
                                  reinterpret_cast<uv_async_t*>(handle));
            delete ts_fn;
          });

      // Prevent the thread-safe function from being deleted here, because
      // the callback above will delete it.
      ts_fn = nullptr;
    }

    delete ts_fn;

    return napi_generic_failure;
  }

  napi_status Unref() {
    uv_unref(reinterpret_cast<uv_handle_t*>(&async));
    uv_unref(reinterpret_cast<uv_handle_t*>(&idle));

    return napi_ok;
  }

  napi_status Ref() {
    uv_ref(reinterpret_cast<uv_handle_t*>(&async));
    uv_ref(reinterpret_cast<uv_handle_t*>(&idle));

    return napi_ok;
  }

  void DispatchOne() {
    void* data = nullptr;
    bool popped_value = false;
    bool idle_stop_failed = false;

    {
      node::Mutex::ScopedLock lock(this->mutex);
      if (is_closing) {
        CloseHandlesAndMaybeDelete();
      } else {
        size_t size = queue.size();
        if (size > 0) {
          data = queue.front();
          queue.pop();
          popped_value = true;
          if (size == max_queue_size && max_queue_size > 0) {
            cond->Signal(lock);
          }
          size--;
        }

        if (size == 0) {
          if (thread_count == 0) {
            is_closing = true;
            if (max_queue_size > 0) {
              cond->Signal(lock);
            }
            CloseHandlesAndMaybeDelete();
          } else {
            if (uv_idle_stop(&idle) != 0) {
              idle_stop_failed = true;
            }
          }
        }
      }
    }

    if (popped_value || idle_stop_failed) {
      v8::HandleScope scope(env->isolate);
      CallbackScope cb_scope(this);

      if (idle_stop_failed) {
        CHECK(napi_throw_error(env,
                               "ERR_NAPI_TSFN_STOP_IDLE_LOOP",
                               "Failed to stop the idle loop") == napi_ok);
      } else {
        napi_value js_callback = nullptr;
        if (!ref.IsEmpty()) {
          v8::Local<v8::Function> js_cb =
            v8::Local<v8::Function>::New(env->isolate, ref);
          js_callback = v8impl::JsValueFromV8LocalValue(js_cb);
        }
        env->CallIntoModuleThrow([&](napi_env env) {
          call_js_cb(env, js_callback, context, data);
        });
      }
    }
  }

  void MaybeStartIdle() {
    if (uv_idle_start(&idle, IdleCb) != 0) {
      v8::HandleScope scope(env->isolate);
      CallbackScope cb_scope(this);
      CHECK(napi_throw_error(env,
                             "ERR_NAPI_TSFN_START_IDLE_LOOP",
                             "Failed to start the idle loop") == napi_ok);
    }
  }

  void Finalize() {
    v8::HandleScope scope(env->isolate);
    if (finalize_cb) {
      CallbackScope cb_scope(this);
      env->CallIntoModuleThrow([&](napi_env env) {
        finalize_cb(env, finalize_data, context);
      });
    }
    EmptyQueueAndDelete();
  }

  inline void* Context() {
    return context;
  }

  void CloseHandlesAndMaybeDelete(bool set_closing = false) {
    v8::HandleScope scope(env->isolate);
    if (set_closing) {
      node::Mutex::ScopedLock lock(this->mutex);
      is_closing = true;
      if (max_queue_size > 0) {
        cond->Signal(lock);
      }
    }
    if (handles_closing) {
      return;
    }
    handles_closing = true;
    env->node_env()->CloseHandle(
        reinterpret_cast<uv_handle_t*>(&async),
        [](uv_handle_t* handle) -> void {
          ThreadSafeFunction* ts_fn =
              node::ContainerOf(&ThreadSafeFunction::async,
                                reinterpret_cast<uv_async_t*>(handle));
          v8::HandleScope scope(ts_fn->env->isolate);
          ts_fn->env->node_env()->CloseHandle(
              reinterpret_cast<uv_handle_t*>(&ts_fn->idle),
              [](uv_handle_t* handle) -> void {
                ThreadSafeFunction* ts_fn =
                    node::ContainerOf(&ThreadSafeFunction::idle,
                                      reinterpret_cast<uv_idle_t*>(handle));
                ts_fn->Finalize();
              });
        });
  }

  // Default way of calling into JavaScript. Used when ThreadSafeFunction is
  //  without a call_js_cb_.
  static void CallJs(napi_env env, napi_value cb, void* context, void* data) {
    if (!(env == nullptr || cb == nullptr)) {
      napi_value recv;
      napi_status status;

      status = napi_get_undefined(env, &recv);
      if (status != napi_ok) {
        napi_throw_error(env, "ERR_NAPI_TSFN_GET_UNDEFINED",
            "Failed to retrieve undefined value");
        return;
      }

      status = napi_call_function(env, recv, cb, 0, nullptr, nullptr);
      if (status != napi_ok && status != napi_pending_exception) {
        napi_throw_error(env, "ERR_NAPI_TSFN_CALL_JS",
            "Failed to call JS callback");
        return;
      }
    }
  }

  static void IdleCb(uv_idle_t* idle) {
    ThreadSafeFunction* ts_fn =
        node::ContainerOf(&ThreadSafeFunction::idle, idle);
    ts_fn->DispatchOne();
  }

  static void AsyncCb(uv_async_t* async) {
    ThreadSafeFunction* ts_fn =
        node::ContainerOf(&ThreadSafeFunction::async, async);
    ts_fn->MaybeStartIdle();
  }

  static void Cleanup(void* data) {
    reinterpret_cast<ThreadSafeFunction*>(data)
        ->CloseHandlesAndMaybeDelete(true);
  }

 private:
  // These are variables protected by the mutex.
  node::Mutex mutex;
  std::unique_ptr<node::ConditionVariable> cond;
  std::queue<void*> queue;
  uv_async_t async;
  uv_idle_t idle;
  size_t thread_count;
  bool is_closing;

  // These are variables set once, upon creation, and then never again, which
  // means we don't need the mutex to read them.
  void* context;
  size_t max_queue_size;

  // These are variables accessed only from the loop thread.
  v8impl::Persistent<v8::Function> ref;
  node_napi_env env;
  void* finalize_data;
  napi_finalize finalize_cb;
  napi_threadsafe_function_call_js call_js_cb;
  bool handles_closing;
};

}  // end of anonymous namespace

}  // end of namespace v8impl

// Intercepts the Node-V8 module registration callback. Converts parameters
// to NAPI equivalents and then calls the registration callback specified
// by the NAPI module.
static void napi_module_register_cb(v8::Local<v8::Object> exports,
                                    v8::Local<v8::Value> module,
                                    v8::Local<v8::Context> context,
                                    void* priv) {
  napi_module_register_by_symbol(exports, module, context,
      static_cast<napi_module*>(priv)->nm_register_func);
}

void napi_module_register_by_symbol(v8::Local<v8::Object> exports,
                                    v8::Local<v8::Value> module,
                                    v8::Local<v8::Context> context,
                                    napi_addon_register_func init) {
  if (init == nullptr) {
    node::Environment* node_env = node::Environment::GetCurrent(context);
    CHECK_NOT_NULL(node_env);
    node_env->ThrowError(
        "Module has no declared entry point.");
    return;
  }

  // Create a new napi_env for this specific module.
  napi_env env = v8impl::NewEnv(context);

  napi_value _exports;
  env->CallIntoModuleThrow([&](napi_env env) {
    _exports = init(env, v8impl::JsValueFromV8LocalValue(exports));
  });

  // If register function returned a non-null exports object different from
  // the exports object we passed it, set that as the "exports" property of
  // the module.
  if (_exports != nullptr &&
      _exports != v8impl::JsValueFromV8LocalValue(exports)) {
    napi_value _module = v8impl::JsValueFromV8LocalValue(module);
    napi_set_named_property(env, _module, "exports", _exports);
  }
}

// Registers a NAPI module.
void napi_module_register(napi_module* mod) {
  node::node_module* nm = new node::node_module {
    -1,
    mod->nm_flags | NM_F_DELETEME,
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

napi_status napi_add_env_cleanup_hook(napi_env env,
                                      void (*fun)(void* arg),
                                      void* arg) {
  CHECK_ENV(env);
  CHECK_ARG(env, fun);

  node::AddEnvironmentCleanupHook(env->isolate, fun, arg);

  return napi_ok;
}

napi_status napi_remove_env_cleanup_hook(napi_env env,
                                         void (*fun)(void* arg),
                                         void* arg) {
  CHECK_ENV(env);
  CHECK_ARG(env, fun);

  node::RemoveEnvironmentCleanupHook(env->isolate, fun, arg);

  return napi_ok;
}

napi_status napi_fatal_exception(napi_env env, napi_value err) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, err);

  v8::Local<v8::Value> local_err = v8impl::V8LocalValueFromJsValue(err);
  v8impl::trigger_fatal_exception(env, local_err);

  return napi_clear_last_error(env);
}

NAPI_NO_RETURN void napi_fatal_error(const char* location,
                                     size_t location_len,
                                     const char* message,
                                     size_t message_len) {
  std::string location_string;
  std::string message_string;

  if (location_len != NAPI_AUTO_LENGTH) {
    location_string.assign(
        const_cast<char*>(location), location_len);
  } else {
    location_string.assign(
        const_cast<char*>(location), strlen(location));
  }

  if (message_len != NAPI_AUTO_LENGTH) {
    message_string.assign(
        const_cast<char*>(message), message_len);
  } else {
    message_string.assign(
        const_cast<char*>(message), strlen(message));
  }

  node::FatalError(location_string.c_str(), message_string.c_str());
}

napi_status napi_open_callback_scope(napi_env env,
                                     napi_value resource_object,
                                     napi_async_context async_context_handle,
                                     napi_callback_scope* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();

  node::async_context* node_async_context =
      reinterpret_cast<node::async_context*>(async_context_handle);

  v8::Local<v8::Object> resource;
  CHECK_TO_OBJECT(env, context, resource, resource_object);

  *result = v8impl::JsCallbackScopeFromV8CallbackScope(
      new node::CallbackScope(env->isolate,
                              resource,
                              *node_async_context));

  env->open_callback_scopes++;
  return napi_clear_last_error(env);
}

napi_status napi_close_callback_scope(napi_env env, napi_callback_scope scope) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ENV(env);
  CHECK_ARG(env, scope);
  if (env->open_callback_scopes == 0) {
    return napi_callback_scope_mismatch;
  }

  env->open_callback_scopes--;
  delete v8impl::V8CallbackScopeFromJsCallbackScope(scope);
  return napi_clear_last_error(env);
}

napi_status napi_async_init(napi_env env,
                            napi_value async_resource,
                            napi_value async_resource_name,
                            napi_async_context* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, async_resource_name);
  CHECK_ARG(env, result);

  v8::Isolate* isolate = env->isolate;
  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Object> v8_resource;
  if (async_resource != nullptr) {
    CHECK_TO_OBJECT(env, context, v8_resource, async_resource);
  } else {
    v8_resource = v8::Object::New(isolate);
  }

  v8::Local<v8::String> v8_resource_name;
  CHECK_TO_STRING(env, context, v8_resource_name, async_resource_name);

  // TODO(jasongin): Consider avoiding allocation here by using
  // a tagged pointer with 2×31 bit fields instead.
  node::async_context* async_context = new node::async_context();

  *async_context = node::EmitAsyncInit(isolate, v8_resource, v8_resource_name);
  *result = reinterpret_cast<napi_async_context>(async_context);

  return napi_clear_last_error(env);
}

napi_status napi_async_destroy(napi_env env,
                               napi_async_context async_context) {
  CHECK_ENV(env);
  CHECK_ARG(env, async_context);

  node::async_context* node_async_context =
      reinterpret_cast<node::async_context*>(async_context);
  node::EmitAsyncDestroy(
      reinterpret_cast<node_napi_env>(env)->node_env(),
      *node_async_context);

  delete node_async_context;

  return napi_clear_last_error(env);
}

napi_status napi_make_callback(napi_env env,
                               napi_async_context async_context,
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

  v8::Local<v8::Object> v8recv;
  CHECK_TO_OBJECT(env, context, v8recv, recv);

  v8::Local<v8::Function> v8func;
  CHECK_TO_FUNCTION(env, v8func, func);

  node::async_context* node_async_context =
    reinterpret_cast<node::async_context*>(async_context);
  if (node_async_context == nullptr) {
    static node::async_context empty_context = { 0, 0 };
    node_async_context = &empty_context;
  }

  v8::MaybeLocal<v8::Value> callback_result = node::MakeCallback(
      env->isolate, v8recv, v8func, argc,
      reinterpret_cast<v8::Local<v8::Value>*>(const_cast<napi_value*>(argv)),
      *node_async_context);

  if (try_catch.HasCaught()) {
    return napi_set_last_error(env, napi_pending_exception);
  } else {
    CHECK_MAYBE_EMPTY(env, callback_result, napi_generic_failure);
    if (result != nullptr) {
      *result = v8impl::JsValueFromV8LocalValue(
          callback_result.ToLocalChecked());
    }
  }

  return GET_RETURN_STATUS(env);
}

napi_status napi_create_buffer(napi_env env,
                               size_t length,
                               void** data,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  auto maybe = node::Buffer::New(env->isolate, length);

  CHECK_MAYBE_EMPTY(env, maybe, napi_generic_failure);

  v8::Local<v8::Object> buffer = maybe.ToLocalChecked();

  *result = v8impl::JsValueFromV8LocalValue(buffer);

  if (data != nullptr) {
    *data = node::Buffer::Data(buffer);
  }

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
      env, finalize_cb, nullptr, finalize_hint,
      v8impl::Finalizer::kKeepEnvReference);

  auto maybe = node::Buffer::New(isolate,
                                static_cast<char*>(data),
                                length,
                                v8impl::BufferFinalizer::FinalizeBufferCallback,
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
  CHECK_ENV(env);
  CHECK_ARG(env, value);
  CHECK_ARG(env, result);

  *result = node::Buffer::HasInstance(v8impl::V8LocalValueFromJsValue(value));
  return napi_clear_last_error(env);
}

napi_status napi_get_buffer_info(napi_env env,
                                 napi_value value,
                                 void** data,
                                 size_t* length) {
  CHECK_ENV(env);
  CHECK_ARG(env, value);

  v8::Local<v8::Value> buffer = v8impl::V8LocalValueFromJsValue(value);

  if (data != nullptr) {
    *data = node::Buffer::Data(buffer);
  }
  if (length != nullptr) {
    *length = node::Buffer::Length(buffer);
  }

  return napi_clear_last_error(env);
}

napi_status napi_get_node_version(napi_env env,
                                  const napi_node_version** result) {
  CHECK_ENV(env);
  CHECK_ARG(env, result);
  static const napi_node_version version = {
    NODE_MAJOR_VERSION,
    NODE_MINOR_VERSION,
    NODE_PATCH_VERSION,
    NODE_RELEASE
  };
  *result = &version;
  return napi_clear_last_error(env);
}

namespace {
namespace uvimpl {

static napi_status ConvertUVErrorCode(int code) {
  switch (code) {
    case 0:
      return napi_ok;
    case UV_EINVAL:
      return napi_invalid_arg;
    case UV_ECANCELED:
      return napi_cancelled;
    default:
      return napi_generic_failure;
  }
}

// Wrapper around uv_work_t which calls user-provided callbacks.
class Work : public node::AsyncResource, public node::ThreadPoolWork {
 private:
  explicit Work(node_napi_env env,
                v8::Local<v8::Object> async_resource,
                v8::Local<v8::String> async_resource_name,
                napi_async_execute_callback execute,
                napi_async_complete_callback complete = nullptr,
                void* data = nullptr)
    : AsyncResource(env->isolate,
                    async_resource,
                    *v8::String::Utf8Value(env->isolate, async_resource_name)),
      ThreadPoolWork(env->node_env()),
      _env(env),
      _data(data),
      _execute(execute),
      _complete(complete) {
  }

  ~Work() override = default;

 public:
  static Work* New(node_napi_env env,
                   v8::Local<v8::Object> async_resource,
                   v8::Local<v8::String> async_resource_name,
                   napi_async_execute_callback execute,
                   napi_async_complete_callback complete,
                   void* data) {
    return new Work(env, async_resource, async_resource_name,
                    execute, complete, data);
  }

  static void Delete(Work* work) {
    delete work;
  }

  void DoThreadPoolWork() override {
    _execute(_env, _data);
  }

  void AfterThreadPoolWork(int status) override {
    if (_complete == nullptr)
      return;

    // Establish a handle scope here so that every callback doesn't have to.
    // Also it is needed for the exception-handling below.
    v8::HandleScope scope(_env->isolate);

    CallbackScope callback_scope(this);

    _env->CallIntoModule([&](napi_env env) {
      _complete(env, ConvertUVErrorCode(status), _data);
    }, [](napi_env env, v8::Local<v8::Value> local_err) {
      // If there was an unhandled exception in the complete callback,
      // report it as a fatal exception. (There is no JavaScript on the
      // callstack that can possibly handle it.)
      v8impl::trigger_fatal_exception(env, local_err);
    });

    // Note: Don't access `work` after this point because it was
    // likely deleted by the complete callback.
  }

 private:
  node_napi_env _env;
  void* _data;
  napi_async_execute_callback _execute;
  napi_async_complete_callback _complete;
};

}  // end of namespace uvimpl
}  // end of anonymous namespace

#define CALL_UV(env, condition)                                         \
  do {                                                                  \
    int result = (condition);                                           \
    napi_status status = uvimpl::ConvertUVErrorCode(result);            \
    if (status != napi_ok) {                                            \
      return napi_set_last_error(env, status, result);                  \
    }                                                                   \
  } while (0)

napi_status napi_create_async_work(napi_env env,
                                   napi_value async_resource,
                                   napi_value async_resource_name,
                                   napi_async_execute_callback execute,
                                   napi_async_complete_callback complete,
                                   void* data,
                                   napi_async_work* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, execute);
  CHECK_ARG(env, result);

  v8::Local<v8::Context> context = env->context();

  v8::Local<v8::Object> resource;
  if (async_resource != nullptr) {
    CHECK_TO_OBJECT(env, context, resource, async_resource);
  } else {
    resource = v8::Object::New(env->isolate);
  }

  v8::Local<v8::String> resource_name;
  CHECK_TO_STRING(env, context, resource_name, async_resource_name);

  uvimpl::Work* work = uvimpl::Work::New(reinterpret_cast<node_napi_env>(env),
                                         resource,
                                         resource_name,
                                         execute,
                                         complete,
                                         data);

  *result = reinterpret_cast<napi_async_work>(work);

  return napi_clear_last_error(env);
}

napi_status napi_delete_async_work(napi_env env, napi_async_work work) {
  CHECK_ENV(env);
  CHECK_ARG(env, work);

  uvimpl::Work::Delete(reinterpret_cast<uvimpl::Work*>(work));

  return napi_clear_last_error(env);
}

napi_status napi_get_uv_event_loop(napi_env env, uv_loop_t** loop) {
  CHECK_ENV(env);
  CHECK_ARG(env, loop);
  *loop = reinterpret_cast<node_napi_env>(env)->node_env()->event_loop();
  return napi_clear_last_error(env);
}

napi_status napi_queue_async_work(napi_env env, napi_async_work work) {
  CHECK_ENV(env);
  CHECK_ARG(env, work);

  napi_status status;
  uv_loop_t* event_loop = nullptr;
  status = napi_get_uv_event_loop(env, &event_loop);
  if (status != napi_ok)
    return napi_set_last_error(env, status);

  uvimpl::Work* w = reinterpret_cast<uvimpl::Work*>(work);

  w->ScheduleWork();

  return napi_clear_last_error(env);
}

napi_status napi_cancel_async_work(napi_env env, napi_async_work work) {
  CHECK_ENV(env);
  CHECK_ARG(env, work);

  uvimpl::Work* w = reinterpret_cast<uvimpl::Work*>(work);

  CALL_UV(env, w->CancelWork());

  return napi_clear_last_error(env);
}

napi_status
napi_create_threadsafe_function(napi_env env,
                                napi_value func,
                                napi_value async_resource,
                                napi_value async_resource_name,
                                size_t max_queue_size,
                                size_t initial_thread_count,
                                void* thread_finalize_data,
                                napi_finalize thread_finalize_cb,
                                void* context,
                                napi_threadsafe_function_call_js call_js_cb,
                                napi_threadsafe_function* result) {
  CHECK_ENV(env);
  CHECK_ARG(env, async_resource_name);
  RETURN_STATUS_IF_FALSE(env, initial_thread_count > 0, napi_invalid_arg);
  CHECK_ARG(env, result);

  napi_status status = napi_ok;

  v8::Local<v8::Function> v8_func;
  if (func == nullptr) {
    CHECK_ARG(env, call_js_cb);
  } else {
    CHECK_TO_FUNCTION(env, v8_func, func);
  }

  v8::Local<v8::Context> v8_context = env->context();

  v8::Local<v8::Object> v8_resource;
  if (async_resource == nullptr) {
    v8_resource = v8::Object::New(env->isolate);
  } else {
    CHECK_TO_OBJECT(env, v8_context, v8_resource, async_resource);
  }

  v8::Local<v8::String> v8_name;
  CHECK_TO_STRING(env, v8_context, v8_name, async_resource_name);

  v8impl::ThreadSafeFunction* ts_fn =
      new v8impl::ThreadSafeFunction(v8_func,
                                     v8_resource,
                                     v8_name,
                                     initial_thread_count,
                                     context,
                                     max_queue_size,
                                     reinterpret_cast<node_napi_env>(env),
                                     thread_finalize_data,
                                     thread_finalize_cb,
                                     call_js_cb);

  if (ts_fn == nullptr) {
    status = napi_generic_failure;
  } else {
    // Init deletes ts_fn upon failure.
    status = ts_fn->Init();
    if (status == napi_ok) {
      *result = reinterpret_cast<napi_threadsafe_function>(ts_fn);
    }
  }

  return napi_set_last_error(env, status);
}

napi_status
napi_get_threadsafe_function_context(napi_threadsafe_function func,
                                     void** result) {
  CHECK_NOT_NULL(func);
  CHECK_NOT_NULL(result);

  *result = reinterpret_cast<v8impl::ThreadSafeFunction*>(func)->Context();
  return napi_ok;
}

napi_status
napi_call_threadsafe_function(napi_threadsafe_function func,
                              void* data,
                              napi_threadsafe_function_call_mode is_blocking) {
  CHECK_NOT_NULL(func);
  return reinterpret_cast<v8impl::ThreadSafeFunction*>(func)->Push(data,
                                                                   is_blocking);
}

napi_status
napi_acquire_threadsafe_function(napi_threadsafe_function func) {
  CHECK_NOT_NULL(func);
  return reinterpret_cast<v8impl::ThreadSafeFunction*>(func)->Acquire();
}

napi_status
napi_release_threadsafe_function(napi_threadsafe_function func,
                                 napi_threadsafe_function_release_mode mode) {
  CHECK_NOT_NULL(func);
  return reinterpret_cast<v8impl::ThreadSafeFunction*>(func)->Release(mode);
}

napi_status
napi_unref_threadsafe_function(napi_env env, napi_threadsafe_function func) {
  CHECK_NOT_NULL(func);
  return reinterpret_cast<v8impl::ThreadSafeFunction*>(func)->Unref();
}

napi_status
napi_ref_threadsafe_function(napi_env env, napi_threadsafe_function func) {
  CHECK_NOT_NULL(func);
  return reinterpret_cast<v8impl::ThreadSafeFunction*>(func)->Ref();
}
