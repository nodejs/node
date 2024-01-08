#include "node_main_instance.h"
#include <memory>
#if HAVE_OPENSSL
#include "crypto/crypto_util.h"
#endif  // HAVE_OPENSSL
#include "debug_utils-inl.h"
#include "node_builtins.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_options-inl.h"
#include "node_realm.h"
#include "node_snapshot_builder.h"
#include "node_snapshotable.h"
#include "node_v8_platform-inl.h"
#include "util-inl.h"
#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

#if HAVE_INSPECTOR
#include "inspector/worker_inspector.h"  // ParentInspectorHandle
#endif

namespace node {

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Locker;

NodeMainInstance::NodeMainInstance(Isolate* isolate,
                                   uv_loop_t* event_loop,
                                   MultiIsolatePlatform* platform,
                                   const std::vector<std::string>& args,
                                   const std::vector<std::string>& exec_args)
    : args_(args),
      exec_args_(exec_args),
      array_buffer_allocator_(nullptr),
      isolate_(isolate),
      platform_(platform),
      isolate_data_(nullptr),
      snapshot_data_(nullptr) {
  isolate_data_ =
      std::make_unique<IsolateData>(isolate_, event_loop, platform, nullptr);

  SetIsolateMiscHandlers(isolate_, {});
}

std::unique_ptr<NodeMainInstance> NodeMainInstance::Create(
    Isolate* isolate,
    uv_loop_t* event_loop,
    MultiIsolatePlatform* platform,
    const std::vector<std::string>& args,
    const std::vector<std::string>& exec_args) {
  return std::unique_ptr<NodeMainInstance>(
      new NodeMainInstance(isolate, event_loop, platform, args, exec_args));
}

NodeMainInstance::NodeMainInstance(const SnapshotData* snapshot_data,
                                   uv_loop_t* event_loop,
                                   MultiIsolatePlatform* platform,
                                   const std::vector<std::string>& args,
                                   const std::vector<std::string>& exec_args)
    : args_(args),
      exec_args_(exec_args),
      array_buffer_allocator_(ArrayBufferAllocator::Create()),
      isolate_(nullptr),
      platform_(platform),
      isolate_data_(),
      isolate_params_(std::make_unique<Isolate::CreateParams>()),
      snapshot_data_(snapshot_data) {
  isolate_params_->array_buffer_allocator = array_buffer_allocator_.get();
  if (snapshot_data != nullptr) {
    SnapshotBuilder::InitializeIsolateParams(snapshot_data,
                                             isolate_params_.get());
  }

  isolate_ = NewIsolate(
      isolate_params_.get(), event_loop, platform, snapshot_data != nullptr);
  CHECK_NOT_NULL(isolate_);

  // If the indexes are not nullptr, we are not deserializing
  isolate_data_ = std::make_unique<IsolateData>(
      isolate_,
      event_loop,
      platform,
      array_buffer_allocator_.get(),
      snapshot_data == nullptr ? nullptr : &(snapshot_data->isolate_data_info));

  isolate_data_->max_young_gen_size =
      isolate_params_->constraints.max_young_generation_size_in_bytes();
}

void NodeMainInstance::Dispose() {
  // This should only be called on a main instance that does not own its
  // isolate.
  CHECK_NULL(isolate_params_);
  platform_->DrainTasks(isolate_);
}

NodeMainInstance::~NodeMainInstance() {
  if (isolate_params_ == nullptr) {
    return;
  }

  {
#ifdef DEBUG
    // node::Environment has been disposed and no JavaScript Execution is
    // allowed at this point.
    // Create a scope to check that no JavaScript is executed in debug build
    // and proactively crash the process in the case JavaScript is being
    // executed.
    // Isolate::Dispose() must be invoked outside of this scope to avoid
    // use-after-free.
    Isolate::DisallowJavascriptExecutionScope disallow_js(
        isolate_, Isolate::DisallowJavascriptExecutionScope::CRASH_ON_FAILURE);
#endif
    // This should only be done on a main instance that owns its isolate.
    // IsolateData must be freed before UnregisterIsolate() is called.
    isolate_data_.reset();
    platform_->UnregisterIsolate(isolate_);
  }
  isolate_->Dispose();
}

int NodeMainInstance::Run() {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope handle_scope(isolate_);

  int exit_code = 0;
  DeleteFnPtr<Environment, FreeEnvironment> env =
      CreateMainEnvironment(&exit_code);
  CHECK_NOT_NULL(env);

  Context::Scope context_scope(env->context());
  Run(&exit_code, env.get());
  return exit_code;
}

void NodeMainInstance::Run(int* exit_code, Environment* env) {
  if (*exit_code == 0) {
    LoadEnvironment(env, StartExecutionCallback{});

    *exit_code = SpinEventLoop(env).FromMaybe(1);
  }

#if defined(LEAK_SANITIZER)
  __lsan_do_leak_check();
#endif
}

DeleteFnPtr<Environment, FreeEnvironment>
NodeMainInstance::CreateMainEnvironment(int* exit_code) {
  *exit_code = 0;  // Reset the exit code to 0

  HandleScope handle_scope(isolate_);

  // TODO(addaleax): This should load a real per-Isolate option, currently
  // this is still effectively per-process.
  if (isolate_data_->options()->track_heap_objects) {
    isolate_->GetHeapProfiler()->StartTrackingHeapObjects(true);
  }

  Local<Context> context;
  DeleteFnPtr<Environment, FreeEnvironment> env;

  if (snapshot_data_ != nullptr) {
    env.reset(new Environment(isolate_data_.get(),
                              isolate_,
                              args_,
                              exec_args_,
                              &(snapshot_data_->env_info),
                              EnvironmentFlags::kDefaultFlags,
                              {}));
#ifdef NODE_V8_SHARED_RO_HEAP
    // TODO(addaleax): Do this as part of creating the Environment
    // once we store the SnapshotData* itself on IsolateData.
    env->builtin_loader()->RefreshCodeCache(snapshot_data_->code_cache);
#endif
    context = Context::FromSnapshot(isolate_,
                                    SnapshotData::kNodeMainContextIndex,
                                    {DeserializeNodeInternalFields, env.get()})
                  .ToLocalChecked();

    CHECK(!context.IsEmpty());
    Context::Scope context_scope(context);

    CHECK(InitializeContextRuntime(context).IsJust());
    SetIsolateErrorHandlers(isolate_, {});
    env->InitializeMainContext(context, &(snapshot_data_->env_info));
#if HAVE_INSPECTOR
    env->InitializeInspector({});
#endif

#if HAVE_OPENSSL
    crypto::InitCryptoOnce(isolate_);
#endif  // HAVE_OPENSSL
  } else {
    context = NewContext(isolate_);
    CHECK(!context.IsEmpty());
    Context::Scope context_scope(context);
    env.reset(
        CreateEnvironment(isolate_data_.get(), context, args_, exec_args_));
  }

  return env;
}

}  // namespace node
