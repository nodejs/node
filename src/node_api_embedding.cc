#include <algorithm>
#include <climits>  // INT_MAX
#include <cmath>
#define NAPI_EXPERIMENTAL
#include "env-inl.h"
#include "js_native_api.h"
#include "js_native_api_v8.h"
#include "node_api_embedding.h"
#include "node_api_internals.h"
#include "simdutf.h"
#include "util-inl.h"

namespace v8impl {
namespace {

class EmbeddedEnvironment : public node::EmbeddedEnvironment {
 public:
  explicit EmbeddedEnvironment(
      std::unique_ptr<node::CommonEnvironmentSetup>&& setup,
      const std::shared_ptr<node::StaticExternalTwoByteResource>& main_resource)
      : main_resource_(main_resource),
        setup_(std::move(setup)),
        locker_(setup_->isolate()),
        isolate_scope_(setup_->isolate()),
        handle_scope_(setup_->isolate()),
        context_scope_(setup_->context()),
        seal_scope_(nullptr) {}

  inline node::CommonEnvironmentSetup* setup() { return setup_.get(); }
  inline void seal() {
    seal_scope_ =
        std::make_unique<node::DebugSealHandleScope>(setup_->isolate());
  }

 private:
  // The pointer to the UTF-16 main script convertible to V8 UnionBytes resource
  // This must be constructed first and destroyed last because the isolate
  // references it
  std::shared_ptr<node::StaticExternalTwoByteResource> main_resource_;

  std::unique_ptr<node::CommonEnvironmentSetup> setup_;
  v8::Locker locker_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;
  // As this handle scope will remain open for the lifetime
  // of the environment, we seal it to prevent it from
  // becoming everyone's favorite trash bin
  std::unique_ptr<node::DebugSealHandleScope> seal_scope_;
};

}  // end of anonymous namespace
}  // end of namespace v8impl

napi_status NAPI_CDECL
napi_create_platform(int argc,
                     char** argv,
                     napi_error_message_handler err_handler,
                     napi_platform* result) {
  argv = uv_setup_args(argc, argv);
  std::vector<std::string> args(argv, argv + argc);
  if (args.size() < 1) args.push_back("libnode");

  std::unique_ptr<node::InitializationResult> node_platform =
      node::InitializeOncePerProcess(
          args,
          {node::ProcessInitializationFlags::kNoInitializeV8,
           node::ProcessInitializationFlags::kNoInitializeNodeV8Platform});

  for (const std::string& error : node_platform->errors()) {
    if (err_handler != nullptr) {
      err_handler(error.c_str());
    } else {
      fprintf(stderr, "%s\n", error.c_str());
    }
  }
  if (node_platform->early_return() != 0) {
    return napi_generic_failure;
  }

  int thread_pool_size =
      static_cast<int>(node::per_process::cli_options->v8_thread_pool_size);
  std::unique_ptr<node::MultiIsolatePlatform> v8_platform =
      node::MultiIsolatePlatform::Create(thread_pool_size);
  v8::V8::InitializePlatform(v8_platform.get());
  v8::V8::Initialize();
  reinterpret_cast<node::InitializationResultImpl*>(node_platform.get())
      ->platform_ = v8_platform.release();
  *result = reinterpret_cast<napi_platform>(node_platform.release());
  return napi_ok;
}

napi_status NAPI_CDECL napi_destroy_platform(napi_platform platform) {
  auto wrapper = reinterpret_cast<node::InitializationResult*>(platform);
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  node::TearDownOncePerProcess();
  delete wrapper->platform();
  delete wrapper;
  return napi_ok;
}

