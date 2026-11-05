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
// The maximum number of work items an isolate runs per slice.
constexpr size_t kSliceBudget = 64;
// The maximum number of slices a pool thread runs before returning to its
// loop.
constexpr size_t kReadyBudget = 16;

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

// Flags passed to the GreenThread constructor.
enum Flags : uint32_t {
  // Installs __greenThreadInfo(), for tests.
  kFlagTestingGlobals = 1 << 0,
};

using Bytes = std::vector<uint8_t>;

class GreenIsolate;
class GreenThreadPool;
class PoolThread;

// NOLINTNEXTLINE(runtime/thread_local)
thread_local PoolThread* current_pool_thread = nullptr;

// A pool thread owns a libuv loop. It runs the callbacks posted to it, in
// order, and slices of the green isolates that are ready to run on any
// thread. Pool threads live until the process exits.
class PoolThread {
 public:
  using Callback = std::function<void()>;

  PoolThread(GreenThreadPool* pool, size_t index)
      : pool_(pool), index_(index) {}

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
              current_pool_thread = self;
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
    Wake();
  }

  // May be called from any thread.
  void Wake() { CHECK_EQ(uv_async_send(&wakeup_), 0); }

  // May be called from any thread.
  void RunOrPost(Callback callback) {
    if (current_pool_thread == this) {
      callback();
    } else {
      Post(std::move(callback));
    }
  }

  bool is_idle() const { return !busy_.load(); }
  size_t index() const { return index_; }
  uv_loop_t* loop() { return &loop_; }
  uintptr_t stack_limit() const { return stack_limit_; }

 private:
  inline static void OnWakeup(uv_async_t* handle);

  bool HasCallbacks() {
    Mutex::ScopedLock lock(mutex_);
    return !queue_.empty();
  }

  GreenThreadPool* const pool_;
  const size_t index_;
  uv_thread_t tid_;
  uv_loop_t loop_;
  uv_async_t wakeup_;
  Mutex mutex_;
  std::deque<Callback> queue_;
  uintptr_t stack_limit_ = 0;
  std::atomic<bool> busy_{false};
};

// The process-wide set of pool threads, and the queue of green isolates that
// are ready to run on any of them. It is created on first use and
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

  size_t size() const { return threads_.size(); }

  // May be called from any thread. Wakes `preferred` if it is idle, so that
  // isolates tend to stay on the same thread, or else any idle thread. If all
  // threads are busy, the first one to finish its work picks `green` up.
  void ScheduleReady(std::shared_ptr<GreenIsolate> green,
                     PoolThread* preferred) {
    {
      Mutex::ScopedLock lock(ready_mutex_);
      ready_.push_back(std::move(green));
    }
    if (preferred != nullptr && preferred->is_idle()) {
      preferred->Wake();
    } else {
      WakeIdleThread(nullptr);
    }
  }

  bool HasReady() {
    Mutex::ScopedLock lock(ready_mutex_);
    return !ready_.empty();
  }

  inline void RunReady(PoolThread* thread, size_t budget);

 private:
  GreenThreadPool() {
    unsigned int count =
        std::clamp(uv_available_parallelism(), 1u, kMaxPoolThreads);
    for (unsigned int i = 0; i < count; i++) {
      threads_.push_back(std::make_unique<PoolThread>(this, i));
    }
    for (auto& thread : threads_) thread->Start();
  }

  void WakeIdleThread(PoolThread* except) {
    const size_t start = next_.fetch_add(1, std::memory_order_relaxed);
    for (size_t i = 0; i < threads_.size(); i++) {
      PoolThread* thread = threads_[(start + i) % threads_.size()].get();
      if (thread != except && thread->is_idle()) {
        thread->Wake();
        return;
      }
    }
  }

  inline std::shared_ptr<GreenIsolate> TakeReady(PoolThread* thread);

  std::vector<std::unique_ptr<PoolThread>> threads_;
  std::atomic<size_t> next_{0};
  Mutex ready_mutex_;
  std::deque<std::shared_ptr<GreenIsolate>> ready_;
};

