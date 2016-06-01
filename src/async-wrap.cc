#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"

#include "v8.h"
#include "v8-profiler.h"

using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::HeapProfiler;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::RetainedObjectInfo;
using v8::TryCatch;
using v8::Value;

namespace node {

static const char* const provider_names[] = {
#define V(PROVIDER)                                                           \
  #PROVIDER,
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
};


class RetainedAsyncInfo: public RetainedObjectInfo {
 public:
  explicit RetainedAsyncInfo(uint16_t class_id, AsyncWrap* wrap);

  virtual void Dispose() override;
  virtual bool IsEquivalent(RetainedObjectInfo* other) override;
  virtual intptr_t GetHash() override;
  virtual const char* GetLabel() override;
  virtual intptr_t GetSizeInBytes() override;

 private:
  const char* label_;
  const AsyncWrap* wrap_;
  const int length_;
};


RetainedAsyncInfo::RetainedAsyncInfo(uint16_t class_id, AsyncWrap* wrap)
    : label_(provider_names[class_id - NODE_ASYNC_ID_OFFSET]),
      wrap_(wrap),
      length_(wrap->self_size()) {
}


void RetainedAsyncInfo::Dispose() {
  delete this;
}


bool RetainedAsyncInfo::IsEquivalent(RetainedObjectInfo* other) {
  return label_ == other->GetLabel() &&
          wrap_ == static_cast<RetainedAsyncInfo*>(other)->wrap_;
}


intptr_t RetainedAsyncInfo::GetHash() {
  return reinterpret_cast<intptr_t>(wrap_);
}


const char* RetainedAsyncInfo::GetLabel() {
  return label_;
}


intptr_t RetainedAsyncInfo::GetSizeInBytes() {
  return length_;
}


RetainedObjectInfo* WrapperInfo(uint16_t class_id, Local<Value> wrapper) {
  // No class_id should be the provider type of NONE.
  CHECK_NE(NODE_ASYNC_ID_OFFSET, class_id);
  CHECK(wrapper->IsObject());
  CHECK(!wrapper.IsEmpty());

  Local<Object> object = wrapper.As<Object>();
  CHECK_GT(object->InternalFieldCount(), 0);

  AsyncWrap* wrap = Unwrap<AsyncWrap>(object);
  CHECK_NE(nullptr, wrap);

  return new RetainedAsyncInfo(class_id, wrap);
}


// end RetainedAsyncInfo


static void EnableHooksJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Function> init_fn = env->async_hooks_init_function();
  if (init_fn.IsEmpty() || !init_fn->IsFunction())
    return env->ThrowTypeError("init callback is not assigned to a function");
  env->async_hooks()->set_enable_callbacks(1);
}


static void DisableHooksJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->async_hooks()->set_enable_callbacks(0);
}

static void GetCurrentAsyncIdArrayFromJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(env->async_hooks()->get_current_async_id_array());
}

static void GetNextAsyncIdArrayFromJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(env->async_hooks()->get_next_async_id_array());
}

static void GetAsyncHookFieldsFromJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(env->async_hooks()->get_fields_array());
}

static bool FireAsyncInitCallbacksInternal(
  Environment* env,
  int64_t uid,
  v8::Local<v8::Object> object,
  int64_t parentUid,
  v8::Local<v8::Object> parentObject,
  AsyncWrap::ProviderType provider,
  AsyncWrap* parent)
{
  v8::Local<v8::Function> init_fn = env->async_hooks_init_function();
  bool didRun = false;

  // No init callback exists, no reason to go on.
  if (!init_fn.IsEmpty()) {

    // If async wrap callbacks are disabled and no parent was passed that has
    // run the init callback then return.
    if (!env->async_wrap_callbacks_enabled() &&
      (parent == nullptr || !parent->ran_init_callback())) {
      return false;
    }

    v8::HandleScope scope(env->isolate());

    v8::Local<v8::Value> argv[] = {
        v8::Integer::New(env->isolate(), uid),
        v8::Int32::New(env->isolate(), provider),
        Null(env->isolate()),
        Null(env->isolate())
    };

    if (!parentObject.IsEmpty() && !parentObject->IsUndefined()) {
      argv[2] = v8::Integer::New(env->isolate(), parentUid);
      argv[3] = parentObject;
    }

    v8::TryCatch try_catch(env->isolate());

    v8::MaybeLocal<v8::Value> ret =
      init_fn->Call(env->context(), object, arraysize(argv), argv);
    didRun = true;

    if (ret.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
    }
  }

  return didRun;
}

