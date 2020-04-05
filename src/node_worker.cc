#include "node_worker.h"
#include "debug_utils-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_buffer.h"
#include "node_options-inl.h"
#include "node_perf.h"
#include "util-inl.h"
#include "async_wrap-inl.h"

#include <memory>
#include <string>
#include <vector>

using node::kAllowedInEnvironment;
using node::kDisallowedInEnvironment;
using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Boolean;
using v8::Context;
using v8::Float64Array;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ResourceConstraints;
using v8::SealHandleScope;
using v8::String;
using v8::TryCatch;
using v8::Value;

namespace node {
namespace worker {

Worker::Worker(Environment* env,
               Local<Object> wrap,
               const std::string& url,
               std::shared_ptr<PerIsolateOptions> per_isolate_opts,
               std::vector<std::string>&& exec_argv,
               std::shared_ptr<KVStore> env_vars)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_WORKER),
      per_isolate_opts_(per_isolate_opts),
      exec_argv_(exec_argv),
      platform_(env->isolate_data()->platform()),
      thread_id_(AllocateEnvironmentThreadId()),
      env_vars_(env_vars) {
  Debug(this, "Creating new worker instance with thread id %llu",
        thread_id_.id);

  // Set up everything that needs to be set up in the parent environment.
  parent_port_ = MessagePort::New(env, env->context());
  if (parent_port_ == nullptr) {
    // This can happen e.g. because execution is terminating.
    return;
  }

  child_port_data_ = std::make_unique<MessagePortData>(nullptr);
  MessagePort::Entangle(parent_port_, child_port_data_.get());

  object()->Set(env->context(),
                env->message_port_string(),
                parent_port_->object()).Check();

  object()->Set(env->context(),
                env->thread_id_string(),
                Number::New(env->isolate(), static_cast<double>(thread_id_.id)))
      .Check();

  inspector_parent_handle_ = GetInspectorParentHandle(
      env, thread_id_, url.c_str());

  argv_ = std::vector<std::string>{env->argv()[0]};
  // Mark this Worker object as weak until we actually start the thread.
  MakeWeak();

  Debug(this, "Preparation for worker %llu finished", thread_id_.id);
}

bool Worker::is_stopped() const {
  Mutex::ScopedLock lock(mutex_);
  if (env_ != nullptr)
    return env_->is_stopping();
  return stopped_;
}

void Worker::UpdateResourceConstraints(ResourceConstraints* constraints) {
  constraints->set_stack_limit(reinterpret_cast<uint32_t*>(stack_base_));

  constexpr double kMB = 1024 * 1024;

  if (resource_limits_[kMaxYoungGenerationSizeMb] > 0) {
    constraints->set_max_young_generation_size_in_bytes(
        resource_limits_[kMaxYoungGenerationSizeMb] * kMB);
  } else {
    resource_limits_[kMaxYoungGenerationSizeMb] =
        constraints->max_young_generation_size_in_bytes() / kMB;
  }

  if (resource_limits_[kMaxOldGenerationSizeMb] > 0) {
    constraints->set_max_old_generation_size_in_bytes(
        resource_limits_[kMaxOldGenerationSizeMb] * kMB);
  } else {
    resource_limits_[kMaxOldGenerationSizeMb] =
        constraints->max_old_generation_size_in_bytes() / kMB;
  }

  if (resource_limits_[kCodeRangeSizeMb] > 0) {
    constraints->set_code_range_size_in_bytes(
        resource_limits_[kCodeRangeSizeMb] * kMB);
  } else {
    resource_limits_[kCodeRangeSizeMb] =
        constraints->code_range_size_in_bytes() / kMB;
  }
}

// This class contains data that is only relevant to the child thread itself,
// and only while it is running.
// (Eventually, the Environment instance should probably also be moved here.)
class WorkerThreadData {
 public:
  explicit WorkerThreadData(Worker* w)
    : w_(w) {
    int ret = uv_loop_init(&loop_);
    if (ret != 0) {
      char err_buf[128];
      uv_err_name_r(ret, err_buf, sizeof(err_buf));
      w->custom_error_ = "ERR_WORKER_INIT_FAILED";
      w->custom_error_str_ = err_buf;
      w->stopped_ = true;
      return;
    }
    loop_init_failed_ = false;

    std::shared_ptr<ArrayBufferAllocator> allocator =
        ArrayBufferAllocator::Create();
    Isolate::CreateParams params;
    SetIsolateCreateParamsForNode(&params);
    params.array_buffer_allocator_shared = allocator;

    w->UpdateResourceConstraints(&params.constraints);

    Isolate* isolate = Isolate::Allocate();
    if (isolate == nullptr) {
      w->custom_error_ = "ERR_WORKER_OUT_OF_MEMORY";
      w->custom_error_str_ = "Failed to create new Isolate";
      w->stopped_ = true;
      return;
    }

    w->platform_->RegisterIsolate(isolate, &loop_);
    Isolate::Initialize(isolate, params);
    SetIsolateUpForNode(isolate);

    isolate->AddNearHeapLimitCallback(Worker::NearHeapLimit, w);

    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);
      // V8 computes its stack limit the first time a `Locker` is used based on
      // --stack-size. Reset it to the correct value.
      isolate->SetStackLimit(w->stack_base_);

