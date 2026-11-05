#include "async_wrap-inl.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_messaging.h"
#include "node_mutex.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace node {
namespace green_thread {

using v8::Array;
using v8::Boolean;
using v8::ConstructorBehavior;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::IdleTask;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::LocalVector;
using v8::Locker;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Message;
using v8::Name;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::Promise;
using v8::PromiseRejectEvent;
using v8::PromiseRejectMessage;
using v8::PropertyAttribute;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::SourceLocation;
using v8::String;
using v8::Task;
using v8::TaskRunner;
using v8::TryCatch;
using v8::Uint32;
using v8::Undefined;
using v8::Value;
using v8::ValueDeserializer;
using v8::ValueSerializer;

namespace {

constexpr size_t kStackSize = 4 * 1024 * 1024;
constexpr size_t kStackBufferSize = 192 * 1024;
constexpr unsigned int kMaxPoolThreads = 8;
constexpr size_t kMB = 1024 * 1024;
constexpr size_t kExtraHeapAllowance = 16 * kMB;

// Event types sent from a green thread to its host.
enum EventType : int32_t {
  kEventMessage = 0,
  kEventConsole,
  kEventEvalResult,
  kEventError,
  kEventExit,
};

// Values of the `id` field of kEventError events.
enum ErrorKind : int32_t {
  kErrorException = 0,
  kErrorOutOfMemory,
};

using Bytes = std::vector<uint8_t>;

// A pool thread owns a libuv loop and runs the tasks posted to it, in order,
// on that loop. Pool threads live until the process exits.
class PoolThread {
 public:
  using Callback = std::function<void()>;

  explicit PoolThread(size_t index) : index_(index) {}

  void Start() {
    CHECK_EQ(uv_loop_init(&loop_), 0);
    CHECK_EQ(uv_async_init(&loop_, &wakeup_, OnWakeup), 0);

    uv_thread_options_t options;
    options.flags = UV_THREAD_HAS_STACK_SIZE;
    options.stack_size = kStackSize;
    CHECK_EQ(
        uv_thread_create_ex(
            &tid_,
            &options,
            [](void* arg) {
              PoolThread* self = static_cast<PoolThread*>(arg);
              const uintptr_t stack_top = reinterpret_cast<uintptr_t>(&arg);
              // Leave some room for C++ code below the V8 limit.
              self->stack_limit_ = stack_top - (kStackSize - kStackBufferSize);
              char name[16];
              snprintf(name, sizeof(name), "GreenThread%zu", self->index_);
              uv_thread_setname(name);
              uv_run(&self->loop_, UV_RUN_DEFAULT);
            },
            this),
        0);
  }

  // May be called from any thread.
  void Post(Callback callback) {
    {
      Mutex::ScopedLock lock(mutex_);
      queue_.push_back(std::move(callback));
    }
    CHECK_EQ(uv_async_send(&wakeup_), 0);
  }

  uv_loop_t* loop() { return &loop_; }
  uintptr_t stack_limit() const { return stack_limit_; }

 private:
  static void OnWakeup(uv_async_t* handle) {
    PoolThread* self = ContainerOf(&PoolThread::wakeup_, handle);
    std::deque<Callback> callbacks;
    {
      Mutex::ScopedLock lock(self->mutex_);
      callbacks.swap(self->queue_);
    }
    for (Callback& callback : callbacks) callback();
  }

  const size_t index_;
  uv_thread_t tid_;
  uv_loop_t loop_;
  uv_async_t wakeup_;
  Mutex mutex_;
  std::deque<Callback> queue_;
  uintptr_t stack_limit_ = 0;
};

// The process-wide set of pool threads. It is created on first use and
// intentionally never destroyed.
class GreenThreadPool {
 public:
  static GreenThreadPool* Get() {
    static GreenThreadPool* pool = new GreenThreadPool();
    return pool;
  }

  PoolThread* Next() {
    size_t index = next_.fetch_add(1, std::memory_order_relaxed);
    return threads_[index % threads_.size()].get();
  }

 private:
  GreenThreadPool() {
    unsigned int count =
        std::clamp(uv_available_parallelism(), 1u, kMaxPoolThreads);
    for (unsigned int i = 0; i < count; i++) {
      threads_.push_back(std::make_unique<PoolThread>(i));
      threads_.back()->Start();
    }
  }

  std::vector<std::unique_ptr<PoolThread>> threads_;
  std::atomic<size_t> next_{0};
};

class SerializerDelegate final : public ValueSerializer::Delegate {
 public:
  SerializerDelegate(Isolate* isolate, Local<Context> context, bool is_host)
      : isolate_(isolate), context_(context), is_host_(is_host) {}

  void ThrowDataCloneError(Local<String> message) override {
    if (is_host_) {
      worker::ThrowDataCloneException(context_, message);
      return;
    }
    // Green threads have no DOMException, so use a plain Error with the
    // same name.
    Local<Value> error = Exception::Error(message);
    error.As<Object>()
        ->Set(context_,
              FIXED_ONE_BYTE_STRING(isolate_, "name"),
              FIXED_ONE_BYTE_STRING(isolate_, "DataCloneError"))
        .Check();
    isolate_->ThrowException(error);
  }