bool AsyncWrap::FireAsyncInitCallbacks(
  Environment* env,
  int64_t uid,
  v8::Local<v8::Object> object,
  AsyncWrap::ProviderType provider,
  AsyncWrap* parent)
{
  v8::Local<v8::Function> init_fn = env->async_hooks_init_function();
  if (!init_fn.IsEmpty()) {

    int64_t parentUid = 0;
    v8::Local<v8::Object> parentObject = v8::Local<v8::Object>();

    if (parent != nullptr) {
      parentUid = parent->get_uid();
      parentObject = parent->object();
    }

    return FireAsyncInitCallbacksInternal(
      env,
      uid,
      object,
      parentUid,
      parentObject,
      provider,
      parent);
  }
  return false;
}

void AsyncWrap::FireAsyncPreCallbacks(
  Environment* env,
  bool ranInitCallbacks,
  v8::Local<v8::Number> uid,
  v8::Local<v8::Object> obj)
{
  env->async_hooks()->set_current_async_wrap_uid(uid->IntegerValue());

  if (ranInitCallbacks) {
    Local<Function> pre_fn = env->async_hooks_pre_function();
    if (!pre_fn.IsEmpty()) {
      TryCatch try_catch(env->isolate());
      v8::Local<v8::Value> argv[] = { uid };
      MaybeLocal<Value> result = pre_fn->Call(env->context(), obj, 1, argv);
      if (result.IsEmpty()) {
        ClearFatalExceptionHandlers(env);
        FatalException(env->isolate(), try_catch);
      }
    }
  }
}

void AsyncWrap::FireAsyncPostCallbacks(Environment* env, bool ranInitCallback, v8::Local<v8::Number> uid, v8::Local<v8::Object> obj, v8::Local<v8::Boolean> didUserCodeThrow) {

  if (ranInitCallback) {
    Local<Function> post_fn = env->async_hooks_post_function();
    if (!post_fn.IsEmpty()) {
      Local<Value> vals[] = { uid, didUserCodeThrow };
      TryCatch try_catch(env->isolate());
      MaybeLocal<Value> ar =
        post_fn->Call(env->context(), obj, arraysize(vals), vals);
      if (ar.IsEmpty()) {
        ClearFatalExceptionHandlers(env);
        FatalException(env->isolate(), try_catch);
      }
    }
  }

  env->async_hooks()->set_current_async_wrap_uid(0);
}

void AsyncWrap::FireAsyncDestroyCallbacks(Environment* env, bool ranInitCallbacks, v8::Local<v8::Number> uid) {

  if (ranInitCallbacks) {
    v8::Local<v8::Function> fn = env->async_hooks_destroy_function();
    if (!fn.IsEmpty()) {
      v8::HandleScope scope(env->isolate());
      v8::TryCatch try_catch(env->isolate());
      Local<Value> argv[] = { uid };
      v8::MaybeLocal<v8::Value> ret =
        fn->Call(env->context(), v8::Null(env->isolate()), arraysize(argv), argv);
      if (ret.IsEmpty()) {
        ClearFatalExceptionHandlers(env);
        FatalException(env->isolate(), try_catch);
      }
    }
  }

}

static void SetupHooks(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (env->async_hooks()->callbacks_enabled())
    return env->ThrowError("hooks should not be set while also enabled");
  if (!args[0]->IsObject())
    return env->ThrowTypeError("first argument must be an object");

  Local<Object> fn_obj = args[0].As<Object>();

  Local<Value> init_v = fn_obj->Get(
    env->context(),
    FIXED_ONE_BYTE_STRING(env->isolate(), "init")).ToLocalChecked();
  Local<Value> pre_v = fn_obj->Get(
    env->context(),
    FIXED_ONE_BYTE_STRING(env->isolate(), "pre")).ToLocalChecked();
  Local<Value> post_v = fn_obj->Get(
    env->context(),
    FIXED_ONE_BYTE_STRING(env->isolate(), "post")).ToLocalChecked();
  Local<Value> destroy_v = fn_obj->Get(
    env->context(),
    FIXED_ONE_BYTE_STRING(env->isolate(), "destroy")).ToLocalChecked();

  if (!init_v->IsFunction())
    return env->ThrowTypeError("init callback must be a function");

  env->set_async_hooks_init_function(init_v.As<Function>());

  if (pre_v->IsFunction())
    env->set_async_hooks_pre_function(pre_v.As<Function>());
  if (post_v->IsFunction())
    env->set_async_hooks_post_function(post_v.As<Function>());
  if (destroy_v->IsFunction())
    env->set_async_hooks_destroy_function(destroy_v.As<Function>());

  env->async_hooks_callbacks_objects()->Set(0, fn_obj);
}

static void GetAsyncCallbacksFromJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(env->async_hooks_callbacks_objects());
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);

  env->SetMethod(target, "setupHooks", SetupHooks);
  env->SetMethod(target, "disable", DisableHooksJS);
  env->SetMethod(target, "enable", EnableHooksJS);
  env->SetMethod(target, "getCurrentAsyncIdArray", GetCurrentAsyncIdArrayFromJS);
  env->SetMethod(target, "getNextAsyncIdArray", GetNextAsyncIdArrayFromJS);
  env->SetMethod(target, "getAsyncHookFields", GetAsyncHookFieldsFromJS);
  env->SetMethod(target, "getAsyncCallbacks", GetAsyncCallbacksFromJS);

  Local<Object> async_providers = Object::New(isolate);
#define V(PROVIDER)                                                           \
  async_providers->Set(FIXED_ONE_BYTE_STRING(isolate, #PROVIDER),             \
      Integer::New(isolate, AsyncWrap::PROVIDER_ ## PROVIDER));
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
  target->Set(FIXED_ONE_BYTE_STRING(isolate, "Providers"), async_providers);

  env->set_async_hooks_init_function(Local<Function>());
  env->set_async_hooks_pre_function(Local<Function>());
  env->set_async_hooks_post_function(Local<Function>());
  env->set_async_hooks_destroy_function(Local<Function>());
  env->set_async_hooks_callbacks_objects(v8::Array::New(env->isolate(), 0));
}


void LoadAsyncWrapperInfo(Environment* env) {
  HeapProfiler* heap_profiler = env->isolate()->GetHeapProfiler();
#define V(PROVIDER)                                                           \
  heap_profiler->SetWrapperClassInfoProvider(                                 \
      (NODE_ASYNC_ID_OFFSET + AsyncWrap::PROVIDER_ ## PROVIDER), WrapperInfo);
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
}


Local<Value> AsyncWrap::MakeCallback(const Local<Function> cb,
                                     int argc,
                                     Local<Value>* argv) {
  CHECK(env()->context() == env()->isolate()->GetCurrentContext());

  Local<Function> pre_fn = env()->async_hooks_pre_function();
  Local<Function> post_fn = env()->async_hooks_post_function();
  Local<Number> uid = Integer::New(env()->isolate(), get_uid());
  Local<Object> context = object();
  Local<Object> domain;
  bool has_domain = false;

  Environment::AsyncCallbackScope callback_scope(env());

  if (env()->using_domains()) {
    Local<Value> domain_v = context->Get(env()->domain_string());
    has_domain = domain_v->IsObject();
    if (has_domain) {
      domain = domain_v.As<Object>();
      if (domain->Get(env()->disposed_string())->IsTrue())
        return Local<Value>();
    }
  }

  if (has_domain) {
    Local<Value> enter_v = domain->Get(env()->enter_string());
    if (enter_v->IsFunction()) {
      if (enter_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("node::AsyncWrap::MakeCallback",
                   "domain enter callback threw, please report this");
      }
    }
  }

  AsyncWrap::FireAsyncPreCallbacks(env(), ran_init_callback(), uid, context);

  Local<Value> ret = cb->Call(context, argc, argv);

  Local<Boolean> did_throw = Boolean::New(env()->isolate(), ret.IsEmpty());
  AsyncWrap::FireAsyncPostCallbacks(env(), ran_init_callback(), uid, context, did_throw);
  
  if (ret.IsEmpty()) {
    return ret;
  }

  if (has_domain) {
    Local<Value> exit_v = domain->Get(env()->exit_string());
    if (exit_v->IsFunction()) {
      if (exit_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("node::AsyncWrap::MakeCallback",
                   "domain exit callback threw, please report this");
      }
    }
  }

  if (callback_scope.in_makecallback()) {
    return ret;
  }

  Environment::TickInfo* tick_info = env()->tick_info();

  if (tick_info->length() == 0) {
    env()->isolate()->RunMicrotasks();
  }

  Local<Object> process = env()->process_object();

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return ret;
  }

  if (env()->tick_callback_function()->Call(process, 0, nullptr).IsEmpty()) {
    return Local<Value>();
  }

  return ret;
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(async_wrap, node::Initialize)