      HandleScope handle_scope(isolate);
      isolate_data_.reset(CreateIsolateData(isolate,
                                            &loop_,
                                            w_->platform_,
                                            allocator.get()));
      CHECK(isolate_data_);
      if (w_->per_isolate_opts_)
        isolate_data_->set_options(std::move(w_->per_isolate_opts_));
      isolate_data_->set_worker_context(w_);
    }

    Mutex::ScopedLock lock(w_->mutex_);
    w_->isolate_ = isolate;
  }

  ~WorkerThreadData() {
    Debug(w_, "Worker %llu dispose isolate", w_->thread_id_.id);
    Isolate* isolate;
    {
      Mutex::ScopedLock lock(w_->mutex_);
      isolate = w_->isolate_;
      w_->isolate_ = nullptr;
    }

    if (isolate != nullptr) {
      CHECK(!loop_init_failed_);
      bool platform_finished = false;

      isolate_data_.reset();

      w_->platform_->AddIsolateFinishedCallback(isolate, [](void* data) {
        *static_cast<bool*>(data) = true;
      }, &platform_finished);

      // The order of these calls is important; if the Isolate is first disposed
      // and then unregistered, there is a race condition window in which no
      // new Isolate at the same address can successfully be registered with
      // the platform.
      // (Refs: https://github.com/nodejs/node/issues/30846)
      w_->platform_->UnregisterIsolate(isolate);
      isolate->Dispose();

      // Wait until the platform has cleaned up all relevant resources.
      while (!platform_finished) {
        uv_run(&loop_, UV_RUN_ONCE);
      }
    }
    if (!loop_init_failed_) {
      CheckedUvLoopClose(&loop_);
    }
  }

  bool loop_is_usable() const { return !loop_init_failed_; }

 private:
  Worker* const w_;
  uv_loop_t loop_;
  bool loop_init_failed_ = true;
  DeleteFnPtr<IsolateData, FreeIsolateData> isolate_data_;

  friend class Worker;
};

size_t Worker::NearHeapLimit(void* data, size_t current_heap_limit,
                             size_t initial_heap_limit) {
  Worker* worker = static_cast<Worker*>(data);
  worker->custom_error_ = "ERR_WORKER_OUT_OF_MEMORY";
  worker->custom_error_str_ = "JS heap out of memory";
  worker->Exit(1);
  // Give the current GC some extra leeway to let it finish rather than
  // crash hard. We are not going to perform further allocations anyway.
  constexpr size_t kExtraHeapAllowance = 16 * 1024 * 1024;
  return current_heap_limit + kExtraHeapAllowance;
}