void PoolThread::OnWakeup(uv_async_t* handle) {
  PoolThread* self = ContainerOf(&PoolThread::wakeup_, handle);
  self->busy_ = true;
  std::deque<Callback> callbacks;
  {
    Mutex::ScopedLock lock(self->mutex_);
    callbacks.swap(self->queue_);
  }
  for (Callback& callback : callbacks) callback();
  self->pool_->RunReady(self, kReadyBudget);

  // Return to the loop so that its timers can run, but come back right away
  // if there is more work.
  if (self->HasCallbacks() || self->pool_->HasReady()) {
    self->Wake();
    return;
  }
  self->busy_ = false;
  // ScheduleReady() may have skipped this thread while it was still busy.
  if (self->pool_->HasReady()) {
    self->busy_ = true;
    self->Wake();
  }
}

// A libuv timer on a given pool thread. It may be started and closed from any
// thread; the handle itself is only touched on its thread, and stays alive
// until libuv is done with it.
class TimerHandle final : public std::enable_shared_from_this<TimerHandle> {
 public:
  TimerHandle(PoolThread* thread,
              uint64_t timeout,
              uint64_t repeat,
              std::function<void()> on_fire)
      : thread_(thread),
        timeout_(timeout),
        repeat_(repeat),
        on_fire_(std::move(on_fire)) {}

  void Start() {
    thread_->RunOrPost([self = shared_from_this()]() {
      if (self->closed_) return;
      CHECK_EQ(uv_timer_init(self->thread_->loop(), &self->handle_), 0);
      self->handle_.data = self.get();
      uv_timer_start(&self->handle_, OnTimer, self->timeout_, self->repeat_);
      self->started_ = true;
      self->self_ = self;
    });
  }

  void Close() {
    thread_->RunOrPost([self = shared_from_this()]() {
      if (self->closed_) return;
      self->closed_ = true;
      self->on_fire_ = nullptr;
      if (!self->started_) return;
      uv_close(reinterpret_cast<uv_handle_t*>(&self->handle_),
               [](uv_handle_t* handle) {
                 static_cast<TimerHandle*>(handle->data)->self_.reset();
               });
    });
  }

 private:
  static void OnTimer(uv_timer_t* handle) {
    TimerHandle* self = static_cast<TimerHandle*>(handle->data);
    // Copy, since the callback may close the timer.
    std::function<void()> on_fire = self->on_fire_;
    if (on_fire) on_fire();
  }