 private:
  Isolate* isolate_;
  Local<Context> context_;
  bool is_host_;
};

// Throws a JS exception and returns std::nullopt if `value` is not cloneable.
std::optional<Bytes> Serialize(Isolate* isolate,
                               Local<Context> context,
                               Local<Value> value,
                               bool is_host) {
  SerializerDelegate delegate(isolate, context, is_host);
  ValueSerializer serializer(isolate, &delegate);
  serializer.WriteHeader();
  if (serializer.WriteValue(context, value).IsNothing()) return std::nullopt;
  std::pair<uint8_t*, size_t> buffer = serializer.Release();
  Bytes bytes(buffer.first, buffer.first + buffer.second);
  free(buffer.first);
  return bytes;
}

MaybeLocal<Value> Deserialize(Isolate* isolate,
                              Local<Context> context,
                              const Bytes& bytes) {
  ValueDeserializer deserializer(isolate, bytes.data(), bytes.size());
  if (deserializer.ReadHeader(context).IsNothing()) return {};
  return deserializer.ReadValue(context);
}

struct HostEvent {
  EventType type;
  int32_t id = 0;
  bool ok = true;
  Bytes payload;
};

// Carries events from a green thread to its host. The host closes it before
// closing its uv_async_t, after which events are dropped.
class HostChannel {
 public:
  explicit HostChannel(uv_async_t* async) : async_(async) {}

  void Push(HostEvent&& event) {
    Mutex::ScopedLock lock(mutex_);
    if (async_ == nullptr) return;
    queue_.push_back(std::move(event));
    CHECK_EQ(uv_async_send(async_), 0);
  }

  std::deque<HostEvent> Take() {
    std::deque<HostEvent> events;
    Mutex::ScopedLock lock(mutex_);
    events.swap(queue_);
    return events;
  }

  void Close() {
    Mutex::ScopedLock lock(mutex_);
    async_ = nullptr;
    queue_.clear();
  }

 private:
  Mutex mutex_;
  uv_async_t* async_;
  std::deque<HostEvent> queue_;
};

class GreenIsolate;

// NOLINTNEXTLINE(runtime/thread_local)
thread_local GreenIsolate* current_green_isolate = nullptr;

// All green isolates that have not been disposed yet, so that they can be
// stopped before the process exits.
Mutex live_green_isolates_mutex;
std::unordered_map<GreenIsolate*, std::weak_ptr<GreenIsolate>>
    live_green_isolates;

// The foreground task runner V8 uses for a green isolate. Tasks are run on the
// green isolate's pool thread, with the isolate locked.
class GreenTaskRunner final : public TaskRunner {
 public:
  explicit GreenTaskRunner(std::weak_ptr<GreenIsolate> owner)
      : owner_(std::move(owner)) {}

  bool IdleTasksEnabled() override { return false; }
  bool NonNestableTasksEnabled() const override { return true; }
  bool NonNestableDelayedTasksEnabled() const override { return true; }

 private:
  void PostTaskImpl(std::unique_ptr<Task> task,
                    const SourceLocation& location) override {
    Post(std::move(task), 0);
  }
  void PostNonNestableTaskImpl(std::unique_ptr<Task> task,
                               const SourceLocation& location) override {
    Post(std::move(task), 0);
  }
  void PostDelayedTaskImpl(std::unique_ptr<Task> task,
                           double delay_in_seconds,
                           const SourceLocation& location) override {
    Post(std::move(task), delay_in_seconds);
  }
  void PostNonNestableDelayedTaskImpl(std::unique_ptr<Task> task,
                                      double delay_in_seconds,
                                      const SourceLocation& location) override {
    Post(std::move(task), delay_in_seconds);
  }
  void PostIdleTaskImpl(std::unique_ptr<IdleTask> task,
                        const SourceLocation& location) override {
    UNREACHABLE();
  }

  inline void Post(std::unique_ptr<Task> task, double delay_in_seconds);