void Worker::Run() {
  std::string name = "WorkerThread ";
  name += std::to_string(thread_id_.id);
  TRACE_EVENT_METADATA1(
      "__metadata", "thread_name", "name",
      TRACE_STR_COPY(name.c_str()));
  CHECK_NOT_NULL(platform_);

  Debug(this, "Creating isolate for worker with id %llu", thread_id_.id);

  WorkerThreadData data(this);
  if (isolate_ == nullptr) return;
  CHECK(data.loop_is_usable());

  Debug(this, "Starting worker with id %llu", thread_id_.id);
  {
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    SealHandleScope outer_seal(isolate_);

    DeleteFnPtr<Environment, FreeEnvironment> env_;
    auto cleanup_env = OnScopeLeave([&]() {
      if (!env_) return;
      env_->set_can_call_into_js(false);

      {
        Mutex::ScopedLock lock(mutex_);
        stopped_ = true;
        this->env_ = nullptr;
      }

      // TODO(addaleax): Try moving DisallowJavascriptExecutionScope into
      // FreeEnvironment().
      Isolate::DisallowJavascriptExecutionScope disallow_js(isolate_,
          Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
      env_.reset();
    });

    if (is_stopped()) return;
    {
      HandleScope handle_scope(isolate_);
      Local<Context> context;
      {
        // We create the Context object before we have an Environment* in place
        // that we could use for error handling. If creation fails due to
        // resource constraints, we need something in place to handle it,
        // though.
        TryCatch try_catch(isolate_);
        context = NewContext(isolate_);
        if (context.IsEmpty()) {
          custom_error_ = "ERR_WORKER_OUT_OF_MEMORY";
          custom_error_str_ = "Failed to create new Context";
          return;
        }
      }

      if (is_stopped()) return;
      CHECK(!context.IsEmpty());
      Context::Scope context_scope(context);
      {
        env_.reset(CreateEnvironment(
            data.isolate_data_.get(),
            context,
            std::move(argv_),
            std::move(exec_argv_),
            EnvironmentFlags::kNoFlags,
            thread_id_,
            std::move(inspector_parent_handle_)));
        if (is_stopped()) return;
        CHECK_NOT_NULL(env_);
        env_->set_env_vars(std::move(env_vars_));
        SetProcessExitHandler(env_.get(), [this](Environment*, int exit_code) {
          Exit(exit_code);
        });
      }
      {
        Mutex::ScopedLock lock(mutex_);
        if (stopped_) return;
        this->env_ = env_.get();
      }
      Debug(this, "Created Environment for worker with id %llu", thread_id_.id);
      if (is_stopped()) return;
      {
        CreateEnvMessagePort(env_.get());
        Debug(this, "Created message port for worker %llu", thread_id_.id);
        if (LoadEnvironment(env_.get(), StartExecutionCallback{}).IsEmpty())
          return;

        Debug(this, "Loaded environment for worker %llu", thread_id_.id);
      }

      if (is_stopped()) return;
      {
        SealHandleScope seal(isolate_);
        bool more;
        env_->performance_state()->Mark(
            node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_START);
        do {
          if (is_stopped()) break;
          uv_run(&data.loop_, UV_RUN_DEFAULT);
          if (is_stopped()) break;

          platform_->DrainTasks(isolate_);

          more = uv_loop_alive(&data.loop_);
          if (more && !is_stopped()) continue;

          EmitBeforeExit(env_.get());

          // Emit `beforeExit` if the loop became alive either after emitting
          // event, or after running some callbacks.
          more = uv_loop_alive(&data.loop_);
        } while (more == true && !is_stopped());
        env_->performance_state()->Mark(
            node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_EXIT);
      }
    }

    {
      int exit_code;
      bool stopped = is_stopped();
      if (!stopped)
        exit_code = EmitExit(env_.get());
      Mutex::ScopedLock lock(mutex_);
      if (exit_code_ == 0 && !stopped)
        exit_code_ = exit_code;

      Debug(this, "Exiting thread for worker %llu with exit code %d",
            thread_id_.id, exit_code_);
    }
  }

  Debug(this, "Worker %llu thread stops", thread_id_.id);
}

void Worker::CreateEnvMessagePort(Environment* env) {
  HandleScope handle_scope(isolate_);
  Mutex::ScopedLock lock(mutex_);
  // Set up the message channel for receiving messages in the child.
  MessagePort* child_port = MessagePort::New(env,
                                             env->context(),
                                             std::move(child_port_data_));
  // MessagePort::New() may return nullptr if execution is terminated
  // within it.
  if (child_port != nullptr)
    env->set_message_port(child_port->object(isolate_));
}

void Worker::JoinThread() {
  if (thread_joined_)
    return;
  CHECK_EQ(uv_thread_join(&tid_), 0);
  thread_joined_ = true;

  env()->remove_sub_worker_context(this);

  {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    // Reset the parent port as we're closing it now anyway.
    object()->Set(env()->context(),
                  env()->message_port_string(),
                  Undefined(env()->isolate())).Check();

    Local<Value> args[] = {
        Integer::New(env()->isolate(), exit_code_),
        custom_error_ != nullptr
            ? OneByteString(env()->isolate(), custom_error_).As<Value>()
            : Null(env()->isolate()).As<Value>(),
        !custom_error_str_.empty()
            ? OneByteString(env()->isolate(), custom_error_str_.c_str())
                  .As<Value>()
            : Null(env()->isolate()).As<Value>(),
    };

    MakeCallback(env()->onexit_string(), arraysize(args), args);
  }

  // If we get here, the !thread_joined_ condition at the top of the function
  // implies that the thread was running. In that case, its final action will
  // be to schedule a callback on the parent thread which will delete this
  // object, so there's nothing more to do here.
}

Worker::~Worker() {
  Mutex::ScopedLock lock(mutex_);

  CHECK(stopped_);
  CHECK_NULL(env_);
  CHECK(thread_joined_);

  Debug(this, "Worker %llu destroyed", thread_id_.id);
}

void Worker::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();

  CHECK(args.IsConstructCall());

  if (env->isolate_data()->platform() == nullptr) {
    THROW_ERR_MISSING_PLATFORM_FOR_WORKER(env);
    return;
  }

  std::string url;
  std::shared_ptr<PerIsolateOptions> per_isolate_opts = nullptr;
  std::shared_ptr<KVStore> env_vars = nullptr;

  std::vector<std::string> exec_argv_out;

  CHECK_EQ(args.Length(), 4);
  // Argument might be a string or URL
  if (!args[0]->IsNullOrUndefined()) {
    Utf8Value value(
        isolate, args[0]->ToString(env->context()).FromMaybe(Local<String>()));
    url.append(value.out(), value.length());
  }

  if (args[1]->IsNull()) {
    // Means worker.env = { ...process.env }.
    env_vars = env->env_vars()->Clone(isolate);
  } else if (args[1]->IsObject()) {
    // User provided env.
    env_vars = KVStore::CreateMapKVStore();
    env_vars->AssignFromObject(isolate->GetCurrentContext(),
                               args[1].As<Object>());
  } else {
    // Env is shared.
    env_vars = env->env_vars();
  }

  if (args[1]->IsObject() || args[2]->IsArray()) {
    per_isolate_opts.reset(new PerIsolateOptions());

    HandleEnvOptions(per_isolate_opts->per_env, [&env_vars](const char* name) {
      return env_vars->Get(name).FromMaybe("");
    });

#ifndef NODE_WITHOUT_NODE_OPTIONS
    MaybeLocal<String> maybe_node_opts =
        env_vars->Get(isolate, OneByteString(isolate, "NODE_OPTIONS"));
    if (!maybe_node_opts.IsEmpty()) {
      std::string node_options(
          *String::Utf8Value(isolate, maybe_node_opts.ToLocalChecked()));
      std::vector<std::string> errors{};
      std::vector<std::string> env_argv =
          ParseNodeOptionsEnvVar(node_options, &errors);
      // [0] is expected to be the program name, add dummy string.
      env_argv.insert(env_argv.begin(), "");
      std::vector<std::string> invalid_args{};
      options_parser::Parse(&env_argv,
                            nullptr,
                            &invalid_args,
                            per_isolate_opts.get(),
                            kAllowedInEnvironment,
                            &errors);
      if (errors.size() > 0 && args[1]->IsObject()) {
        // Only fail for explicitly provided env, this protects from failures
        // when NODE_OPTIONS from parent's env is used (which is the default).
        Local<Value> error;
        if (!ToV8Value(env->context(), errors).ToLocal(&error)) return;
        Local<String> key =
            FIXED_ONE_BYTE_STRING(env->isolate(), "invalidNodeOptions");
        // Ignore the return value of Set() because exceptions bubble up to JS
        // when we return anyway.
        USE(args.This()->Set(env->context(), key, error));
        return;
      }
    }
#endif
  }

  if (args[2]->IsArray()) {
    Local<Array> array = args[2].As<Array>();
    // The first argument is reserved for program name, but we don't need it
    // in workers.
    std::vector<std::string> exec_argv = {""};
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; i++) {
      Local<Value> arg;
      if (!array->Get(env->context(), i).ToLocal(&arg)) {
        return;
      }
      MaybeLocal<String> arg_v8_string =
          arg->ToString(env->context());
      if (arg_v8_string.IsEmpty()) {
        return;
      }
      Utf8Value arg_utf8_value(
          args.GetIsolate(),
          arg_v8_string.FromMaybe(Local<String>()));
      std::string arg_string(arg_utf8_value.out(), arg_utf8_value.length());
      exec_argv.push_back(arg_string);
    }

    std::vector<std::string> invalid_args{};
    std::vector<std::string> errors{};
    // Using invalid_args as the v8_args argument as it stores unknown
    // options for the per isolate parser.
    options_parser::Parse(
        &exec_argv,
        &exec_argv_out,
        &invalid_args,
        per_isolate_opts.get(),
        kDisallowedInEnvironment,
        &errors);

    // The first argument is program name.
    invalid_args.erase(invalid_args.begin());
    if (errors.size() > 0 || invalid_args.size() > 0) {
      Local<Value> error;
      if (!ToV8Value(env->context(),
                     errors.size() > 0 ? errors : invalid_args)
                         .ToLocal(&error)) {
        return;
      }
      Local<String> key =
          FIXED_ONE_BYTE_STRING(env->isolate(), "invalidExecArgv");
      // Ignore the return value of Set() because exceptions bubble up to JS
      // when we return anyway.
      USE(args.This()->Set(env->context(), key, error));
      return;
    }
  } else {
    exec_argv_out = env->exec_argv();
  }

  Worker* worker = new Worker(env,
                              args.This(),
                              url,
                              per_isolate_opts,
                              std::move(exec_argv_out),
                              env_vars);

  CHECK(args[3]->IsFloat64Array());
  Local<Float64Array> limit_info = args[3].As<Float64Array>();
  CHECK_EQ(limit_info->Length(), kTotalResourceLimitCount);
  limit_info->CopyContents(worker->resource_limits_,
                           sizeof(worker->resource_limits_));
}