  PoolThread* const thread_;
  const uint64_t timeout_;
  const uint64_t repeat_;
  uv_timer_t handle_;
  // Only accessed on `thread_`.
  std::function<void()> on_fire_;
  bool started_ = false;
  bool closed_ = false;
  std::shared_ptr<TimerHandle> self_;
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

// NOLINTNEXTLINE(runtime/thread_local)
thread_local GreenIsolate* current_green_isolate = nullptr;

// All green isolates that have not been disposed yet, so that they can be
// stopped before the process exits.
// Leaked, since pool threads may still use it while exit() runs static
// destructors.
Mutex& live_green_isolates_mutex = *new Mutex();
std::unordered_map<GreenIsolate*, std::weak_ptr<GreenIsolate>>&
    live_green_isolates =
        *new std::unordered_map<GreenIsolate*, std::weak_ptr<GreenIsolate>>();

// The foreground task runner V8 uses for a green isolate. Tasks are run like
// other work for the isolate, with the isolate locked.
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

// The state of one green thread: a V8 isolate, its invocation contexts and
// their timers.
//
// Work for the isolate is queued in FIFO order. Each item is either unbound,
// and may run on any pool thread, or bound to the pool thread that owns the
// I/O it continues. A pool thread that holds the isolate's (logical) lock
// runs a slice: it runs items from the front of the queue until its budget is
// used up or the next item is bound to another thread. Threads never block
// waiting for an isolate. Except where noted, methods run in a slice, i.e.
// with the logical lock held.
class GreenIsolate final : public IsolatePlatformDelegate,
                           public std::enable_shared_from_this<GreenIsolate> {
 public:
  using Work = std::function<void(GreenIsolate*)>;

  struct Options {
    std::string source;
    std::string filename;
    size_t max_old_generation_size = 0;
    size_t max_young_generation_size = 0;
    bool testing_globals = false;
  };

  GreenIsolate(MultiIsolatePlatform* platform,
               std::shared_ptr<HostChannel> host)
      : platform_(platform), host_(std::move(host)) {}

  // Must be called once, from the host thread, right after construction.
  void Start(Options options) {
    task_runner_ = std::make_shared<GreenTaskRunner>(weak_from_this());
    {
      Mutex::ScopedLock lock(live_green_isolates_mutex);
      live_green_isolates.emplace(this, weak_from_this());
    }
    Submit([options = std::move(options)](GreenIsolate* self) {
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
    Submit([data = std::move(data)](GreenIsolate* self) {
      self->RunTask(kNewInvocation,
                    [&](Isolate* isolate, Local<Context> context) {
                      self->DeliverMessage(isolate, context, data);
                    });
    });
  }

  // May be called from any thread.
  void Evaluate(int32_t id, std::string source, std::string filename) {
    Submit([=](GreenIsolate* self) {
      self->RunTask(
          kNewInvocation, [&](Isolate* isolate, Local<Context> context) {
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
    Mutex::ScopedLock lock(scheduler_mutex_);
    MaybeScheduleLocked();
  }

  // May be called from any thread but a pool thread. Terminates the green
  // thread and waits until its isolate is disposed. Unless a pool thread is
  // running the isolate right now, which termination interrupts, it is shut
  // down on the calling thread, so that this never waits for pool threads
  // that are busy with other isolates.
  void TerminateAndWait() {
    CHECK_NULL(current_pool_thread);
    Terminate();
    bool shut_down_here = false;
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      if (!stopped_ && !locked_) {
        locked_ = true;
        shut_down_here = true;
      }
    }
    if (shut_down_here) {
      Shutdown();
      Mutex::ScopedLock lock(scheduler_mutex_);
      locked_ = false;
    }
    Mutex::ScopedLock lock(disposal_mutex_);
    while (!disposed_) disposal_cv_.Wait(lock);
  }

  // May be called from any thread.
  void PostV8Task(std::unique_ptr<Task> task, double delay_in_seconds) {
    if (delay_in_seconds <= 0) return Submit(MakeV8TaskWork(std::move(task)));

    // A due delayed task may run on any thread; its timer just needs a loop.
    // The timer's callback must not own the task, since the callback may
    // outlive the isolate. Until the task is due, it's owned by
    // delayed_v8_tasks_, and then by the queue, which are both emptied before
    // the isolate is freed.
    uint64_t key;
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      if (stopped_) return;
      key = ++next_delayed_v8_task_id_;
    }
    auto handle = std::make_shared<TimerHandle>(
        GreenThreadPool::Get()->Next(),
        static_cast<uint64_t>(delay_in_seconds * 1000),
        0,
        [weak = weak_from_this(), key]() {
          std::shared_ptr<GreenIsolate> self = weak.lock();
          if (!self) return;
          Mutex::ScopedLock lock(self->scheduler_mutex_);
          auto it = self->delayed_v8_tasks_.find(key);
          if (it == self->delayed_v8_tasks_.end()) return;
          it->second.first->Close();
          self->queue_.push_back(
              {nullptr, MakeV8TaskWork(std::move(it->second.second))});
          self->delayed_v8_tasks_.erase(it);
          self->MaybeScheduleLocked();
        });
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      if (stopped_) return;
      delayed_v8_tasks_.emplace(key, std::make_pair(handle, std::move(task)));
    }
    handle->Start();
  }

  // May be called from any thread.
  PoolThread* last_thread() const { return last_thread_.load(); }

  // Runs a slice on `thread`. Must only be called after MaybeScheduleLocked()
  // has scheduled this isolate.
  void RunSlice(PoolThread* thread) {
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      CHECK(scheduled_);
      scheduled_ = false;
      // TerminateAndWait() took the lock to shut the isolate down.
      if (locked_ || stopped_) return;
      locked_ = true;
    }
    last_thread_ = thread;

    for (size_t n = 0; n < kSliceBudget && !ShouldShutdown(); n++) {
      Work work;
      {
        Mutex::ScopedLock lock(scheduler_mutex_);
        if (queue_.empty()) break;
        PoolThread* bound = queue_.front().thread;
        if (bound != nullptr && bound != thread) break;
        work = std::move(queue_.front().work);
        queue_.pop_front();
      }
      work(this);
    }
    // Only after `work` is gone, since it may own V8 tasks that must not
    // outlive the isolate.
    if (ShouldShutdown()) Shutdown();

    Mutex::ScopedLock lock(scheduler_mutex_);
    locked_ = false;
    MaybeScheduleLocked();
  }

 private:
  enum class State { kPending, kRunning, kStopped };

  // Context IDs passed to RunTask().
  static constexpr uint32_t kNewInvocation = 0;
  static constexpr uint32_t kDefaultInvocation = 1;

  // The context of an invocation: the initial script (the default context),
  // a message, an evaluation, or everything that continues them. Its timers
  // run on the pool thread that ran the invocation, where their handles live.
  struct InvocationContext {
    PoolThread* thread;
    uint32_t pending = 0;
  };

  struct TimerRecord {
    Global<Function> callback;
    bool repeat;
    uint32_t context_id;
    std::shared_ptr<TimerHandle> handle;
    // Set while a fire of this timer is queued, so that an interval whose
    // isolate is busy elsewhere fires once when it's due, not once per
    // missed interval.
    std::shared_ptr<std::atomic<bool>> fire_queued;
  };

  struct QueuedWork {
    PoolThread* thread;
    Work work;
  };

  // Delayed V8 tasks that are not due yet, with their timers. The tasks are
  // owned separately so that they can be destroyed before the isolate.
  using DelayedV8Tasks = std::unordered_map<
      uint64_t,
      std::pair<std::shared_ptr<TimerHandle>, std::unique_ptr<Task>>>;

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

  // May be called from any thread. If `thread` is set, `work` only runs on it.
  void Submit(Work work, PoolThread* thread = nullptr) {
    Mutex::ScopedLock lock(scheduler_mutex_);
    if (stopped_) return;
    queue_.push_back({thread, std::move(work)});
    MaybeScheduleLocked();
  }

  void MaybeScheduleLocked() {
    if (locked_ || scheduled_ || stopped_) return;
    if (queue_.empty() && !terminate_requested_) return;
    scheduled_ = true;
    // Termination doesn't wait for the thread the next item is bound to.
    PoolThread* bound = terminate_requested_ ? nullptr : queue_.front().thread;
    if (bound != nullptr) {
      bound->Post(
          [self = shared_from_this(), bound]() { self->RunSlice(bound); });
    } else {
      GreenThreadPool::Get()->ScheduleReady(shared_from_this(),
                                            last_thread_.load());
    }
  }

  // The returned work owns `task`. Work is only ever owned by the queue or by
  // the slice running it, both of which are gone before the isolate is freed.
  static Work MakeV8TaskWork(std::unique_ptr<Task> task) {
    auto holder = std::make_shared<std::unique_ptr<Task>>(std::move(task));
    return [holder](GreenIsolate* self) {
      std::unique_ptr<Task> task = std::move(*holder);
      self->RunTask(kDefaultInvocation,
                    [&](Isolate*, Local<Context>) { task->Run(); });
    };
  }

  // Whether JS may run (again) in the current task. Once a termination has
  // unwound the stack, IsExecutionTerminating() is false again, and a
  // TerminateExecution() request made while the isolate was unlocked is
  // discarded when the lock is acquired, so the flags are checked as well.
  bool CanRunJS(Isolate* isolate) const {
    return !terminate_requested_ && !fatal_ && !out_of_memory_ &&
           !isolate->IsExecutionTerminating();
  }

  bool ShouldShutdown() const {
    return state_ != State::kStopped &&
           (fatal_ || terminate_requested_ || close_requested_);
  }

  void Initialize(const Options& options) {
    CHECK_EQ(state_, State::kPending);
    PoolThread* thread = current_pool_thread;

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
      return;
    }
    {
      Mutex::ScopedLock lock(isolate_mutex_);
      isolate_ = isolate;
    }

    testing_globals_ = options.testing_globals;
    const uint32_t default_context_id = CreateInvocationContext(thread);
    CHECK_EQ(default_context_id, kDefaultInvocation);
    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);
      isolate->SetStackLimit(thread->stack_limit());
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
    if (fatal_) return;

    state_ = State::kRunning;
    RunTask(kDefaultInvocation, [&](Isolate* isolate, Local<Context> context) {
      Local<Script> script;
      if (!Compile(isolate, context, options.source, options.filename)
               .ToLocal(&script)) {
        return;
      }
      USE(script->Run(context));
    });
  }

  uint32_t CreateInvocationContext(PoolThread* thread) {
    uint32_t id;
    do {
      id = ++next_context_id_;
    } while (id == kNewInvocation || contexts_.contains(id));
    contexts_.emplace(id, InvocationContext{thread});
    return id;
  }

  void MaybeReleaseInvocationContext(uint32_t id) {
    if (id == kDefaultInvocation || id == running_context_id_) return;
    auto it = contexts_.find(id);
    if (it != contexts_.end() && it->second.pending == 0) contexts_.erase(it);
  }

  // The context that JS is currently running in. Promise reactions run in
  // the context they were created in, as long as it's still alive, and
  // otherwise in that of the running task.
  uint32_t CurrentInvocationContextId(Isolate* isolate) {
    Local<v8::Data> data = isolate->GetContinuationPreservedEmbedderData();
    if (!data.IsEmpty() && data->IsValue() && data.As<Value>()->IsUint32()) {
      uint32_t id = data.As<Value>().As<Uint32>()->Value();
      if (contexts_.contains(id)) return id;
    }
    return running_context_id_;
  }

  // Runs `fn` as one macrotask in the given invocation context: with the
  // isolate locked and entered, followed by a microtask checkpoint.
  template <typename Fn>
  void RunTask(uint32_t context_id, Fn&& fn) {
    if (state_ != State::kRunning) return;
    PoolThread* thread = current_pool_thread;
    if (context_id == kNewInvocation) {
      context_id = CreateInvocationContext(thread);
    } else if (!contexts_.contains(context_id)) {
      context_id = kDefaultInvocation;
    }
    running_context_id_ = context_id;

    Isolate* isolate = isolate_;
    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);
      isolate->SetStackLimit(thread->stack_limit());
      HandleScope handle_scope(isolate);
      Local<Context> context = context_.Get(isolate);
      Context::Scope context_scope(context);
      CurrentScope current_scope(this);
      isolate->SetContinuationPreservedEmbedderData(
          Uint32::NewFromUnsigned(isolate, context_id));

      if (CanRunJS(isolate)) {
        TryCatch try_catch(isolate);
        fn(isolate, context);
        if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
          ReportUncaughtException(isolate, context, try_catch.Exception());
        }
      }
      if (CanRunJS(isolate)) isolate->PerformMicrotaskCheckpoint();
      if (CanRunJS(isolate)) CheckUnhandledRejections(isolate, context);
      isolate->SetContinuationPreservedEmbedderData(Undefined(isolate));
    }

    running_context_id_ = 0;
    MaybeReleaseInvocationContext(context_id);
    if (out_of_memory_ && !fatal_) {
      fatal_ = true;
      host_->Push({kEventError, kErrorOutOfMemory, false, {}});
    }
  }

  void RunTimer(uint32_t id) {
    auto it = timers_.find(id);
    if (it == timers_.end()) return;
    it->second.fire_queued->store(false);
    RunTask(it->second.context_id,
            [&](Isolate* isolate, Local<Context> context) {
              auto it = timers_.find(id);
              if (it == timers_.end()) return;
              Local<Function> callback = it->second.callback.Get(isolate);
              if (!it->second.repeat) RemoveTimer(it);
              USE(callback->Call(context, context->Global(), 0, nullptr));
            });
  }

  void RemoveTimer(std::unordered_map<uint32_t, TimerRecord>::iterator it) {
    const uint32_t context_id = it->second.context_id;
    it->second.handle->Close();
    timers_.erase(it);
    auto context = contexts_.find(context_id);
    if (context != contexts_.end()) context->second.pending--;
    MaybeReleaseInvocationContext(context_id);
  }

  void Shutdown() {
    CHECK_NE(state_, State::kStopped);
    state_ = State::kStopped;

    Isolate* isolate = isolate_;
    if (isolate != nullptr) {
      {
        Locker locker(isolate);
        Isolate::Scope isolate_scope(isolate);
        HandleScope handle_scope(isolate);
        for (auto& [id, timer] : timers_) timer.handle->Close();
        timers_.clear();
        contexts_.clear();
        pending_rejections_.clear();
        context_.Reset();
      }
      {
        Mutex::ScopedLock lock(isolate_mutex_);
        isolate_ = nullptr;
      }
      // Like MultiIsolatePlatform::DisposeIsolate(), but the V8 tasks that
      // were never run are destroyed while the isolate still exists.
      isolate->Deinitialize();
      DropQueuedWork();
      platform_->UnregisterIsolate(isolate);
      Isolate::Free(isolate);
    } else {
      DropQueuedWork();
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

  // Stops accepting work and destroys all queued work.
  void DropQueuedWork() {
    std::deque<QueuedWork> queue;
    DelayedV8Tasks delayed;
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      stopped_ = true;
      queue.swap(queue_);
      delayed.swap(delayed_v8_tasks_);
    }
    for (auto& [key, entry] : delayed) entry.first->Close();
    // `queue` and `delayed`, and with them the tasks, are destroyed here.
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
        !define(global, "clearInterval", ClearTimerCallback) ||
        (testing_globals_ &&
         !define(global, "__greenThreadInfo", InfoCallback))) {
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
    const uint64_t timeout = static_cast<uint64_t>(delay);

    // The timer belongs to the current invocation, and its handle lives on
    // the invocation's thread, which is also where its callback runs.
    const uint32_t context_id = self->CurrentInvocationContextId(isolate);
    InvocationContext& invocation = self->contexts_.at(context_id);
    PoolThread* thread = invocation.thread;
    const uint32_t id = ++self->next_timer_id_;
    auto fire_queued = std::make_shared<std::atomic<bool>>(false);
    auto handle = std::make_shared<TimerHandle>(
        thread,
        timeout,
        repeat ? timeout : 0,
        [weak = self->weak_from_this(), id, thread, fire_queued]() {
          if (fire_queued->exchange(true)) return;
          if (std::shared_ptr<GreenIsolate> self = weak.lock()) {
            self->Submit([id](GreenIsolate* self) { self->RunTimer(id); },
                         thread);
          }
        });
    invocation.pending++;
    self->timers_.emplace(
        id,
        TimerRecord{Global<Function>(isolate, args[0].As<Function>()),
                    repeat,
                    context_id,
                    handle,
                    fire_queued});
    handle->Start();
    args.GetReturnValue().Set(Uint32::NewFromUnsigned(isolate, id));
  }

  static void SetTimeoutCallback(const FunctionCallbackInfo<Value>& args) {
    StartTimer(args, false);
  }
  static void SetIntervalCallback(const FunctionCallbackInfo<Value>& args) {
    StartTimer(args, true);
  }

  // Timers can be cleared from any invocation; the handle is closed on its
  // own thread.
  static void ClearTimerCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    if (!args[0]->IsNumber()) return;
    double value = args[0].As<Number>()->Value();
    if (!(value >= 1 && value <= UINT32_MAX)) return;
    auto it = self->timers_.find(static_cast<uint32_t>(value));
    if (it != self->timers_.end()) self->RemoveTimer(it);
  }

  // Only installed for tests: describes where the caller is running.
  static void InfoCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    const uint32_t context_id = self->CurrentInvocationContextId(isolate);
    Local<Name> names[] = {
        FIXED_ONE_BYTE_STRING(isolate, "thread"),
        FIXED_ONE_BYTE_STRING(isolate, "poolSize"),
        FIXED_ONE_BYTE_STRING(isolate, "context"),
        FIXED_ONE_BYTE_STRING(isolate, "contextThread"),
        FIXED_ONE_BYTE_STRING(isolate, "contexts"),
        FIXED_ONE_BYTE_STRING(isolate, "timers"),
    };
    Local<Value> values[] = {
        Number::New(isolate, current_pool_thread->index()),
        Number::New(isolate, GreenThreadPool::Get()->size()),
        Number::New(isolate, context_id),
        Number::New(isolate, self->contexts_.at(context_id).thread->index()),
        Number::New(isolate, self->contexts_.size()),
        Number::New(isolate, self->timers_.size()),
    };
    args.GetReturnValue().Set(Object::New(
        isolate, v8::Null(isolate), names, values, arraysize(names)));
    USE(context);
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
  const std::shared_ptr<HostChannel> host_;
  std::shared_ptr<GreenTaskRunner> task_runner_;

  // Written in slices; read by Terminate() on any thread.
  Mutex isolate_mutex_;
  Isolate* isolate_ = nullptr;

  std::atomic<bool> terminate_requested_{false};
  std::atomic<PoolThread*> last_thread_{nullptr};

  Mutex disposal_mutex_;
  ConditionVariable disposal_cv_;
  bool disposed_ = false;

  // Guarded by scheduler_mutex_.
  Mutex scheduler_mutex_;
  std::deque<QueuedWork> queue_;
  bool locked_ = false;
  bool scheduled_ = false;
  bool stopped_ = false;
  uint64_t next_delayed_v8_task_id_ = 0;
  DelayedV8Tasks delayed_v8_tasks_;

  // Only accessed in slices.
  State state_ = State::kPending;
  Global<Context> context_;
  bool testing_globals_ = false;
  bool fatal_ = false;
  bool close_requested_ = false;
  std::atomic<bool> out_of_memory_{false};
  uint32_t next_context_id_ = 0;
  uint32_t running_context_id_ = 0;
  std::unordered_map<uint32_t, InvocationContext> contexts_;
  uint32_t next_timer_id_ = 0;
  std::unordered_map<uint32_t, TimerRecord> timers_;
  std::vector<std::pair<Global<Promise>, Global<Value>>> pending_rejections_;
};