  std::weak_ptr<GreenIsolate> owner_;
};

// The state of one green thread: a V8 isolate pinned to a pool thread, plus
// its timers. Except where noted, methods run on the pool thread.
class GreenIsolate final : public IsolatePlatformDelegate,
                           public std::enable_shared_from_this<GreenIsolate> {
 public:
  struct Options {
    std::string source;
    std::string filename;
    size_t max_old_generation_size = 0;
    size_t max_young_generation_size = 0;
  };

  GreenIsolate(MultiIsolatePlatform* platform,
               std::shared_ptr<HostChannel> host)
      : platform_(platform),
        thread_(GreenThreadPool::Get()->Next()),
        host_(std::move(host)) {}

  // Must be called once, from the host thread, right after construction.
  void Start(Options options) {
    task_runner_ = std::make_shared<GreenTaskRunner>(weak_from_this());
    {
      Mutex::ScopedLock lock(live_green_isolates_mutex);
      live_green_isolates.emplace(this, weak_from_this());
    }
    Post([options = std::move(options)](GreenIsolate* self) {
      self->Initialize(options);
    });
  }

  // IsolatePlatformDelegate. May be called from any thread.
  std::shared_ptr<TaskRunner> GetForegroundTaskRunner() override {
    return task_runner_;
  }
  bool IdleTasksEnabled() override { return false; }

  // May be called from any thread.
  void PostMessage(Bytes data) {
    Post([data = std::move(data)](GreenIsolate* self) {
      self->RunTask([&](Isolate* isolate, Local<Context> context) {
        self->DeliverMessage(isolate, context, data);
      });
    });
  }

  // May be called from any thread.
  void Evaluate(int32_t id, std::string source, std::string filename) {
    Post([=](GreenIsolate* self) {
      self->RunTask([&](Isolate* isolate, Local<Context> context) {
        self->EvaluateSource(isolate, context, id, source, filename);
      });
    });
  }

  // May be called from any thread. Interrupts running JS, if any.
  void Terminate() {
    terminate_requested_ = true;
    {
      Mutex::ScopedLock lock(isolate_mutex_);
      if (isolate_ != nullptr) isolate_->TerminateExecution();
    }
    Post([](GreenIsolate* self) { self->Shutdown(); });
  }

  // May be called from any thread.
  void WaitForDisposal() {
    Mutex::ScopedLock lock(disposal_mutex_);
    while (!disposed_) disposal_cv_.Wait(lock);
  }

  // May be called from any thread.
  void PostV8Task(std::unique_ptr<Task> task, double delay_in_seconds) {
    auto holder = std::make_shared<std::unique_ptr<Task>>(std::move(task));
    Post([holder, delay_in_seconds](GreenIsolate* self) {
      if (delay_in_seconds <= 0) {
        self->RunV8Task(std::move(*holder));
      } else {
        self->ScheduleV8Task(std::move(*holder), delay_in_seconds);
      }
    });
  }

 private:
  enum class State { kPending, kRunning, kStopped };

  struct Timer {
    uv_timer_t handle;
    GreenIsolate* owner;
    uint32_t id;
    bool repeat;
    Global<Function> callback;
  };

  struct DelayedV8Task {
    uv_timer_t handle;
    GreenIsolate* owner;
    std::unique_ptr<Task> task;
  };

  class CurrentScope {
   public:
    explicit CurrentScope(GreenIsolate* green)
        : previous_(current_green_isolate) {
      current_green_isolate = green;
    }
    ~CurrentScope() { current_green_isolate = previous_; }

   private:
    GreenIsolate* previous_;
  };

  template <typename Fn>
  void Post(Fn&& fn) {
    thread_->Post([self = shared_from_this(), fn = std::forward<Fn>(fn)]() {
      fn(self.get());
    });
  }

  void Initialize(const Options& options) {
    CHECK_EQ(state_, State::kPending);
    if (terminate_requested_) return Shutdown();

    Isolate::CreateParams params;
    params.array_buffer_allocator_shared = ArrayBufferAllocator::Create();
    if (options.max_old_generation_size > 0) {
      params.constraints.set_max_old_generation_size_in_bytes(
          options.max_old_generation_size);
    }
    if (options.max_young_generation_size > 0) {
      params.constraints.set_max_young_generation_size_in_bytes(
          options.max_young_generation_size);
    }

    // Node.js' default callbacks expect a Node.js context, so skip or replace
    // them.
    IsolateSettings settings;
    settings.flags = SHOULD_NOT_SET_PREPARE_STACK_TRACE_CALLBACK |
                     SHOULD_NOT_SET_WASM_STREAMING_CALLBACK;
    settings.promise_reject_callback = OnPromiseReject;
    settings.allow_wasm_code_generation_callback =
        [](Local<Context>, Local<String>) { return true; };

    Isolate* isolate = NewIsolate(&params, this, platform_, settings);
    if (isolate == nullptr) {
      fatal_ = true;
      return Shutdown();
    }
    {
      Mutex::ScopedLock lock(isolate_mutex_);
      isolate_ = isolate;
    }

    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);
      isolate->SetStackLimit(thread_->stack_limit());
      HandleScope handle_scope(isolate);
      isolate->AddNearHeapLimitCallback(NearHeapLimit, this);
      isolate->AddMessageListenerWithErrorLevel(
          OnMessage, Isolate::MessageErrorLevel::kMessageError);
      // The default context of the startup snapshot contains only what V8
      // creates.
      Local<Context> context = Context::New(isolate);
      if (context.IsEmpty()) {
        fatal_ = true;
      } else {
        Context::Scope context_scope(context);
        context_.Reset(isolate, context);
        if (!InstallGlobals(isolate, context)) fatal_ = true;
      }
    }
    if (fatal_) return Shutdown();

    state_ = State::kRunning;
    RunTask([&](Isolate* isolate, Local<Context> context) {
      Local<Script> script;
      if (!Compile(isolate, context, options.source, options.filename)
               .ToLocal(&script)) {
        return;
      }
      USE(script->Run(context));
    });
  }

  // Runs `fn` as one macrotask: with the isolate locked and entered, followed
  // by a microtask checkpoint. Uncaught errors stop the green thread.
  template <typename Fn>
  void RunTask(Fn&& fn) {
    if (state_ != State::kRunning) return;
    Isolate* isolate = isolate_;
    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);
      isolate->SetStackLimit(thread_->stack_limit());
      HandleScope handle_scope(isolate);
      Local<Context> context = context_.Get(isolate);
      Context::Scope context_scope(context);
      CurrentScope current_scope(this);