void Worker::StartThread(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  Mutex::ScopedLock lock(w->mutex_);

  w->stopped_ = false;

  uv_thread_options_t thread_options;
  thread_options.flags = UV_THREAD_HAS_STACK_SIZE;
  thread_options.stack_size = kStackSize;
  int ret = uv_thread_create_ex(&w->tid_, &thread_options, [](void* arg) {
    // XXX: This could become a std::unique_ptr, but that makes at least
    // gcc 6.3 detect undefined behaviour when there shouldn't be any.
    // gcc 7+ handles this well.
    Worker* w = static_cast<Worker*>(arg);
    const uintptr_t stack_top = reinterpret_cast<uintptr_t>(&arg);

    // Leave a few kilobytes just to make sure we're within limits and have
    // some space to do work in C++ land.
    w->stack_base_ = stack_top - (kStackSize - kStackBufferSize);

    w->Run();

    Mutex::ScopedLock lock(w->mutex_);
    w->env()->SetImmediateThreadsafe(
        [w = std::unique_ptr<Worker>(w)](Environment* env) {
          if (w->has_ref_)
            env->add_refs(-1);
          w->JoinThread();
          // implicitly delete w
        });
  }, static_cast<void*>(w));

  if (ret == 0) {
    // The object now owns the created thread and should not be garbage
    // collected until that finishes.
    w->ClearWeak();
    w->thread_joined_ = false;

    if (w->has_ref_)
      w->env()->add_refs(1);

    w->env()->add_sub_worker_context(w);
  } else {
    w->stopped_ = true;

    char err_buf[128];
    uv_err_name_r(ret, err_buf, sizeof(err_buf));
    {
      Isolate* isolate = w->env()->isolate();
      HandleScope handle_scope(isolate);
      THROW_ERR_WORKER_INIT_FAILED(isolate, err_buf);
    }
  }
}

