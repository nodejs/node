#include "node_main_instance.h"
#include <memory>
#include "debug_utils-inl.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_options-inl.h"
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

std::unique_ptr<ExternalReferenceRegistry> NodeMainInstance::registry_ =
    nullptr;
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
      owns_isolate_(false),
      deserialize_mode_(false) {
  isolate_data_ =
      std::make_unique<IsolateData>(isolate_, event_loop, platform, nullptr);

  SetIsolateMiscHandlers(isolate_, {});
}

const std::vector<intptr_t>& NodeMainInstance::CollectExternalReferences() {
  // Cannot be called more than once.
  CHECK_NULL(registry_);
  registry_.reset(new ExternalReferenceRegistry());
  return registry_->external_references();
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

NodeMainInstance::NodeMainInstance(
    Isolate::CreateParams* params,
    uv_loop_t* event_loop,
    MultiIsolatePlatform* platform,
    const std::vector<std::string>& args,
    const std::vector<std::string>& exec_args,
    const std::vector<size_t>* per_isolate_data_indexes)
    : args_(args),
      exec_args_(exec_args),
      array_buffer_allocator_(ArrayBufferAllocator::Create()),
      isolate_(nullptr),
      platform_(platform),
      isolate_data_(nullptr),
      owns_isolate_(true) {
  params->array_buffer_allocator = array_buffer_allocator_.get();
  deserialize_mode_ = per_isolate_data_indexes != nullptr;
  if (deserialize_mode_) {
    // TODO(joyeecheung): collect external references and set it in
    // params.external_references.
    const std::vector<intptr_t>& external_references =
        CollectExternalReferences();
    params->external_references = external_references.data();
  }

  isolate_ = Isolate::Allocate();
  CHECK_NOT_NULL(isolate_);
  // Register the isolate on the platform before the isolate gets initialized,
  // so that the isolate can access the platform during initialization.
  platform->RegisterIsolate(isolate_, event_loop);
  SetIsolateCreateParamsForNode(params);
  Isolate::Initialize(isolate_, *params);

  // If the indexes are not nullptr, we are not deserializing
  CHECK_IMPLIES(deserialize_mode_, params->external_references != nullptr);
  isolate_data_ = std::make_unique<IsolateData>(isolate_,
                                                event_loop,
                                                platform,
                                                array_buffer_allocator_.get(),
                                                per_isolate_data_indexes);
  IsolateSettings s;
  SetIsolateMiscHandlers(isolate_, s);
  if (!deserialize_mode_) {
    // If in deserialize mode, delay until after the deserialization is
    // complete.
    SetIsolateErrorHandlers(isolate_, s);
  }
  isolate_data_->max_young_gen_size =
      params->constraints.max_young_generation_size_in_bytes();
}

void NodeMainInstance::Dispose() {
  CHECK(!owns_isolate_);
  platform_->DrainTasks(isolate_);
}

NodeMainInstance::~NodeMainInstance() {
  if (!owns_isolate_) {
    return;
  }
  platform_->UnregisterIsolate(isolate_);
  isolate_->Dispose();
}

int NodeMainInstance::Run(const EnvSerializeInfo* env_info) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope handle_scope(isolate_);

  int exit_code = 0;
  DeleteFnPtr<Environment, FreeEnvironment> env =
      CreateMainEnvironment(&exit_code, env_info);
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
NodeMainInstance::CreateMainEnvironment(int* exit_code,
                                        const EnvSerializeInfo* env_info) {
  *exit_code = 0;  // Reset the exit code to 0

  HandleScope handle_scope(isolate_);

  // TODO(addaleax): This should load a real per-Isolate option, currently
  // this is still effectively per-process.
  if (isolate_data_->options()->track_heap_objects) {
    isolate_->GetHeapProfiler()->StartTrackingHeapObjects(true);
  }

  CHECK_IMPLIES(deserialize_mode_, env_info != nullptr);
  Local<Context> context;
  DeleteFnPtr<Environment, FreeEnvironment> env;

  if (deserialize_mode_) {
    env.reset(new Environment(isolate_data_.get(),
                              isolate_,
                              args_,
                              exec_args_,
                              env_info,
                              EnvironmentFlags::kDefaultFlags,
                              {}));
    context = Context::FromSnapshot(isolate_,
                                    kNodeContextIndex,
                                    {DeserializeNodeInternalFields, env.get()})
                  .ToLocalChecked();

    CHECK(!context.IsEmpty());
    Context::Scope context_scope(context);
    InitializeContextRuntime(context);
    SetIsolateErrorHandlers(isolate_, {});
    env->InitializeMainContext(context, env_info);
#if HAVE_INSPECTOR
    env->InitializeInspector({});
#endif
    env->DoneBootstrapping();
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
