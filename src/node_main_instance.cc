#include "node_main_instance.h"
#include <memory>
#if HAVE_OPENSSL
#include "crypto/crypto_util.h"
#endif  // HAVE_OPENSSL
#include "debug_utils-inl.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_native_module_env.h"
#include "node_options-inl.h"
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

  isolate_ = Isolate::Allocate();
  CHECK_NOT_NULL(isolate_);
  // Register the isolate on the platform before the isolate gets initialized,
  // so that the isolate can access the platform during initialization.
  platform->RegisterIsolate(isolate_, event_loop);
  SetIsolateCreateParamsForNode(isolate_params_.get());
  Isolate::Initialize(isolate_, *isolate_params_);

  // If the indexes are not nullptr, we are not deserializing
  isolate_data_ = std::make_unique<IsolateData>(
      isolate_,
      event_loop,
      platform,
      array_buffer_allocator_.get(),
      snapshot_data == nullptr ? nullptr
                               : &(snapshot_data->isolate_data_indices));
  IsolateSettings s;
  SetIsolateMiscHandlers(isolate_, s);
  if (snapshot_data == nullptr) {
    // If in deserialize mode, delay until after the deserialization is
    // complete.
    SetIsolateErrorHandlers(isolate_, s);
  }
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
  // This should only be done on a main instance that owns its isolate.
  platform_->UnregisterIsolate(isolate_);
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

  ResetStdio();

  // TODO(addaleax): Neither NODE_SHARED_MODE nor HAVE_INSPECTOR really
  // make sense here.
#if HAVE_INSPECTOR && defined(__POSIX__) && !defined(NODE_SHARED_MODE)
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  for (unsigned nr = 1; nr < kMaxSignal; nr += 1) {
    if (nr == SIGKILL || nr == SIGSTOP || nr == SIGPROF)
      continue;
    act.sa_handler = (nr == SIGPIPE) ? SIG_IGN : SIG_DFL;
    CHECK_EQ(0, sigaction(nr, &act, nullptr));
  }
#endif

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
    env->DoneBootstrapping();

#if HAVE_OPENSSL
    crypto::InitCryptoOnce(isolate_);
#endif  // HAVE_OPENSSL
  } else {
    context = NewContext(isolate_);
    CHECK(!context.IsEmpty());
    Context::Scope context_scope(context);
    env.reset(new Environment(isolate_data_.get(),
                              context,
                              args_,
                              exec_args_,
                              nullptr,
                              EnvironmentFlags::kDefaultFlags,
                              {}));
#if HAVE_INSPECTOR
    env->InitializeInspector({});
#endif
    if (env->RunBootstrapping().IsEmpty()) {
      return nullptr;
    }
  }

  return env;
}

}  // namespace node