void Worker::StopThread(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());

  Debug(w, "Worker %llu is getting stopped by parent", w->thread_id_.id);
  w->Exit(1);
}

void Worker::Ref(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  if (!w->has_ref_) {
    w->has_ref_ = true;
    w->env()->add_refs(1);
  }
}

void Worker::Unref(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  if (w->has_ref_) {
    w->has_ref_ = false;
    w->env()->add_refs(-1);
  }
}

void Worker::GetResourceLimits(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  args.GetReturnValue().Set(w->GetResourceLimits(args.GetIsolate()));
}

Local<Float64Array> Worker::GetResourceLimits(Isolate* isolate) const {
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sizeof(resource_limits_));

  memcpy(ab->GetBackingStore()->Data(),
         resource_limits_,
         sizeof(resource_limits_));
  return Float64Array::New(ab, 0, kTotalResourceLimitCount);
}

void Worker::Exit(int code) {
  Mutex::ScopedLock lock(mutex_);
  Debug(this, "Worker %llu called Exit(%d)", thread_id_.id, code);
  if (env_ != nullptr) {
    exit_code_ = code;
    Stop(env_);
  } else {
    stopped_ = true;
  }
}

void Worker::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("parent_port", parent_port_);
}

class WorkerHeapSnapshotTaker : public AsyncWrap {
 public:
  WorkerHeapSnapshotTaker(Environment* env, Local<Object> obj)
    : AsyncWrap(env, obj, AsyncWrap::PROVIDER_WORKERHEAPSNAPSHOT) {}

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(WorkerHeapSnapshotTaker)
  SET_SELF_SIZE(WorkerHeapSnapshotTaker)
};