void GreenTaskRunner::Post(std::unique_ptr<Task> task,
                           double delay_in_seconds) {
  if (std::shared_ptr<GreenIsolate> owner = owner_.lock()) {
    owner->PostV8Task(std::move(task), delay_in_seconds);
  }
}

std::shared_ptr<GreenIsolate> GreenThreadPool::TakeReady(PoolThread* thread) {
  Mutex::ScopedLock lock(ready_mutex_);
  if (ready_.empty()) return nullptr;
  // Prefer an isolate that last ran on this thread, but only look ahead a
  // little, so that the queue stays roughly FIFO.
  constexpr size_t kAffinityLookahead = 8;
  auto end = ready_.begin() + std::min(ready_.size(), kAffinityLookahead);
  auto it = std::find_if(ready_.begin(), end, [&](const auto& green) {
    return green->last_thread() == thread;
  });
  if (it == end) it = ready_.begin();
  std::shared_ptr<GreenIsolate> green = std::move(*it);
  ready_.erase(it);
  return green;
}

void GreenThreadPool::RunReady(PoolThread* thread, size_t budget) {
  for (size_t i = 0; i < budget; i++) {
    std::shared_ptr<GreenIsolate> green = TakeReady(thread);
    if (!green) return;
    // Let another idle thread help with the rest.
    if (HasReady()) WakeIdleThread(thread);
    green->RunSlice(thread);
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
  // Interrupt all of them first, so that none of them keeps a pool thread
  // busy while another one is waited for.
  for (auto& green : green_isolates) green->Terminate();
  for (auto& green : green_isolates) green->TerminateAndWait();
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
    V("kFlagTestingGlobals", kFlagTestingGlobals)
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
  //                 maxYoungGenerationSizeMb, flags)
  static void New(const FunctionCallbackInfo<Value>& args) {
    CHECK(args.IsConstructCall());
    Environment* env = Environment::GetCurrent(args);
    CHECK(args[0]->IsString());
    CHECK(args[1]->IsString());
    CHECK(args[2]->IsNumber());
    CHECK(args[3]->IsNumber());
    CHECK(args[4]->IsUint32());

    GreenIsolate::Options options;
    options.source = Utf8Value(env->isolate(), args[0]).ToString();
    options.filename = Utf8Value(env->isolate(), args[1]).ToString();
    // Clamped, since out-of-range float to integer conversions are undefined.
    auto megabytes = [](Local<Value> value) -> size_t {
      constexpr double kMaxMegabytes = 1 << 20;
      const double megabytes = value.As<Number>()->Value();
      if (!(megabytes > 0)) return 0;
      return static_cast<size_t>(std::min(megabytes, kMaxMegabytes) * kMB);
    };
    options.max_old_generation_size = megabytes(args[2]);
    options.max_young_generation_size = megabytes(args[3]);
    options.testing_globals =
        (args[4].As<Uint32>()->Value() & kFlagTestingGlobals) != 0;

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
    green_->TerminateAndWait();
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