      // A TerminateExecution() request made while the isolate was unlocked
      // is discarded when the lock is acquired, so check the flag as well.
      if (!terminate_requested_ && !isolate->IsExecutionTerminating()) {
        TryCatch try_catch(isolate);
        fn(isolate, context);
        if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
          ReportUncaughtException(isolate, context, try_catch.Exception());
        }
      }
      if (!terminate_requested_ && !isolate->IsExecutionTerminating()) {
        isolate->PerformMicrotaskCheckpoint();
      }
      if (!fatal_ && !isolate->IsExecutionTerminating()) {
        CheckUnhandledRejections(isolate, context);
      }
    }
    if (out_of_memory_ && !fatal_) {
      fatal_ = true;
      host_->Push({kEventError, kErrorOutOfMemory, false, {}});
    }
    if (fatal_ || terminate_requested_ || close_requested_) Shutdown();
  }

  void RunV8Task(std::unique_ptr<Task> task) {
    RunTask([&](Isolate*, Local<Context>) { task->Run(); });
  }

  void ScheduleV8Task(std::unique_ptr<Task> task, double delay_in_seconds) {
    if (state_ == State::kStopped) return;
    DelayedV8Task* delayed = new DelayedV8Task{{}, this, std::move(task)};
    CHECK_EQ(uv_timer_init(thread_->loop(), &delayed->handle), 0);
    uv_timer_start(
        &delayed->handle,
        [](uv_timer_t* handle) {
          DelayedV8Task* delayed = ContainerOf(&DelayedV8Task::handle, handle);
          GreenIsolate* self = delayed->owner;
          self->delayed_v8_tasks_.erase(delayed);
          std::unique_ptr<Task> task = std::move(delayed->task);
          CloseDelayedV8Task(delayed);
          self->RunV8Task(std::move(task));
        },
        static_cast<uint64_t>(delay_in_seconds * 1000),
        0);
    delayed_v8_tasks_.insert(delayed);
  }

  static void CloseDelayedV8Task(DelayedV8Task* delayed) {
    uv_close(reinterpret_cast<uv_handle_t*>(&delayed->handle),
             [](uv_handle_t* handle) {
               DelayedV8Task* self =
                   ContainerOf(&DelayedV8Task::handle,
                               reinterpret_cast<uv_timer_t*>(handle));
               delete self;
             });
  }

  void Shutdown() {
    if (state_ == State::kStopped) return;
    state_ = State::kStopped;

    if (isolate_ != nullptr) {
      {
        Locker locker(isolate_);
        Isolate::Scope isolate_scope(isolate_);
        HandleScope handle_scope(isolate_);
        for (auto& [id, timer] : timers_) {
          timer->callback.Reset();
          CloseTimer(timer.release());
        }
        timers_.clear();
        pending_rejections_.clear();
        context_.Reset();
      }
      for (DelayedV8Task* delayed : delayed_v8_tasks_) {
        CloseDelayedV8Task(delayed);
      }
      delayed_v8_tasks_.clear();

      Isolate* isolate;
      {
        Mutex::ScopedLock lock(isolate_mutex_);
        isolate = isolate_;
        isolate_ = nullptr;
      }
      platform_->DisposeIsolate(isolate);
    }

    const bool clean_exit = close_requested_ && !fatal_ && !out_of_memory_;
    host_->Push({kEventExit, clean_exit ? 0 : 1, true, {}});

    {
      Mutex::ScopedLock lock(live_green_isolates_mutex);
      live_green_isolates.erase(this);
    }
    Mutex::ScopedLock lock(disposal_mutex_);
    disposed_ = true;
    disposal_cv_.Broadcast(lock);
  }

  MaybeLocal<Script> Compile(Isolate* isolate,
                             Local<Context> context,
                             const std::string& source,
                             const std::string& filename) {
    Local<Value> source_string;
    Local<Value> filename_string;
    if (!ToV8Value(context, source, isolate).ToLocal(&source_string) ||
        !ToV8Value(context, filename, isolate).ToLocal(&filename_string)) {
      return {};
    }
    ScriptOrigin origin(filename_string);
    ScriptCompiler::Source script_source(source_string.As<String>(), origin);
    return ScriptCompiler::Compile(context, &script_source);
  }

  void DeliverMessage(Isolate* isolate,
                      Local<Context> context,
                      const Bytes& data) {
    Local<Value> handler;
    if (!context->Global()
             ->Get(context, FIXED_ONE_BYTE_STRING(isolate, "onmessage"))
             .ToLocal(&handler) ||
        !handler->IsFunction()) {
      return;
    }
    Local<Value> value;
    if (!Deserialize(isolate, context, data).ToLocal(&value)) return;
    Local<Object> event = Object::New(isolate);
    if (event->Set(context, FIXED_ONE_BYTE_STRING(isolate, "data"), value)
            .IsNothing()) {
      return;
    }
    Local<Value> argv[] = {event};
    USE(handler.As<Function>()->Call(
        context, context->Global(), arraysize(argv), argv));
  }

  void EvaluateSource(Isolate* isolate,
                      Local<Context> context,
                      int32_t id,
                      const std::string& source,
                      const std::string& filename) {
    TryCatch try_catch(isolate);
    Local<Script> script;
    Local<Value> result;
    if (!Compile(isolate, context, source, filename).ToLocal(&script) ||
        !script->Run(context).ToLocal(&result)) {
      if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
        PushValue(isolate,
                  context,
                  kEventEvalResult,
                  id,
                  false,
                  try_catch.Exception());
      }
      return;
    }
    if (!result->IsPromise()) {
      PushValue(isolate, context, kEventEvalResult, id, true, result);
      return;
    }

    Local<Value> data = Integer::New(isolate, id);
    Local<Function> on_fulfilled;
    Local<Function> on_rejected;
    if (!Function::New(
             context, OnEvalFulfilled, data, 1, ConstructorBehavior::kThrow)
             .ToLocal(&on_fulfilled) ||
        !Function::New(
             context, OnEvalRejected, data, 1, ConstructorBehavior::kThrow)
             .ToLocal(&on_rejected) ||
        result.As<Promise>()
            ->Then(context, on_fulfilled, on_rejected)
            .IsEmpty()) {
      if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
        PushValue(isolate,
                  context,
                  kEventEvalResult,
                  id,
                  false,
                  try_catch.Exception());
      }
    }
  }

  static void OnEvalSettled(const FunctionCallbackInfo<Value>& args, bool ok) {
    GreenIsolate* self = current_green_isolate;
    CHECK_NOT_NULL(self);
    Isolate* isolate = args.GetIsolate();
    int32_t id = args.Data().As<Int32>()->Value();
    self->PushValue(isolate,
                    isolate->GetCurrentContext(),
                    kEventEvalResult,
                    id,
                    ok,
                    args[0]);
  }
  static void OnEvalFulfilled(const FunctionCallbackInfo<Value>& args) {
    OnEvalSettled(args, true);
  }
  static void OnEvalRejected(const FunctionCallbackInfo<Value>& args) {
    OnEvalSettled(args, false);
  }

  enum class OnCloneError { kSendError, kSendString };

  // Sends `value` to the host. If it can't be cloned, sends either the clone
  // error (with ok = false) or the string form of `value` instead.
  void PushValue(Isolate* isolate,
                 Local<Context> context,
                 EventType type,
                 int32_t id,
                 bool ok,
                 Local<Value> value,
                 OnCloneError on_clone_error = OnCloneError::kSendError) {
    TryCatch try_catch(isolate);
    std::optional<Bytes> bytes = Serialize(isolate, context, value, false);
    if (!bytes && try_catch.HasCaught() && !try_catch.HasTerminated()) {
      Local<Value> fallback = try_catch.Exception();
      try_catch.Reset();
      if (on_clone_error == OnCloneError::kSendError) {
        ok = false;
        bytes = Serialize(isolate, context, fallback, false);
        try_catch.Reset();
      }
      Local<String> string;
      if (!bytes && !try_catch.HasTerminated()) {
        if (on_clone_error == OnCloneError::kSendString) fallback = value;
        if (fallback->ToDetailString(context).ToLocal(&string)) {
          bytes = Serialize(isolate, context, string, false);
        }
      }
    }
    if (try_catch.HasTerminated()) return;
    host_->Push({type, id, ok, bytes ? std::move(*bytes) : Bytes{}});
  }

  void ReportUncaughtException(Isolate* isolate,
                               Local<Context> context,
                               Local<Value> exception) {
    if (fatal_) return;
    fatal_ = true;
    PushValue(isolate,
              context,
              kEventError,
              kErrorException,
              false,
              exception,
              OnCloneError::kSendString);
    // Stop any further JS, including the remaining microtasks.
    isolate->TerminateExecution();
  }

  void CheckUnhandledRejections(Isolate* isolate, Local<Context> context) {
    if (pending_rejections_.empty()) return;
    Local<Value> reason = pending_rejections_.front().second.Get(isolate);
    pending_rejections_.clear();
    ReportUncaughtException(isolate, context, reason);
  }

  static void OnPromiseReject(PromiseRejectMessage message) {
    GreenIsolate* self = current_green_isolate;
    if (self == nullptr) return;
    Isolate* isolate = Isolate::GetCurrent();
    Local<Promise> promise = message.GetPromise();
    auto& pending = self->pending_rejections_;
    if (message.GetEvent() == v8::kPromiseRejectWithNoHandler) {
      Local<Value> value = message.GetValue();
      if (value.IsEmpty()) value = Undefined(isolate);
      pending.emplace_back(Global<Promise>(isolate, promise),
                           Global<Value>(isolate, value));
    } else if (message.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
      std::erase_if(pending, [&](const auto& entry) {
        return entry.first.Get(isolate) == promise;
      });
    }
  }

  // Called for errors that are not caught by a TryCatch, e.g. those thrown
  // by queueMicrotask() callbacks.
  static void OnMessage(Local<Message> message, Local<Value> error) {
    GreenIsolate* self = current_green_isolate;
    if (self == nullptr) return;
    Isolate* isolate = Isolate::GetCurrent();
    self->ReportUncaughtException(isolate, isolate->GetCurrentContext(), error);
  }

  static size_t NearHeapLimit(void* data,
                              size_t current_heap_limit,
                              size_t initial_heap_limit) {
    GreenIsolate* self = static_cast<GreenIsolate*>(data);
    self->out_of_memory_ = true;
    // Give the current GC some leeway, then stop running JS.
    self->isolate_->TerminateExecution();
    return current_heap_limit + kExtraHeapAllowance;
  }

  bool InstallGlobals(Isolate* isolate, Local<Context> context) {
    Local<Object> global = context->Global();
    auto define = [&](Local<Object> target,
                      const char* name,
                      v8::FunctionCallback callback,
                      Local<Value> data = Local<Value>()) {
      Local<String> name_string = OneByteString(isolate, name);
      Local<Function> fn;
      if (!Function::New(
               context, callback, data, 0, ConstructorBehavior::kThrow)
               .ToLocal(&fn)) {
        return false;
      }
      fn->SetName(name_string);
      return target
          ->DefineOwnProperty(
              context, name_string, fn, PropertyAttribute::DontEnum)
          .IsJust();
    };

    if (!define(global, "postMessage", PostMessageCallback) ||
        !define(global, "close", CloseCallback) ||
        !define(global, "queueMicrotask", QueueMicrotaskCallback) ||
        !define(global, "setTimeout", SetTimeoutCallback) ||
        !define(global, "setInterval", SetIntervalCallback) ||
        !define(global, "clearTimeout", ClearTimerCallback) ||
        !define(global, "clearInterval", ClearTimerCallback)) {
      return false;
    }

    Local<Object> console = Object::New(isolate);
    static constexpr const char* kConsoleMethods[] = {
        "log", "info", "warn", "error", "debug"};
    for (size_t i = 0; i < arraysize(kConsoleMethods); i++) {
      if (!define(console,
                  kConsoleMethods[i],
                  ConsoleCallback,
                  Integer::New(isolate, static_cast<int32_t>(i)))) {
        return false;
      }
    }
    return global
               ->DefineOwnProperty(context,
                                   FIXED_ONE_BYTE_STRING(isolate, "console"),
                                   console,
                                   PropertyAttribute::DontEnum)
               .IsJust() &&
           global
               ->Set(context,
                     FIXED_ONE_BYTE_STRING(isolate, "onmessage"),
                     v8::Null(isolate))
               .IsJust();
  }

  static GreenIsolate* Current() {
    CHECK_NOT_NULL(current_green_isolate);
    return current_green_isolate;
  }

  static void PostMessageCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    std::optional<Bytes> bytes = Serialize(isolate, context, args[0], false);
    if (!bytes) return;
    self->host_->Push({kEventMessage, 0, true, std::move(*bytes)});
  }

  static void CloseCallback(const FunctionCallbackInfo<Value>& args) {
    Current()->close_requested_ = true;
  }

  static void QueueMicrotaskCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!args[0]->IsFunction()) {
      isolate->ThrowException(Exception::TypeError(FIXED_ONE_BYTE_STRING(
          isolate, "The \"callback\" argument must be of type function")));
      return;
    }
    isolate->EnqueueMicrotask(args[0].As<Function>());
  }

  static void StartTimer(const FunctionCallbackInfo<Value>& args, bool repeat) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    if (!args[0]->IsFunction()) {
      isolate->ThrowException(Exception::TypeError(FIXED_ONE_BYTE_STRING(
          isolate, "The \"callback\" argument must be of type function")));
      return;
    }
    double delay = 1;
    if (!args[1]->IsUndefined() && !args[1]->NumberValue(context).To(&delay)) {
      return;
    }
    // Match Node.js: out-of-range delays become 1ms.
    if (!(delay >= 1 && delay <= 2147483647)) delay = 1;

    uint32_t id = ++self->next_timer_id_;
    auto timer = std::make_unique<Timer>();
    timer->owner = self;
    timer->id = id;
    timer->repeat = repeat;
    timer->callback.Reset(isolate, args[0].As<Function>());
    CHECK_EQ(uv_timer_init(self->thread_->loop(), &timer->handle), 0);
    const uint64_t timeout = static_cast<uint64_t>(delay);
    uv_timer_start(&timer->handle, OnTimer, timeout, repeat ? timeout : 0);
    self->timers_.emplace(id, std::move(timer));
    args.GetReturnValue().Set(Uint32::NewFromUnsigned(isolate, id));
  }

  static void SetTimeoutCallback(const FunctionCallbackInfo<Value>& args) {
    StartTimer(args, false);
  }
  static void SetIntervalCallback(const FunctionCallbackInfo<Value>& args) {
    StartTimer(args, true);
  }

  static void ClearTimerCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    if (!args[0]->IsNumber()) return;
    double value = args[0].As<Number>()->Value();
    if (!(value >= 1 && value <= UINT32_MAX)) return;
    auto it = self->timers_.find(static_cast<uint32_t>(value));
    if (it == self->timers_.end()) return;
    std::unique_ptr<Timer> timer = std::move(it->second);
    self->timers_.erase(it);
    timer->callback.Reset();
    CloseTimer(timer.release());
  }

  static void CloseTimer(Timer* timer) {
    uv_close(reinterpret_cast<uv_handle_t*>(&timer->handle),
             [](uv_handle_t* handle) {
               Timer* self = ContainerOf(&Timer::handle,
                                         reinterpret_cast<uv_timer_t*>(handle));
               delete self;
             });
  }

  static void OnTimer(uv_timer_t* handle) {
    Timer* timer = ContainerOf(&Timer::handle, handle);
    GreenIsolate* self = timer->owner;
    const uint32_t id = timer->id;
    self->RunTask([&](Isolate* isolate, Local<Context> context) {
      auto it = self->timers_.find(id);
      if (it == self->timers_.end()) return;
      Local<Function> callback = it->second->callback.Get(isolate);
      if (!it->second->repeat) {
        std::unique_ptr<Timer> done = std::move(it->second);
        self->timers_.erase(it);
        done->callback.Reset();
        CloseTimer(done.release());
      }
      USE(callback->Call(context, context->Global(), 0, nullptr));
    });
  }

  static void ConsoleCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    const int32_t level = args.Data().As<Int32>()->Value();

    LocalVector<Value> values(isolate);
    for (int i = 0; i < args.Length(); i++) values.push_back(args[i]);
    Local<Array> array = Array::New(isolate, values.data(), values.size());

    std::optional<Bytes> bytes;
    {
      TryCatch try_catch(isolate);
      bytes = Serialize(isolate, context, array, false);
      if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        return;
      }
    }
    if (!bytes) {
      // Fall back to the string forms of values that can't be cloned.
      for (size_t i = 0; i < values.size(); i++) {
        if (values[i]->IsString()) continue;
        Local<String> string;
        if (!values[i]->ToDetailString(context).ToLocal(&string)) return;
        values[i] = string;
      }
      array = Array::New(isolate, values.data(), values.size());
      bytes = Serialize(isolate, context, array, false);
      if (!bytes) return;
    }
    self->host_->Push({kEventConsole, level, true, std::move(*bytes)});
  }

  MultiIsolatePlatform* const platform_;
  PoolThread* const thread_;
  const std::shared_ptr<HostChannel> host_;
  std::shared_ptr<GreenTaskRunner> task_runner_;

  // Written on the pool thread; read by Terminate() on any thread.
  Mutex isolate_mutex_;
  Isolate* isolate_ = nullptr;

  std::atomic<bool> terminate_requested_{false};

  Mutex disposal_mutex_;
  ConditionVariable disposal_cv_;
  bool disposed_ = false;

  // Pool thread only.
  State state_ = State::kPending;
  Global<Context> context_;
  bool fatal_ = false;
  bool close_requested_ = false;
  std::atomic<bool> out_of_memory_{false};
  uint32_t next_timer_id_ = 0;
  std::unordered_map<uint32_t, std::unique_ptr<Timer>> timers_;
  std::unordered_set<DelayedV8Task*> delayed_v8_tasks_;
  std::vector<std::pair<Global<Promise>, Global<Value>>> pending_rejections_;
};