void Worker::TakeHeapSnapshot(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());

  Debug(w, "Worker %llu taking heap snapshot", w->thread_id_.id);

  Environment* env = w->env();
  AsyncHooks::DefaultTriggerAsyncIdScope trigger_id_scope(w);
  Local<Object> wrap;
  if (!env->worker_heap_snapshot_taker_template()
      ->NewInstance(env->context()).ToLocal(&wrap)) {
    return;
  }
  BaseObjectPtr<WorkerHeapSnapshotTaker> taker =
      MakeDetachedBaseObject<WorkerHeapSnapshotTaker>(env, wrap);

  // Interrupt the worker thread and take a snapshot, then schedule a call
  // on the parent thread that turns that snapshot into a readable stream.
  bool scheduled = w->RequestInterrupt([taker, env](Environment* worker_env) {
    heap::HeapSnapshotPointer snapshot {
        worker_env->isolate()->GetHeapProfiler()->TakeHeapSnapshot() };
    CHECK(snapshot);
    env->SetImmediateThreadsafe(
        [taker, snapshot = std::move(snapshot)](Environment* env) mutable {
          HandleScope handle_scope(env->isolate());
          Context::Scope context_scope(env->context());

          AsyncHooks::DefaultTriggerAsyncIdScope trigger_id_scope(taker.get());
          BaseObjectPtr<AsyncWrap> stream = heap::CreateHeapSnapshotStream(
              env, std::move(snapshot));
          Local<Value> args[] = { stream->object() };
          taker->MakeCallback(env->ondone_string(), arraysize(args), args);
        });
  });
  args.GetReturnValue().Set(scheduled ? taker->object() : Local<Object>());
}