napi_status NAPI_CDECL
napi_create_environment(napi_platform platform,
                        napi_error_message_handler err_handler,
                        const char* main_script,
                        int32_t api_version,
                        napi_env* result) {
  auto wrapper = reinterpret_cast<node::InitializationResult*>(platform);
  std::vector<std::string> errors_vec;

  auto setup = node::CommonEnvironmentSetup::Create(
      wrapper->platform(), &errors_vec, wrapper->args(), wrapper->exec_args());

  for (const std::string& error : errors_vec) {
    if (err_handler != nullptr) {
      err_handler(error.c_str());
    } else {
      fprintf(stderr, "%s\n", error.c_str());
    }
  }
  if (setup == nullptr) {
    return napi_generic_failure;
  }

  std::shared_ptr<node::StaticExternalTwoByteResource> main_resource = nullptr;
  if (main_script != nullptr) {
    // We convert the user-supplied main_script to a UTF-16 resource
    // and we store its shared_ptr in the environment
    size_t u8_length = strlen(main_script);
    size_t expected_u16_length =
        simdutf::utf16_length_from_utf8(main_script, u8_length);
    auto out = std::make_shared<std::vector<uint16_t>>(expected_u16_length);
    size_t u16_length = simdutf::convert_utf8_to_utf16(
        main_script, u8_length, reinterpret_cast<char16_t*>(out->data()));
    out->resize(u16_length);
    main_resource = std::make_shared<node::StaticExternalTwoByteResource>(
        out->data(), out->size(), out);
  }

  auto emb_env =
      new v8impl::EmbeddedEnvironment(std::move(setup), main_resource);

  std::string filename =
      wrapper->args().size() > 1 ? wrapper->args()[1] : "<internal>";
  auto env__ =
      new node_napi_env__(emb_env->setup()->context(), filename, api_version);
  emb_env->setup()->env()->set_embedded(emb_env);
  env__->node_env()->AddCleanupHook(
      [](void* arg) { static_cast<napi_env>(arg)->Unref(); },
      static_cast<void*>(env__));
  *result = env__;

  auto env = emb_env->setup()->env();

  auto ret = node::LoadEnvironment(
      env,
      [env, resource = main_resource.get()](
          const node::StartExecutionCallbackInfo& info)
          -> v8::MaybeLocal<v8::Value> {
        node::Realm* realm = env->principal_realm();
        auto ret = realm->ExecuteBootstrapper(
            "internal/bootstrap/switches/is_embedded_env");
        if (ret.IsEmpty()) return ret;

        std::string name =
            "embedder_main_napi_" + std::to_string(env->thread_id());
        if (resource != nullptr) {
          env->builtin_loader()->Add(name.c_str(), node::UnionBytes(resource));
          return realm->ExecuteBootstrapper(name.c_str());
        } else {
          return v8::Undefined(env->isolate());
        }
      });
  if (ret.IsEmpty()) return napi_pending_exception;

  emb_env->seal();

  return napi_ok;
}

napi_status NAPI_CDECL napi_destroy_environment(napi_env env, int* exit_code) {
  CHECK_ARG(env, env);
  node_napi_env node_env = reinterpret_cast<node_napi_env>(env);

  int r = node::SpinEventLoop(node_env->node_env()).FromMaybe(1);
  if (exit_code != nullptr) *exit_code = r;
  node::Stop(node_env->node_env());

  auto emb_env = reinterpret_cast<v8impl::EmbeddedEnvironment*>(
      node_env->node_env()->get_embedded());
  node_env->node_env()->set_embedded(nullptr);
  // This deletes the uniq_ptr to node::CommonEnvironmentSetup
  // and the v8::locker
  delete emb_env;

  cppgc::ShutdownProcess();

  return napi_ok;
}

napi_status NAPI_CDECL napi_run_environment(napi_env env) {
  CHECK_ARG(env, env);
  node_napi_env node_env = reinterpret_cast<node_napi_env>(env);

  if (node::SpinEventLoopWithoutCleanup(node_env->node_env()).IsNothing())
    return napi_closing;

  return napi_ok;
}

static void napi_promise_error_handler(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return;
}

napi_status napi_await_promise(napi_env env,
                               napi_value promise,
                               napi_value* result) {
  NAPI_PREAMBLE(env);
  CHECK_ARG(env, result);

  v8::EscapableHandleScope scope(env->isolate);
  node_napi_env node_env = reinterpret_cast<node_napi_env>(env);

  v8::Local<v8::Value> promise_value = v8impl::V8LocalValueFromJsValue(promise);
  if (promise_value.IsEmpty() || !promise_value->IsPromise())
    return napi_invalid_arg;
  v8::Local<v8::Promise> promise_object = promise_value.As<v8::Promise>();

  v8::Local<v8::Value> rejected = v8::Boolean::New(env->isolate, false);
  v8::Local<v8::Function> err_handler =
      v8::Function::New(env->context(), napi_promise_error_handler, rejected)
          .ToLocalChecked();

  if (promise_object->Catch(env->context(), err_handler).IsEmpty())
    return napi_pending_exception;

  if (node::SpinEventLoopWithoutCleanup(
          node_env->node_env(),
          [&promise_object]() {
            return promise_object->State() ==
                   v8::Promise::PromiseState::kPending;
          })
          .IsNothing())
    return napi_closing;

  *result =
      v8impl::JsValueFromV8LocalValue(scope.Escape(promise_object->Result()));
  if (promise_object->State() == v8::Promise::PromiseState::kRejected)
    return napi_pending_exception;

  return napi_ok;
}
