#include "node.h"
#include "env-inl.h"
#include "debug_utils-inl.h"

using v8::Context;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::Maybe;
using v8::Nothing;
using v8::SealHandleScope;

namespace node {

Maybe<int> SpinEventLoop(Environment* env) {
  CHECK_NOT_NULL(env);
  MultiIsolatePlatform* platform = GetMultiIsolatePlatform(env);
  CHECK_NOT_NULL(platform);

  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(env->context());
  SealHandleScope seal(isolate);

  if (env->is_stopping()) return Nothing<int>();

  env->set_trace_sync_io(env->options()->trace_sync_io);
  {
    bool more;
    env->performance_state()->Mark(
        node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_START);
    do {
      if (env->is_stopping()) break;
      uv_run(env->event_loop(), UV_RUN_DEFAULT);
      if (env->is_stopping()) break;

      platform->DrainTasks(isolate);

      more = uv_loop_alive(env->event_loop());
      if (more && !env->is_stopping()) continue;

      if (EmitProcessBeforeExit(env).IsNothing())
        break;

      // Emit `beforeExit` if the loop became alive either after emitting
      // event, or after running some callbacks.
      more = uv_loop_alive(env->event_loop());
    } while (more == true && !env->is_stopping());
    env->performance_state()->Mark(
        node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_EXIT);
  }
  if (env->is_stopping()) return Nothing<int>();

  env->set_trace_sync_io(false);
  env->PrintInfoForSnapshotIfDebug();
  env->VerifyNoStrongBaseObjects();
  return EmitProcessExit(env);
}

struct CommonEnvironmentSetup::Impl {
  MultiIsolatePlatform* platform = nullptr;
  uv_loop_t loop;
  std::shared_ptr<ArrayBufferAllocator> allocator;
  Isolate* isolate = nullptr;
  DeleteFnPtr<IsolateData, FreeIsolateData> isolate_data;
  DeleteFnPtr<Environment, FreeEnvironment> env;
  Global<Context> context;
};

CommonEnvironmentSetup::CommonEnvironmentSetup(
    MultiIsolatePlatform* platform,
    std::vector<std::string>* errors,
    std::function<Environment*(const CommonEnvironmentSetup*)> make_env)
  : impl_(new Impl()) {
  CHECK_NOT_NULL(platform);
  CHECK_NOT_NULL(errors);

  impl_->platform = platform;
  uv_loop_t* loop = &impl_->loop;
  // Use `data` to tell the destructor whether the loop was initialized or not.
  loop->data = nullptr;
  int ret = uv_loop_init(loop);
  if (ret != 0) {
    errors->push_back(
        SPrintF("Failed to initialize loop: %s", uv_err_name(ret)));
    return;
  }
  loop->data = this;

  impl_->allocator = ArrayBufferAllocator::Create();
  impl_->isolate = NewIsolate(impl_->allocator, &impl_->loop, platform);
  Isolate* isolate = impl_->isolate;

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    impl_->isolate_data.reset(CreateIsolateData(
        isolate, loop, platform, impl_->allocator.get()));

    HandleScope handle_scope(isolate);
    Local<Context> context = NewContext(isolate);
    impl_->context.Reset(isolate, context);
    if (context.IsEmpty()) {
      errors->push_back("Failed to initialize V8 Context");
      return;
    }

    Context::Scope context_scope(context);
    impl_->env.reset(make_env(this));
  }
}

CommonEnvironmentSetup::~CommonEnvironmentSetup() {
  if (impl_->isolate != nullptr) {
    Isolate* isolate = impl_->isolate;
    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);

      impl_->context.Reset();
      impl_->env.reset();
      impl_->isolate_data.reset();
    }

    bool platform_finished = false;
    impl_->platform->AddIsolateFinishedCallback(isolate, [](void* data) {
      *static_cast<bool*>(data) = true;
    }, &platform_finished);
    impl_->platform->UnregisterIsolate(isolate);
    isolate->Dispose();

    // Wait until the platform has cleaned up all relevant resources.
    while (!platform_finished)
      uv_run(&impl_->loop, UV_RUN_ONCE);
  }

  if (impl_->isolate || impl_->loop.data != nullptr)
    CheckedUvLoopClose(&impl_->loop);

  delete impl_;
}


uv_loop_t* CommonEnvironmentSetup::event_loop() const {
  return &impl_->loop;
}

std::shared_ptr<ArrayBufferAllocator>
CommonEnvironmentSetup::array_buffer_allocator() const {
  return impl_->allocator;
}

Isolate* CommonEnvironmentSetup::isolate() const {
  return impl_->isolate;
}

IsolateData* CommonEnvironmentSetup::isolate_data() const {
  return impl_->isolate_data.get();
}

Environment* CommonEnvironmentSetup::env() const {
  return impl_->env.get();
}

v8::Local<v8::Context> CommonEnvironmentSetup::context() const {
  return impl_->context.Get(impl_->isolate);
}

}  // namespace node