namespace {

// Return the MessagePort that is global for this Environment and communicates
// with the internal [kPort] port of the JS Worker class in the parent thread.
void GetEnvMessagePort(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Object> port = env->message_port();
  CHECK_IMPLIES(!env->is_main_thread(), !port.IsEmpty());
  if (!port.IsEmpty()) {
    CHECK_EQ(port->CreationContext()->GetIsolate(), args.GetIsolate());
    args.GetReturnValue().Set(port);
  }
}

void InitWorker(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  {
    Local<FunctionTemplate> w = env->NewFunctionTemplate(Worker::New);

    w->InstanceTemplate()->SetInternalFieldCount(
        Worker::kInternalFieldCount);
    w->Inherit(AsyncWrap::GetConstructorTemplate(env));

    env->SetProtoMethod(w, "startThread", Worker::StartThread);
    env->SetProtoMethod(w, "stopThread", Worker::StopThread);
    env->SetProtoMethod(w, "ref", Worker::Ref);
    env->SetProtoMethod(w, "unref", Worker::Unref);
    env->SetProtoMethod(w, "getResourceLimits", Worker::GetResourceLimits);
    env->SetProtoMethod(w, "takeHeapSnapshot", Worker::TakeHeapSnapshot);

    Local<String> workerString =
        FIXED_ONE_BYTE_STRING(env->isolate(), "Worker");
    w->SetClassName(workerString);
    target->Set(env->context(),
                workerString,
                w->GetFunction(env->context()).ToLocalChecked()).Check();
  }

  {
    Local<FunctionTemplate> wst = FunctionTemplate::New(env->isolate());

    wst->InstanceTemplate()->SetInternalFieldCount(
        WorkerHeapSnapshotTaker::kInternalFieldCount);
    wst->Inherit(AsyncWrap::GetConstructorTemplate(env));

    Local<String> wst_string =
        FIXED_ONE_BYTE_STRING(env->isolate(), "WorkerHeapSnapshotTaker");
    wst->SetClassName(wst_string);
    env->set_worker_heap_snapshot_taker_template(wst->InstanceTemplate());
  }

  env->SetMethod(target, "getEnvMessagePort", GetEnvMessagePort);

  target
      ->Set(env->context(),
            env->thread_id_string(),
            Number::New(env->isolate(), static_cast<double>(env->thread_id())))
      .Check();

  target
      ->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "isMainThread"),
            Boolean::New(env->isolate(), env->is_main_thread()))
      .Check();

  target
      ->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "ownsProcessState"),
            Boolean::New(env->isolate(), env->owns_process_state()))
      .Check();

  if (!env->is_main_thread()) {
    target
        ->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "resourceLimits"),
              env->worker_context()->GetResourceLimits(env->isolate()))
        .Check();
  }

  NODE_DEFINE_CONSTANT(target, kMaxYoungGenerationSizeMb);
  NODE_DEFINE_CONSTANT(target, kMaxOldGenerationSizeMb);
  NODE_DEFINE_CONSTANT(target, kCodeRangeSizeMb);
  NODE_DEFINE_CONSTANT(target, kTotalResourceLimitCount);
}

}  // anonymous namespace

}  // namespace worker
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(worker, node::worker::InitWorker)