void GreenTaskRunner::Post(std::unique_ptr<Task> task,
                           double delay_in_seconds) {
  if (std::shared_ptr<GreenIsolate> owner = owner_.lock()) {
    owner->PostV8Task(std::move(task), delay_in_seconds);
  }
}

}  // namespace

void TerminateAll() {
  std::vector<std::shared_ptr<GreenIsolate>> green_isolates;
  {
    Mutex::ScopedLock lock(live_green_isolates_mutex);
    for (auto& [ptr, weak] : live_green_isolates) {
      if (std::shared_ptr<GreenIsolate> green = weak.lock()) {
        green_isolates.push_back(std::move(green));
      }
    }
  }
  for (auto& green : green_isolates) green->Terminate();
  for (auto& green : green_isolates) green->WaitForDisposal();
}

namespace {

// The host side of a green thread.
class GreenThreadWrap final : public HandleWrap {
 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context,
                         void* priv) {
    Environment* env = Environment::GetCurrent(context);
    Isolate* isolate = env->isolate();
    Local<FunctionTemplate> constructor = NewFunctionTemplate(isolate, New);
    constructor->InstanceTemplate()->SetInternalFieldCount(
        GreenThreadWrap::kInternalFieldCount);
    constructor->Inherit(HandleWrap::GetConstructorTemplate(env));

    SetProtoMethod(isolate, constructor, "postMessage", PostMessage);
    SetProtoMethod(isolate, constructor, "evaluate", Evaluate);
    SetProtoMethod(isolate, constructor, "terminate", Terminate);

    SetConstructorFunction(context, target, "GreenThread", constructor);

#define V(name, value)                                                         \
  target                                                                       \
      ->Set(context,                                                           \
            FIXED_ONE_BYTE_STRING(isolate, name),                              \
            Integer::New(isolate, value))                                      \
      .Check();
    V("kEventMessage", kEventMessage)
    V("kEventConsole", kEventConsole)
    V("kEventEvalResult", kEventEvalResult)
    V("kEventError", kEventError)
    V("kEventExit", kEventExit)
    V("kErrorException", kErrorException)
    V("kErrorOutOfMemory", kErrorOutOfMemory)
#undef V
  }

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
    registry->Register(New);
    registry->Register(PostMessage);
    registry->Register(Evaluate);
    registry->Register(Terminate);
  }

  void Close(Local<Value> close_callback) override {
    Detach();
    HandleWrap::Close(close_callback);
  }

  ~GreenThreadWrap() override {
    Detach();
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(GreenThreadWrap)
  SET_SELF_SIZE(GreenThreadWrap)

 private:
  // new GreenThread(source, filename, maxOldGenerationSizeMb,
  //                 maxYoungGenerationSizeMb)
  static void New(const FunctionCallbackInfo<Value>& args) {
    CHECK(args.IsConstructCall());
    Environment* env = Environment::GetCurrent(args);
    CHECK(args[0]->IsString());
    CHECK(args[1]->IsString());
    CHECK(args[2]->IsNumber());
    CHECK(args[3]->IsNumber());

    GreenIsolate::Options options;
    options.source = Utf8Value(env->isolate(), args[0]).ToString();
    options.filename = Utf8Value(env->isolate(), args[1]).ToString();
    options.max_old_generation_size =
        static_cast<size_t>(args[2].As<Number>()->Value() * kMB);
    options.max_young_generation_size =
        static_cast<size_t>(args[3].As<Number>()->Value() * kMB);

    new GreenThreadWrap(env, args.This(), std::move(options));
  }

  GreenThreadWrap(Environment* env,
                  Local<Object> object,
                  GreenIsolate::Options options)
      : HandleWrap(env,
                   object,
                   reinterpret_cast<uv_handle_t*>(&async_),
                   AsyncWrap::PROVIDER_GREENTHREAD) {
    CHECK_EQ(uv_async_init(env->event_loop(), &async_, OnAsync), 0);
    channel_ = std::make_shared<HostChannel>(&async_);
    green_ = std::make_shared<GreenIsolate>(env->isolate_data()->platform(),
                                            channel_);
    green_->Start(std::move(options));
  }

  // Stops the green thread and waits until its isolate is disposed, so that
  // it can never outlive the host Environment.
  void Detach() {
    if (!green_) return;
    channel_->Close();
    green_->Terminate();
    green_->WaitForDisposal();
    green_.reset();
  }

  static void PostMessage(const FunctionCallbackInfo<Value>& args) {
    GreenThreadWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
    if (!wrap->green_) return;
    Environment* env = wrap->env();
    std::optional<Bytes> bytes =
        Serialize(env->isolate(), env->context(), args[0], true);
    if (!bytes) return;
    wrap->green_->PostMessage(std::move(*bytes));
  }

  // evaluate(id, source, filename)
  static void Evaluate(const FunctionCallbackInfo<Value>& args) {
    GreenThreadWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
    CHECK(args[0]->IsInt32());
    CHECK(args[1]->IsString());
    CHECK(args[2]->IsString());
    if (!wrap->green_) return;
    Isolate* isolate = wrap->env()->isolate();
    wrap->green_->Evaluate(args[0].As<Int32>()->Value(),
                           Utf8Value(isolate, args[1]).ToString(),
                           Utf8Value(isolate, args[2]).ToString());
  }

  static void Terminate(const FunctionCallbackInfo<Value>& args) {
    GreenThreadWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
    if (wrap->green_) wrap->green_->Terminate();
  }

  static void OnAsync(uv_async_t* handle) {
    GreenThreadWrap* wrap = ContainerOf(&GreenThreadWrap::async_, handle);
    wrap->OnEvents();
  }

  void OnEvents() {
    std::deque<HostEvent> events = channel_->Take();
    Environment* env = this->env();
    Isolate* isolate = env->isolate();
    HandleScope handle_scope(isolate);
    Local<Context> context = env->context();
    Context::Scope context_scope(context);

    for (HostEvent& event : events) {
      if (IsHandleClosing()) return;
      Local<Value> value = Undefined(isolate);
      if (!event.payload.empty()) {
        TryCatch try_catch(isolate);
        if (!Deserialize(isolate, context, event.payload).ToLocal(&value)) {
          value = Undefined(isolate);
        }
      }
      Local<Value> argv[] = {
          Integer::New(isolate, event.type),
          Integer::New(isolate, event.id),
          Boolean::New(isolate, event.ok),
          value,
      };
      if (MakeCallback(FIXED_ONE_BYTE_STRING(isolate, "onevent").As<Name>(),
                       arraysize(argv),
                       argv)
              .IsEmpty()) {
        return;
      }
    }
  }

  uv_async_t async_;
  std::shared_ptr<HostChannel> channel_;
  std::shared_ptr<GreenIsolate> green_;
};

}  // namespace

}  // namespace green_thread
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    green_thread, node::green_thread::GreenThreadWrap::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    green_thread,
    node::green_thread::GreenThreadWrap::RegisterExternalReferences)
