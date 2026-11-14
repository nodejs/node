#include "ada.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node_builtins.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_messaging.h"
#include "node_mutex.h"
#include "node_snapshot_builder.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Where a pool thread that runs JS which ignores termination can be cut off.
#if defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
#define NODE_GREEN_THREAD_CAN_CUT_OFF 1
#include <pthread.h>
#include <ucontext.h>
#endif

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
using v8::ObjectTemplate;
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
  kEventWarmPoolFull,
  kEventAbandoned,
  kEventRpc,
};

// Values of the `id` field of kEventError events.
enum ErrorKind : int32_t {
  kErrorException = 0,
  kErrorOutOfMemory,
  kErrorCpuLimit,
};

// Flags passed to the GreenThread constructor.
enum Flags : uint32_t {
  // Installs __greenThreadInfo() and __greenThreadBlock(), for tests.
  kFlagTestingGlobals = 1 << 0,
  // Makes the isolate ignore termination, like JS that V8 has compiled into
  // code that doesn't handle interrupts would, for tests.
  kFlagTestingIgnoreTermination = 1 << 1,
};

// How long a green thread has to stop after it has been asked to, before it
// is abandoned, and how long after that the backup watchdog tries to cut off
// the pool thread that runs it, before aborting the process. In nanoseconds;
// UINT64_MAX means never. Set with setUnresponsiveTimeouts().
std::atomic<uint64_t> unresponsive_grace_period{1000 * 1000000ull};
std::atomic<uint64_t> cut_off_period{10000 * 1000000ull};

// Now plus `delay` nanoseconds, in uv_hrtime() time.
uint64_t DeadlineAfter(uint64_t delay) {
  const uint64_t now = uv_hrtime();
  return delay > UINT64_MAX - now ? UINT64_MAX : now + delay;
}
constexpr uint64_t kCutOffRetryInterval = 5 * 1000000ull;

#ifdef NODE_GREEN_THREAD_CAN_CUT_OFF
// A real-time signal, which Node.js doesn't otherwise use, and which JS can't
// install handlers for.
int CutOffSignal() {
  return SIGRTMIN + 7;
}
#endif

using Bytes = std::vector<uint8_t>;
using worker::MessagePortData;
using PortDataList = std::vector<std::unique_ptr<MessagePortData>>;

// A value serialized by Serialize(), with the ArrayBuffers and MessagePorts
// that were transferred and the SharedArrayBuffers that are shared with it.
// It uses the format of Node.js' messages, so that it can be sent through
// MessagePorts.
struct SerializedValue {
  Bytes data;
  std::vector<std::shared_ptr<v8::BackingStore>> array_buffers;
  std::vector<std::shared_ptr<v8::BackingStore>> shared_array_buffers;
  // Taken by Deserialize(). Shared so that work that delivers the value can
  // be copied.
  std::shared_ptr<PortDataList> ports;

  bool empty() const { return data.empty(); }
};

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

  // The green isolate whose work this thread is running, if any, and the one
  // that the backup watchdog wants to cut off. See RunGuarded().
  sigjmp_buf* cut_off_jump() { return &cut_off_jump_; }
  GreenIsolate* running() const { return running_.load(); }
  void set_running(GreenIsolate* green) { running_ = green; }
  GreenIsolate* cut_off_target() const { return cut_off_target_.load(); }
  void set_cut_off_target(GreenIsolate* green) { cut_off_target_ = green; }

  // May be called from any thread. Returns false if cutting threads off is
  // not supported.
  inline bool SendCutOffSignal(GreenIsolate* target);
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
  sigjmp_buf cut_off_jump_;
  std::atomic<GreenIsolate*> running_{nullptr};
  std::atomic<GreenIsolate*> cut_off_target_{nullptr};
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

  // Queues work that only runs when a pool thread has nothing else to do.
  // May be called from any thread.
  void PostBackground(std::function<void()> callback) {
    {
      Mutex::ScopedLock lock(ready_mutex_);
      background_.push_back(std::move(callback));
    }
    WakeIdleThread(nullptr);
  }

  // Whether there is background work that may run now.
  bool HasBackground() {
    Mutex::ScopedLock lock(ready_mutex_);
    return !background_.empty() && background_running_ < MaxBackground();
  }

  // Runs one item of background work, if there is any that may run now.
  void RunBackground() {
    std::function<void()> callback;
    {
      Mutex::ScopedLock lock(ready_mutex_);
      if (background_.empty() || background_running_ >= MaxBackground()) {
        return;
      }
      callback = std::move(background_.front());
      background_.pop_front();
      background_running_++;
    }
    callback();
    Mutex::ScopedLock lock(ready_mutex_);
    background_running_--;
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

  // Background work can take a while, e.g. creating an isolate, and can't be
  // interrupted, so some threads are always left for other work.
  size_t MaxBackground() const {
    return std::max<size_t>(1, threads_.size() / 2);
  }

  std::vector<std::unique_ptr<PoolThread>> threads_;
  std::atomic<size_t> next_{0};
  Mutex ready_mutex_;
  std::deque<std::shared_ptr<GreenIsolate>> ready_;
  std::deque<std::function<void()>> background_;
  size_t background_running_ = 0;
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
  if (!self->HasCallbacks() && !self->pool_->HasReady()) {
    self->pool_->RunBackground();
  }

  // Return to the loop so that its timers can run, but come back right away
  // if there is more work.
  if (self->HasCallbacks() || self->pool_->HasReady() ||
      self->pool_->HasBackground()) {
    self->Wake();
    return;
  }
  self->busy_ = false;
  // ScheduleReady() and PostBackground() may have skipped this thread while
  // it was still busy.
  if (self->pool_->HasReady() || self->pool_->HasBackground()) {
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

// The CPU time used by the calling thread, in nanoseconds.
uint64_t ThreadCpuTime() {
#ifdef CLOCK_THREAD_CPUTIME_ID
  // More precise than getrusage(), which is only updated on scheduler ticks
  // on some systems.
  timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000 +
           static_cast<uint64_t>(ts.tv_nsec);
  }
#endif
  uv_rusage_t usage;
  if (uv_getrusage_thread(&usage) != 0) return 0;
  auto ns = [](const uv_timeval_t& tv) {
    return static_cast<uint64_t>(tv.tv_sec) * 1000000000 +
           static_cast<uint64_t>(tv.tv_usec) * 1000;
  };
  return ns(usage.ru_utime) + ns(usage.ru_stime);
}

// An ArrayBuffer allocator that counts what it has allocated, and refuses
// allocations that would take the isolate over its memory limit, together
// with the size of the JS heap as of the last GC. Free() may be called from
// any thread.
class LimitedAllocator final : public v8::ArrayBuffer::Allocator {
 public:
  // In sandbox builds, memory has to come from the sandbox of the isolate's
  // group.
  LimitedAllocator(size_t limit, const v8::IsolateGroup& group)
#if defined(V8_COMPRESS_POINTERS) &&                                           \
    !defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)
      : allocator_(v8::ArrayBuffer::Allocator::NewDefaultAllocator(group)),
#else
      : allocator_(v8::ArrayBuffer::Allocator::NewDefaultAllocator()),
#endif
        limit_(limit) {
  }

  v8::PageAllocator* GetPageAllocator() override {
    return allocator_->GetPageAllocator();
  }

  void* Allocate(size_t length) override {
    if (!Reserve(length)) return nullptr;
    void* data = allocator_->Allocate(length);
    if (data == nullptr) allocated_ -= length;
    return data;
  }

  void* AllocateUninitialized(size_t length) override {
    if (!Reserve(length)) return nullptr;
    void* data = allocator_->AllocateUninitialized(length);
    if (data == nullptr) allocated_ -= length;
    return data;
  }

  void Free(void* data, size_t length) override {
    allocator_->Free(data, length);
    allocated_ -= length;
  }

  size_t limit() const {
    return limit_;
  }
  size_t allocated() const {
    return allocated_.load();
  }
  void set_heap_used(size_t heap_used) {
    heap_used_ = heap_used;
  }

  // A copy of `store` in memory that doesn't count towards the limit, for
  // ArrayBuffers that are transferred away. Returns nullptr if allocating
  // fails.
  static std::unique_ptr<v8::BackingStore> CopyUntracked(
      std::shared_ptr<LimitedAllocator> self, const v8::BackingStore& store) {
    const size_t length = store.ByteLength();
    void* data = self->allocator_->AllocateUninitialized(length);
    if (data == nullptr && length > 0) return nullptr;
    if (length > 0) memcpy(data, store.Data(), length);
    auto* holder = new std::shared_ptr<LimitedAllocator>(std::move(self));
    return v8::ArrayBuffer::NewBackingStore(
        data,
        length,
        [](void* data, size_t length, void* holder_data) {
          auto* holder =
              static_cast<std::shared_ptr<LimitedAllocator>*>(holder_data);
          (*holder)->allocator_->Free(data, length);
          delete holder;
        },
        holder);
  }

 private:
  // Leaves some of the limit for the heap to grow into before the next GC,
  // which checks the heap and ArrayBuffers together, so that running out of
  // memory through ArrayBuffers throws a RangeError rather than terminating
  // the isolate.
  bool Reserve(size_t length) {
    size_t allocated = allocated_.fetch_add(length) + length;
    if (limit_ == 0 ||
        allocated + heap_used_.load() <= limit_ - limit_ / kHeadroomDivisor) {
      return true;
    }
    allocated_ -= length;
    return false;
  }

  static constexpr size_t kHeadroomDivisor = 16;

  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
  const size_t limit_;
  std::atomic<size_t> allocated_{0};
  std::atomic<size_t> heap_used_{0};
};

// Calls a callback once a deadline (in uv_hrtime() nanoseconds) has passed,
// on its own thread. Used to interrupt green threads that may have used up
// their CPU time. Created on first use and intentionally never destroyed.
class Watchdog {
 public:
  using Callback = std::function<void()>;

  static Watchdog* Get() {
    static Watchdog* watchdog = new Watchdog();
    return watchdog;
  }

  // Replaces the deadline of `key`, if any. May be called from any thread.
  void Arm(const void* key, uint64_t deadline, Callback callback) {
    Mutex::ScopedLock lock(mutex_);
    entries_[key] = {deadline, std::move(callback)};
    cv_.Signal(lock);
  }

  // May be called from any thread. The callback of `key` may still be
  // running, or about to run, when this returns.
  void Disarm(const void* key) {
    Mutex::ScopedLock lock(mutex_);
    entries_.erase(key);
  }

 private:
  struct Entry {
    uint64_t deadline;
    Callback callback;
  };

  Watchdog() {
    CHECK_EQ(
        uv_thread_create(
            &tid_, [](void* arg) { static_cast<Watchdog*>(arg)->Run(); }, this),
        0);
  }

  void Run() {
    uv_thread_setname("GreenWatchdog");
    Mutex::ScopedLock lock(mutex_);
    for (;;) {
      if (entries_.empty()) {
        cv_.Wait(lock);
        continue;
      }
      const uint64_t now = uv_hrtime();
      uint64_t next = UINT64_MAX;
      std::vector<Callback> due;
      for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.deadline <= now) {
          due.push_back(std::move(it->second.callback));
          it = entries_.erase(it);
        } else {
          next = std::min(next, it->second.deadline);
          ++it;
        }
      }
      if (!due.empty()) {
        Mutex::ScopedUnlock unlock(lock);
        for (Callback& callback : due) callback();
        continue;
      }
      cv_.TimedWait(lock, next - now);
    }
  }

  uv_thread_t tid_;
  Mutex mutex_;
  ConditionVariable cv_;
  std::unordered_map<const void*, Entry> entries_;
};

// The private symbol that RpcTargets and RPC stubs are branded with, in both
// green threads and their hosts. See internal/green_threads/rpc.
Local<v8::Private> RpcBrand(Isolate* isolate) {
  return v8::Private::ForApi(
      isolate, FIXED_ONE_BYTE_STRING(isolate, "node:green_thread:rpc"));
}

// The private symbol that holds the ID of a green thread's MessagePort. See
// internal/green_threads/runtime/message_port.
Local<v8::Private> PortIdSymbol(Isolate* isolate) {
  return v8::Private::ForApi(
      isolate, FIXED_ONE_BYTE_STRING(isolate, "node:green_thread:port_id"));
}

// Calls `hooks[name](arg)`, where `hooks` is an RpcSession's hooks: see
// internal/green_threads/runtime/bootstrap and lib/green_threads.js.
MaybeLocal<Value> CallRpcHook(Isolate* isolate,
                              Local<Context> context,
                              Local<Object> hooks,
                              const char* name,
                              Local<Value> arg) {
  Local<Value> hook;
  if (!hooks->Get(context, OneByteString(isolate, name)).ToLocal(&hook)) {
    return {};
  }
  CHECK(hook->IsFunction());
  return hook.As<Function>()->Call(context, hooks, 1, &arg);
}

// How the host or a green thread represents MessagePorts.
class PortAdapter {
 public:
  virtual ~PortAdapter() = default;
  // Whether `object` is a MessagePort, and if so, whether it is detached.
  virtual bool IsPort(Local<Object> object, bool* detached) = 0;
  // Takes the data of a port that is not detached, which detaches it.
  virtual std::unique_ptr<MessagePortData> TakePort(Local<Object> port) = 0;
  // Creates a port for data that was transferred.
  virtual MaybeLocal<Object> CreatePort(
      std::unique_ptr<MessagePortData> data) = 0;
};

// What Node.js' messages use for the host object index of an object that is
// serialized in place, such as process.env.
constexpr uint32_t kNormalObject = static_cast<uint32_t>(-1);

class SerializerDelegate final : public ValueSerializer::Delegate {
 public:
  // With `rpc_hooks`, RpcTargets, RPC stubs and AbortSignals are serialized
  // as indices into descriptors(), which holds what the hooks'
  // exportHostObject() returns for them.
  SerializerDelegate(Isolate* isolate,
                     Local<Context> context,
                     bool is_host,
                     Local<Object> rpc_hooks = {})
      : isolate_(isolate),
        context_(context),
        is_host_(is_host),
        rpc_hooks_(rpc_hooks) {
    if (rpc_hooks_.IsEmpty()) return;
    rpc_brand_ = RpcBrand(isolate);
    Local<Value> prototype;
    if (rpc_hooks_
            ->Get(context,
                  FIXED_ONE_BYTE_STRING(isolate, "abortSignalPrototype"))
            .ToLocal(&prototype)) {
      abort_signal_prototype_ = prototype;
    }
  }

  void set_serializer(ValueSerializer* serializer) { serializer_ = serializer; }
  // Ports in `transfer_ports` are written as their indices in it, as in
  // Node.js' messages.
  void set_ports(PortAdapter* ports,
                 const LocalVector<Object>* transfer_ports) {
    ports_ = ports;
    transfer_ports_ = transfer_ports;
  }
  void set_shared_array_buffers(
      std::vector<std::shared_ptr<v8::BackingStore>>* shared_array_buffers) {
    shared_array_buffers_ = shared_array_buffers;
  }

  Maybe<uint32_t> GetSharedArrayBufferId(
      Isolate* isolate, Local<v8::SharedArrayBuffer> buffer) override {
    std::shared_ptr<v8::BackingStore> store = buffer->GetBackingStore();
    for (size_t i = 0; i < shared_array_buffers_->size(); i++) {
      if ((*shared_array_buffers_)[i] == store) {
        return Just(static_cast<uint32_t>(i));
      }
    }
    shared_array_buffers_->push_back(std::move(store));
    return Just(static_cast<uint32_t>(shared_array_buffers_->size() - 1));
  }

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

  bool HasCustomHostObject(Isolate* isolate) override {
    return !rpc_hooks_.IsEmpty() || ports_ != nullptr;
  }

  Maybe<bool> IsHostObject(Isolate* isolate, Local<Object> object) override {
    // What V8 does without custom host objects.
    if (object->InternalFieldCount() > 0) return Just(true);
    bool detached;
    if (ports_ != nullptr && ports_->IsPort(object, &detached)) {
      return Just(true);
    }
    return IsRpcObject(object);
  }

  Maybe<bool> WriteHostObject(Isolate* isolate, Local<Object> object) override {
    if (ports_ != nullptr) {
      for (size_t i = 0; i < transfer_ports_->size(); i++) {
        if ((*transfer_ports_)[i] == object) {
          serializer_->WriteUint32(static_cast<uint32_t>(i));
          return Just(true);
        }
      }
      bool detached;
      if (ports_->IsPort(object, &detached)) {
        ThrowDataCloneError(FIXED_ONE_BYTE_STRING(
            isolate, "A MessagePort must be in the transfer list to be sent"));
        return Nothing<bool>();
      }
    }
    Maybe<bool> is_rpc_object = IsRpcObject(object);
    if (is_rpc_object.IsNothing()) return Nothing<bool>();
    if (!is_rpc_object.FromJust()) {
      return ValueSerializer::Delegate::WriteHostObject(isolate, object);
    }
    Local<Value> descriptor;
    if (!CallRpcHook(isolate, context_, rpc_hooks_, "exportHostObject", object)
             .ToLocal(&descriptor)) {
      return Nothing<bool>();
    }
    serializer_->WriteUint32(static_cast<uint32_t>(descriptors_.size()));
    // V8 writes each object in a HandleScope of its own.
    descriptors_.emplace_back(isolate, descriptor);
    return Just(true);
  }

  // Once the value is serialized.
  Local<v8::Array> descriptors() {
    if (descriptors_.empty()) return {};
    LocalVector<Value> values(isolate_);
    values.reserve(descriptors_.size());
    for (const Global<Value>& descriptor : descriptors_) {
      values.push_back(descriptor.Get(isolate_));
    }
    return v8::Array::New(isolate_, values.data(), values.size());
  }

 private:
  Maybe<bool> IsRpcObject(Local<Object> object) {
    if (rpc_hooks_.IsEmpty()) return Just(false);
    Maybe<bool> branded = object->HasPrivate(context_, rpc_brand_);
    if (branded.IsNothing() || branded.FromJust()) return branded;
    if (abort_signal_prototype_.IsEmpty()) return Just(false);
    // Like `instanceof AbortSignal`, without calling into JS.
    constexpr int kMaxDepth = 8;
    Local<Value> prototype = object->GetPrototype();
    for (int i = 0; i < kMaxDepth && prototype->IsObject(); i++) {
      if (prototype == abort_signal_prototype_) return Just(true);
      prototype = prototype.As<Object>()->GetPrototype();
    }
    return Just(false);
  }

  Isolate* isolate_;
  Local<Context> context_;
  bool is_host_;
  Local<Object> rpc_hooks_;
  Local<v8::Private> rpc_brand_;
  Local<Value> abort_signal_prototype_;
  ValueSerializer* serializer_ = nullptr;
  std::vector<Global<Value>> descriptors_;
  std::vector<std::shared_ptr<v8::BackingStore>>* shared_array_buffers_ =
      nullptr;
  PortAdapter* ports_ = nullptr;
  const LocalVector<Object>* transfer_ports_ = nullptr;
};

// Reads the host objects written by SerializerDelegate, which were imported
// before deserializing, since JS can't run while V8 deserializes.
class DeserializerDelegate final : public ValueDeserializer::Delegate {
 public:
  DeserializerDelegate(Local<Context> context,
                       Local<v8::Array> objects,
                       const std::vector<std::shared_ptr<v8::BackingStore>>*
                           shared_array_buffers)
      : context_(context),
        objects_(objects),
        shared_array_buffers_(shared_array_buffers) {}

  MaybeLocal<v8::SharedArrayBuffer> GetSharedArrayBufferFromId(
      Isolate* isolate, uint32_t id) override {
    if (shared_array_buffers_ == nullptr ||
        id >= shared_array_buffers_->size()) {
      isolate->ThrowException(Exception::Error(
          FIXED_ONE_BYTE_STRING(isolate, "Invalid SharedArrayBuffer")));
      return {};
    }
    return v8::SharedArrayBuffer::New(isolate, (*shared_array_buffers_)[id]);
  }

  void set_deserializer(ValueDeserializer* deserializer) {
    deserializer_ = deserializer;
  }

  MaybeLocal<Object> ReadHostObject(Isolate* isolate) override {
    uint32_t index;
    Local<Value> object;
    if (!deserializer_->ReadUint32(&index)) return {};
    if (index == kNormalObject) {
      if (!deserializer_->ReadValue(context_).ToLocal(&object)) return {};
      CHECK(object->IsObject());
      return object.As<Object>();
    }
    if (objects_.IsEmpty() || index >= objects_->Length() ||
        !objects_->Get(context_, index).ToLocal(&object) ||
        !object->IsObject()) {
      isolate->ThrowException(Exception::Error(
          FIXED_ONE_BYTE_STRING(isolate, "Invalid RPC host object")));
      return {};
    }
    return object.As<Object>();
  }

 private:
  Local<Context> context_;
  Local<v8::Array> objects_;
  const std::vector<std::shared_ptr<v8::BackingStore>>* shared_array_buffers_;
  ValueDeserializer* deserializer_ = nullptr;
};

// The transfer list of postMessage(value, transfer), where `transfer` is an
// array, or an object with a `transfer` property.
MaybeLocal<Value> GetTransferList(Local<Context> context, Local<Value> arg) {
  if (!arg->IsObject() || arg->IsArray()) return arg;
  return arg.As<Object>()->Get(
      context, FIXED_ONE_BYTE_STRING(Isolate::GetCurrent(), "transfer"));
}

// Serializes `value` alone, adding the SharedArrayBuffers in it to
// `shared_array_buffers`, and transferring the ArrayBuffers in `transfer` and
// the ports in `transfer_ports`.
std::optional<Bytes> SerializeBytes(
    Isolate* isolate,
    Local<Context> context,
    Local<Value> value,
    bool is_host,
    Local<Object> rpc_hooks,
    const LocalVector<v8::ArrayBuffer>& transfer,
    PortAdapter* ports,
    const LocalVector<Object>& transfer_ports,
    std::vector<std::shared_ptr<v8::BackingStore>>* shared_array_buffers,
    Local<v8::Array>* descriptors) {
  SerializerDelegate delegate(isolate, context, is_host, rpc_hooks);
  // Before V8 asks the delegate whether it has custom host objects.
  if (ports != nullptr) delegate.set_ports(ports, &transfer_ports);
  ValueSerializer serializer(isolate, &delegate);
  delegate.set_serializer(&serializer);
  delegate.set_shared_array_buffers(shared_array_buffers);
  serializer.WriteHeader();
  for (size_t i = 0; i < transfer.size(); i++) {
    serializer.TransferArrayBuffer(static_cast<uint32_t>(i), transfer[i]);
  }
  if (serializer.WriteValue(context, value).IsNothing()) return std::nullopt;
  if (descriptors != nullptr) *descriptors = delegate.descriptors();
  std::pair<uint8_t*, size_t> buffer = serializer.Release();
  Bytes bytes(buffer.first, buffer.first + buffer.second);
  free(buffer.first);
  return bytes;
}

struct TransferOptions {
  // An array of what to transfer, or undefined.
  Local<Value> list;
  // If MessagePorts can be transferred, how they are represented.
  PortAdapter* ports = nullptr;
  // The port that sends the value, which can't be transferred.
  Local<Object> source_port;
  // The allocator of a sender with a memory limit. Transferred ArrayBuffers
  // are copied, so that they no longer count towards the limit.
  std::shared_ptr<LimitedAllocator> limited_allocator;
};

// Throws a JS exception and returns std::nullopt if `value` is not cloneable,
// or the transfer list is not an array of distinct, detachable ArrayBuffers
// and ports. With `rpc_hooks`, the data starts with the length of the
// serialized descriptors of its host objects, and those descriptors.
std::optional<SerializedValue> Serialize(Isolate* isolate,
                                         Local<Context> context,
                                         Local<Value> value,
                                         bool is_host,
                                         Local<Object> rpc_hooks = {},
                                         const TransferOptions& options = {}) {
  SerializedValue result;
  LocalVector<v8::ArrayBuffer> transfer(isolate);
  LocalVector<Object> transfer_ports(isolate);
  const Local<Value> transfer_list = options.list;
  const std::shared_ptr<LimitedAllocator>& limited_allocator =
      options.limited_allocator;
  if (!transfer_list.IsEmpty() && !transfer_list->IsUndefined()) {
    SerializerDelegate delegate(isolate, context, is_host);
    auto fail = [&](const char* message) {
      delegate.ThrowDataCloneError(OneByteString(isolate, message));
      return std::nullopt;
    };
    if (!transfer_list->IsArray()) {
      return fail("The transfer list must be an array");
    }
    Local<v8::Array> array = transfer_list.As<v8::Array>();
    for (uint32_t i = 0; i < array->Length(); i++) {
      Local<Value> item;
      if (!array->Get(context, i).ToLocal(&item)) return std::nullopt;
      bool detached = false;
      if (item->IsObject() && options.ports != nullptr &&
          options.ports->IsPort(item.As<Object>(), &detached)) {
        Local<Object> port = item.As<Object>();
        if (port == options.source_port) {
          return fail("Transfer list contains source port");
        }
        if (detached) {
          return fail("MessagePort in transfer list is already detached");
        }
        if (std::find(transfer_ports.begin(), transfer_ports.end(), port) !=
            transfer_ports.end()) {
          return fail("A MessagePort is in the transfer list more than once");
        }
        transfer_ports.push_back(port);
        continue;
      }
      if (!item->IsArrayBuffer()) {
        return fail(
            options.ports != nullptr
                ? "Only ArrayBuffers and MessagePorts can be transferred"
                : "Only ArrayBuffers can be transferred");
      }
      Local<v8::ArrayBuffer> buffer = item.As<v8::ArrayBuffer>();
      if (!buffer->IsDetachable() || buffer->WasDetached()) {
        return fail("An ArrayBuffer is detached or can't be detached");
      }
      if (std::find(transfer.begin(), transfer.end(), buffer) !=
          transfer.end()) {
        return fail("An ArrayBuffer is in the transfer list more than once");
      }
      transfer.push_back(buffer);
    }
  }

  Local<v8::Array> descriptors;
  std::optional<Bytes> bytes =
      SerializeBytes(isolate,
                     context,
                     value,
                     is_host,
                     rpc_hooks,
                     transfer,
                     options.ports,
                     transfer_ports,
                     &result.shared_array_buffers,
                     rpc_hooks.IsEmpty() ? nullptr : &descriptors);
  if (!bytes) return std::nullopt;
  if (!rpc_hooks.IsEmpty()) {
    Bytes descriptor_bytes;
    if (!descriptors.IsEmpty()) {
      std::optional<Bytes> serialized =
          SerializeBytes(isolate,
                         context,
                         descriptors,
                         is_host,
                         {},
                         LocalVector<v8::ArrayBuffer>(isolate),
                         nullptr,
                         LocalVector<Object>(isolate),
                         &result.shared_array_buffers,
                         nullptr);
      if (!serialized) return std::nullopt;
      descriptor_bytes = std::move(*serialized);
    }
    const uint32_t length = descriptor_bytes.size();
    const uint8_t* length_bytes = reinterpret_cast<const uint8_t*>(&length);
    result.data.insert(
        result.data.end(), length_bytes, length_bytes + sizeof(length));
    result.data.insert(
        result.data.end(), descriptor_bytes.begin(), descriptor_bytes.end());
  }
  result.data.insert(result.data.end(), bytes->begin(), bytes->end());

  for (Local<v8::ArrayBuffer> buffer : transfer) {
    std::shared_ptr<v8::BackingStore> store = buffer->GetBackingStore();
    if (limited_allocator && !store->IsResizableByUserJavaScript()) {
      store = LimitedAllocator::CopyUntracked(limited_allocator, *store);
      if (!store) {
        isolate->ThrowException(Exception::RangeError(
            FIXED_ONE_BYTE_STRING(isolate, "Array buffer allocation failed")));
        return std::nullopt;
      }
    }
    result.array_buffers.push_back(std::move(store));
  }
  // Nothing can fail anymore.
  for (Local<v8::ArrayBuffer> buffer : transfer) {
    CHECK(buffer->Detach(Local<Value>()).FromJust());
  }
  if (!transfer_ports.empty()) {
    result.ports = std::make_shared<PortDataList>();
    for (Local<Object> port : transfer_ports) {
      result.ports->push_back(options.ports->TakePort(port));
    }
  }
  return result;
}

MaybeLocal<Value> DeserializeBytes(
    Isolate* isolate,
    Local<Context> context,
    const uint8_t* data,
    size_t size,
    const LocalVector<v8::ArrayBuffer>& transferred,
    const std::vector<std::shared_ptr<v8::BackingStore>>& shared_array_buffers,
    Local<v8::Array> host_objects = {}) {
  DeserializerDelegate delegate(context, host_objects, &shared_array_buffers);
  ValueDeserializer deserializer(isolate, data, size, &delegate);
  delegate.set_deserializer(&deserializer);
  if (deserializer.ReadHeader(context).IsNothing()) return {};
  for (size_t i = 0; i < transferred.size(); i++) {
    deserializer.TransferArrayBuffer(static_cast<uint32_t>(i), transferred[i]);
  }
  return deserializer.ReadValue(context);
}

// Deserializes what Serialize() returned, once. With `rpc_hooks`, imports the
// host objects with the hooks' importHostObjects() first. With
// `copy_array_buffers`, for receivers with a memory limit, transferred
// ArrayBuffers are copied into memory of the isolate's own.
MaybeLocal<Value> Deserialize(Isolate* isolate,
                              Local<Context> context,
                              const SerializedValue& value,
                              Local<Object> rpc_hooks = {},
                              bool copy_array_buffers = false,
                              PortAdapter* ports = nullptr,
                              Local<v8::Array>* port_list = nullptr) {
  Local<v8::Array> port_objects;
  if (value.ports && !value.ports->empty()) {
    CHECK(rpc_hooks.IsEmpty());
    CHECK_NOT_NULL(ports);
    PortDataList data = std::move(*value.ports);
    LocalVector<Value> objects(isolate);
    for (std::unique_ptr<MessagePortData>& port_data : data) {
      Local<Object> object;
      if (!ports->CreatePort(std::move(port_data)).ToLocal(&object)) return {};
      objects.push_back(object);
    }
    port_objects = v8::Array::New(isolate, objects.data(), objects.size());
  }
  if (port_list != nullptr) {
    *port_list =
        port_objects.IsEmpty() ? v8::Array::New(isolate, 0) : port_objects;
  }

  LocalVector<v8::ArrayBuffer> transferred(isolate);
  for (const std::shared_ptr<v8::BackingStore>& store : value.array_buffers) {
    if (!copy_array_buffers || store->IsResizableByUserJavaScript()) {
      transferred.push_back(v8::ArrayBuffer::New(isolate, store));
      continue;
    }
    std::unique_ptr<v8::BackingStore> copy = v8::ArrayBuffer::NewBackingStore(
        isolate,
        store->ByteLength(),
        v8::BackingStoreInitializationMode::kUninitialized,
        v8::BackingStoreOnFailureMode::kReturnNull);
    if (!copy) {
      isolate->ThrowException(Exception::RangeError(
          FIXED_ONE_BYTE_STRING(isolate, "Array buffer allocation failed")));
      return {};
    }
    if (store->ByteLength() > 0) {
      memcpy(copy->Data(), store->Data(), store->ByteLength());
    }
    transferred.push_back(v8::ArrayBuffer::New(isolate, std::move(copy)));
  }

  const Bytes& bytes = value.data;
  if (rpc_hooks.IsEmpty()) {
    return DeserializeBytes(isolate,
                            context,
                            bytes.data(),
                            bytes.size(),
                            transferred,
                            value.shared_array_buffers,
                            port_objects);
  }
  uint32_t length;
  CHECK_GE(bytes.size(), sizeof(length));
  memcpy(&length, bytes.data(), sizeof(length));
  CHECK_LE(length, bytes.size() - sizeof(length));
  const uint8_t* descriptors = bytes.data() + sizeof(length);
  Local<v8::Array> host_objects;
  if (length > 0) {
    Local<Value> descriptor_array;
    Local<Value> objects;
    if (!DeserializeBytes(isolate,
                          context,
                          descriptors,
                          length,
                          LocalVector<v8::ArrayBuffer>(isolate),
                          value.shared_array_buffers)
             .ToLocal(&descriptor_array) ||
        !CallRpcHook(
             isolate, context, rpc_hooks, "importHostObjects", descriptor_array)
             .ToLocal(&objects)) {
      return {};
    }
    CHECK(objects->IsArray());
    host_objects = objects.As<v8::Array>();
  }
  return DeserializeBytes(isolate,
                          context,
                          descriptors + length,
                          bytes.size() - sizeof(length) - length,
                          transferred,
                          value.shared_array_buffers,
                          host_objects);
}

// A Node.js message with the contents of `value`, for MessagePorts.
std::shared_ptr<worker::Message> ToMessage(SerializedValue&& value) {
  MallocedBuffer<char> payload(value.data.size());
  memcpy(payload.data, value.data.data(), value.data.size());
  auto message = std::make_shared<worker::Message>(std::move(payload));
  for (std::shared_ptr<v8::BackingStore>& store : value.array_buffers) {
    message->AddArrayBuffer(std::move(store));
  }
  for (std::shared_ptr<v8::BackingStore>& store : value.shared_array_buffers) {
    message->AddSharedArrayBuffer(std::move(store));
  }
  if (value.ports) {
    for (std::unique_ptr<MessagePortData>& port : *value.ports) {
      message->AddTransferable(std::move(port));
    }
  }
  return message;
}

// The contents of a Node.js message that isn't the close message. Returns
// std::nullopt if it has contents that green threads can't receive: objects
// other than MessagePorts that were transferred or cloned with it, such as
// FileHandles, or WebAssembly modules.
std::optional<SerializedValue> FromMessage(worker::Message* message) {
  if (message->has_wasm_modules_or_shared_values()) return std::nullopt;
  SerializedValue value;
  for (std::unique_ptr<worker::TransferData>& data :
       message->mutable_transferables()) {
    if (data->AsMessagePortData() == nullptr) return std::nullopt;
  }
  const MallocedBuffer<char>& payload = message->payload();
  value.data.assign(payload.data, payload.data + payload.size);
  value.array_buffers = std::move(message->array_buffers());
  value.shared_array_buffers = message->shared_array_buffers();
  if (!message->mutable_transferables().empty()) {
    value.ports = std::make_shared<PortDataList>();
    for (std::unique_ptr<worker::TransferData>& data :
         message->mutable_transferables()) {
      value.ports->emplace_back(data.release()->AsMessagePortData());
    }
    message->mutable_transferables().clear();
  }
  return value;
}

struct HostEvent {
  EventType type;
  int32_t id = 0;
  bool ok = true;
  SerializedValue payload;
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
// The sources and code cache of built-in modules, for pool threads. Set by
// the first Environment that loads the binding; leaked.
Mutex shared_builtin_loader_mutex;
builtins::BuiltinLoader* shared_builtin_loader = nullptr;
// While a snapshot is made outside a pool thread, the loader that the
// bootstrap uses. See GreenIsolate::GetBuiltinLoader().
// NOLINTNEXTLINE(runtime/thread_local)
thread_local builtins::BuiltinLoader* bootstrap_builtin_loader = nullptr;

// Green isolates that didn't stop when asked to, and were abandoned. They are
// never freed. Guarded by live_green_isolates_mutex.
std::vector<std::shared_ptr<GreenIsolate>>* abandoned_green_isolates =
    new std::vector<std::shared_ptr<GreenIsolate>>();

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
enum class IsolateGroupKind : int32_t { kDefault = 0, kNew = 1, kShared = 2 };

// Like Node.js' own isolates, green isolates get a group of their own if the
// build supports that.
IsolateGroupKind DefaultIsolateGroupKind() {
  return v8::IsolateGroup::CanCreateNewGroups() ? IsolateGroupKind::kNew
                                                : IsolateGroupKind::kDefault;
}

// Identifies isolate groups in tests. 0 is V8's default group.
std::atomic<uint64_t> next_isolate_group_id{1};

// A MessagePort of a green thread, which its runtime's MessagePort object
// refers to by its ID. Owned by its green isolate, and only used by it, but
// for OnIncomingMessage().
class GreenPort final : public MessagePortData::Owner {
 public:
  GreenPort(GreenIsolate* green, uint32_t id) : green_(green), id_(id) {}
  ~GreenPort() override { Detach(); }

  void Attach(std::unique_ptr<MessagePortData> data) {
    data_ = std::move(data);
    data_->SetOwner(this);
  }

  // Once this returns, OnIncomingMessage() is no longer called.
  std::unique_ptr<MessagePortData> Detach() {
    if (data_) data_->SetOwner(nullptr);
    return std::move(data_);
  }

  // Null once the port is closed or transferred.
  MessagePortData* data() const { return data_.get(); }

  // May be called from any thread.
  void OnIncomingMessage() override { Notify(); }
  // Makes the green isolate take the port's messages, unless that is queued.
  void Notify();
  // Before taking the port's messages.
  void ClearNotified() { notified_ = false; }

  Global<Object> object;
  // Whether it delivers messages other than the close message.
  bool started = false;

 private:
  // Outlives the port. Abandoned green isolates are never freed.
  GreenIsolate* const green_;
  const uint32_t id_;
  std::unique_ptr<MessagePortData> data_;
  std::atomic<bool> notified_ = false;
};

// The platform delegate of the isolate that creates the green snapshot (see
// GreenIsolate::GreenSnapshot()). Its foreground tasks are never run, since
// the isolate only exists until the snapshot is made, and are destroyed
// with Clear().
class SnapshotPlatformDelegate final : public IsolatePlatformDelegate {
 public:
  std::shared_ptr<TaskRunner> GetForegroundTaskRunner() override {
    return runner_;
  }
  bool IdleTasksEnabled() override { return false; }

  // Must be called after the isolate is deinitialized.
  void Clear() { runner_->Clear(); }

 private:
  class Runner final : public TaskRunner {
   public:
    bool IdleTasksEnabled() override { return false; }
    bool NonNestableTasksEnabled() const override { return true; }
    bool NonNestableDelayedTasksEnabled() const override { return true; }

    void Clear() {
      std::vector<std::unique_ptr<Task>> tasks;
      {
        Mutex::ScopedLock lock(mutex_);
        tasks.swap(tasks_);
      }
    }

   private:
    void PostTaskImpl(std::unique_ptr<Task> task,
                      const SourceLocation& location) override {
      Keep(std::move(task));
    }
    void PostNonNestableTaskImpl(std::unique_ptr<Task> task,
                                 const SourceLocation& location) override {
      Keep(std::move(task));
    }
    void PostDelayedTaskImpl(std::unique_ptr<Task> task,
                             double delay_in_seconds,
                             const SourceLocation& location) override {
      Keep(std::move(task));
    }
    void PostNonNestableDelayedTaskImpl(
        std::unique_ptr<Task> task,
        double delay_in_seconds,
        const SourceLocation& location) override {
      Keep(std::move(task));
    }
    void PostIdleTaskImpl(std::unique_ptr<IdleTask> task,
                          const SourceLocation& location) override {
      UNREACHABLE();
    }

    void Keep(std::unique_ptr<Task> task) {
      Mutex::ScopedLock lock(mutex_);
      tasks_.push_back(std::move(task));
    }

    Mutex mutex_;
    std::vector<std::unique_ptr<Task>> tasks_;
  };

  std::shared_ptr<Runner> runner_ = std::make_shared<Runner>();
};

class GreenIsolate final : public IsolatePlatformDelegate,
                           public PortAdapter,
                           public std::enable_shared_from_this<GreenIsolate> {
 public:
  using Work = std::function<void(GreenIsolate*)>;

  struct Options {
    std::string source;
    std::string filename;
    // Whether `source` is an ES module rather than a script.
    bool is_module = false;
    // What the runtime's initialize() gets, serialized, if anything: the
    // `env` global, the module bundle and the module loader hooks.
    SerializedValue init;
    bool testing_globals = false;
    bool testing_ignore_termination = false;
    // Fixed when the isolate is created.
    size_t max_old_generation_size = 0;
    size_t max_young_generation_size = 0;
    size_t max_memory = 0;
    // In nanoseconds.
    uint64_t max_cpu_time = 0;
    uint64_t max_task_cpu_time = 0;
    // The isolate group to create the isolate in: V8's default group, a new
    // one, or `shared_group`. `group_id` identifies the group in tests.
    IsolateGroupKind group = DefaultIsolateGroupKind();
    std::optional<v8::IsolateGroup> shared_group;
    uint64_t group_id = 0;

    // Whether an isolate created with the default options (i.e. a warm one)
    // can be used.
    bool CanUseDefaultIsolate() const {
      return max_old_generation_size == 0 && max_young_generation_size == 0 &&
             max_memory == 0 && group == DefaultIsolateGroupKind();
    }
  };

  explicit GreenIsolate(MultiIsolatePlatform* platform) : platform_(platform) {}

  // Creates the isolate and its context, using the isolate options from
  // `options`. Must be called once, right after construction. May be called
  // from any thread but a pool thread.
  void Prepare(const Options& options) {
    task_runner_ = std::make_shared<GreenTaskRunner>(weak_from_this());
    {
      Mutex::ScopedLock lock(live_green_isolates_mutex);
      live_green_isolates.emplace(this, weak_from_this());
    }
    Submit([options](GreenIsolate* self) {
      self->Initialize(options);
      {
        Mutex::ScopedLock lock(self->scheduler_mutex_);
        self->low_priority_ = false;
      }
      if (self->on_initialized_) {
        self->on_initialized_(self->state_ == State::kWarm && !self->fatal_);
      }
    });
  }

  // Makes Prepare()'s work background work, which only runs when pool threads
  // have nothing else to do. Must be called before Prepare().
  void set_low_priority() { low_priority_ = true; }

  // Called on a pool thread once Prepare()'s work has run, with whether the
  // isolate is ready to be activated. Must be set before Prepare().
  void set_on_initialized(std::function<void(bool)> on_initialized) {
    on_initialized_ = std::move(on_initialized);
  }

  // Connects the green thread to its host and runs its script, possibly
  // before Prepare()'s work has run. Must be called once. May be called from
  // any thread but a pool thread.
  void Activate(std::shared_ptr<HostChannel> host, Options options) {
    ignore_termination_ = options.testing_ignore_termination;
    {
      Mutex::ScopedLock lock(host_mutex_);
      host_ = std::move(host);
    }
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      if (!stopped_) {
        queue_.push_back(
            {nullptr, [options = std::move(options)](GreenIsolate* self) {
               self->Run(options);
             }});
        MaybeScheduleLocked();
        return;
      }
    }
    // It failed to initialize, or was terminated, before it had a host.
    PushExit(1);
  }

  // IsolatePlatformDelegate. May be called from any thread.
  std::shared_ptr<TaskRunner> GetForegroundTaskRunner() override {
    return task_runner_;
  }
  bool IdleTasksEnabled() override { return false; }

  // May be called from any thread.
  void PostMessage(SerializedValue data) {
    Submit([data = std::move(data)](GreenIsolate* self) {
      self->RunTask(kNewInvocation,
                    [&](Isolate* isolate, Local<Context> context) {
                      self->DeliverMessage(isolate, context, data);
                    });
    });
  }

  // May be called from any thread. `context_id` is the invocation context
  // of the call that this is the result of, or 0.
  void PostRpc(SerializedValue data, uint32_t context_id) {
    Submit([data = std::move(data), context_id](GreenIsolate* self) {
      self->RunTask(context_id == 0 ? kNewInvocation : context_id,
                    [&](Isolate* isolate, Local<Context> context) {
                      self->DeliverRpc(isolate, context, data);
                    });
      if (context_id == 0) return;
      auto it = self->contexts_.find(context_id);
      if (it != self->contexts_.end() && it->second.pending > 0) {
        it->second.pending--;
        self->MaybeReleaseInvocationContext(context_id);
      }
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

  // May be called from any thread. Delivers the messages of a port.
  void DrainPortLater(uint32_t id) {
    Submit([id](GreenIsolate* self) { self->DrainPort(id); });
  }

  // May be called from any thread. Interrupts running JS, if any.
  void Terminate() {
    terminate_requested_ = true;
    {
      Mutex::ScopedLock lock(isolate_mutex_);
      if (isolate_ != nullptr) TerminateExecution(isolate_);
    }
    Mutex::ScopedLock lock(scheduler_mutex_);
    MaybeScheduleLocked();
  }

  // May be called from any thread but a pool thread. Terminates the green
  // thread and waits until its isolate is disposed. Unless a pool thread is
  // running the isolate right now, which termination interrupts, it is shut
  // down on the calling thread, so that this never waits for pool threads
  // that are busy with other isolates. If the green thread doesn't stop, this
  // waits until it is abandoned, or, if `wait_for_thread` is true, until the
  // pool thread that runs it has been cut off, or the process aborted.
  void TerminateAndWait(bool wait_for_thread) {
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
    while (!disposed_ && !cut_off_ && (wait_for_thread || !abandoned_)) {
      disposal_cv_.Wait(lock);
    }
  }

  // Whether ArrayBuffers that are transferred to or from this isolate must be
  // copied, so that they count towards its memory limit only while it uses
  // them.
  bool has_memory_limit() const {
    return allocator_ != nullptr && allocator_->limit() > 0;
  }

  // Code that V8 generated for this isolate, or V8's built-in code. While a
  // thread runs such code, it holds no locks. Read by signal handlers.
  bool IsGeneratedCode(uintptr_t pc) const {
    auto contains = [pc](const std::pair<uintptr_t, size_t>& range) {
      return pc >= range.first && pc - range.first < range.second;
    };
    return contains(code_range_) || contains(embedded_code_range_);
  }

  // Runs `work`, unless the backup watchdog cuts it off, in which case this
  // returns false: the signal handler then jumps back here, leaving behind
  // the frames of the work, which never return.
  static V8_NOINLINE bool RunGuarded(PoolThread* thread,
                                     GreenIsolate* green,
                                     Work* work) {
    if (sigsetjmp(*thread->cut_off_jump(), 0) != 0) return false;
    thread->set_running(green);
    (*work)(green);
    thread->set_running(nullptr);
    return true;
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
      work_epoch_++;
      running_thread_ = thread;
      const bool completed = RunGuarded(thread, this, &work);
      running_thread_ = nullptr;
      if (!completed) return OnCutOff(thread, std::move(work));
      work_epoch_++;
    }
    // Only after `work` is gone, since it may own V8 tasks that must not
    // outlive the isolate.
    if (ShouldShutdown()) Shutdown();

    Mutex::ScopedLock lock(scheduler_mutex_);
    locked_ = false;
    MaybeScheduleLocked();
  }

 private:
  // kWarm: initialized, but not activated yet.
  enum class State { kPending, kWarm, kRunning, kStopped };

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

  // The ES modules of the green thread. The runtime's module loader
  // (internal/green_threads/runtime/modules) refers to them by their IDs.
  struct ModuleRecord {
    Global<v8::Module> module;
    std::string url;
    // The value of a JSON module.
    Global<Value> json;
    // The modules that its requests resolve to, in the order of
    // GetModuleRequests(), once the loader has linked it.
    std::vector<uint32_t> dependencies;
    bool linked = false;
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
    } else if (low_priority_) {
      GreenThreadPool::Get()->PostBackground([self = shared_from_this()]() {
        self->RunSlice(current_pool_thread);
      });
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
           !cpu_limit_exceeded_ && !isolate->IsExecutionTerminating();
  }

  bool ShouldShutdown() const {
    return state_ != State::kStopped &&
           (fatal_ || terminate_requested_ || close_requested_);
  }

  void Initialize(const Options& options) {
    CHECK_EQ(state_, State::kPending);
    PoolThread* thread = current_pool_thread;

    Isolate::CreateParams params;
    v8::IsolateGroup group = v8::IsolateGroup::GetDefault();
    group_id_ = 0;
    if (options.group == IsolateGroupKind::kNew) {
      group = v8::IsolateGroup::Create();
      group_id_ = next_isolate_group_id++;
    } else if (options.group == IsolateGroupKind::kShared) {
      group = *options.shared_group;
      group_id_ = options.group_id;
    }
    // Isolates in V8's default group must use the snapshot that all other
    // isolates in it use, Node.js' own. Groups that only contain green
    // isolates can use V8's built-in snapshot, which is much smaller, since
    // green threads don't need any of Node.js' per-isolate state.
    uses_node_snapshot_ = group == v8::IsolateGroup::GetDefault();
    // Isolates in the same group must all use the same snapshot, which the
    // green snapshot is once it exists. Without it, V8's.
    const v8::StartupData* green_snapshot =
        uses_node_snapshot_ ? nullptr : GreenSnapshot(platform_);
    if (green_snapshot != nullptr) {
      params.snapshot_blob = green_snapshot;
      params.external_references = SnapshotExternalReferences();
    }
    allocator_ = std::make_shared<LimitedAllocator>(options.max_memory, group);
    params.array_buffer_allocator_shared = allocator_;
    size_t max_old_generation_size = options.max_old_generation_size;
    if (max_old_generation_size == 0) {
      max_old_generation_size = options.max_memory;
    }
    if (max_old_generation_size > 0) {
      params.constraints.set_max_old_generation_size_in_bytes(
          max_old_generation_size);
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

    Isolate* isolate = NewIsolate(
        &params, this, platform_, settings, group, uses_node_snapshot_);
    if (isolate == nullptr) {
      fatal_ = true;
      return;
    }
    // It would block the pool thread, and with it other green threads.
    isolate->SetAllowAtomicsWait(false);
    isolate->SetHostImportModuleDynamicallyCallback(ImportModuleDynamically);
    isolate->SetHostInitializeImportMetaObjectCallback(InitializeImportMeta);
    {
      Mutex::ScopedLock lock(isolate_mutex_);
      isolate_ = isolate;
    }
    {
      void* start;
      size_t length;
      isolate->GetCodeRange(&start, &length);
      code_range_ = {reinterpret_cast<uintptr_t>(start), length};
      const void* embedded_start;
      isolate->GetEmbeddedCodeRange(&embedded_start, &length);
      embedded_code_range_ = {reinterpret_cast<uintptr_t>(embedded_start),
                              length};
    }

    const uint32_t default_context_id = CreateInvocationContext(thread);
    CHECK_EQ(default_context_id, kDefaultInvocation);
    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);
      isolate->SetStackLimit(thread->stack_limit());
      HandleScope handle_scope(isolate);
      isolate->AddNearHeapLimitCallback(NearHeapLimit, this);
      if (options.max_memory > 0) {
        isolate->AddGCEpilogueCallback(AfterGarbageCollection, this);
      }
      isolate->AddMessageListenerWithErrorLevel(
          OnMessage, Isolate::MessageErrorLevel::kMessageError);
      // The green snapshot has a context with the runtime bootstrapped, and
      // so does Node.js' own, unless it was built by users. Otherwise, the
      // default context of the startup snapshot contains only what V8
      // creates.
      Local<Context> context;
      bool bootstrapped = false;
      if (green_snapshot != nullptr) {
        USE(Context::FromSnapshot(isolate, kGreenSnapshotContextIndex)
                .ToLocal(&context));
        bootstrapped = true;
      } else if (uses_node_snapshot_ &&
                 Context::FromSnapshot(isolate,
                                       SnapshotData::kNodeGreenContextIndex)
                     .ToLocal(&context)) {
        bootstrapped = true;
      } else {
        context = Context::New(isolate);
      }
      if (context.IsEmpty()) {
        fatal_ = true;
      } else {
        Context::Scope context_scope(context);
        context_.Reset(isolate, context);
        Local<Object> runtime;
        if (bootstrapped) {
          if (!context
                   ->GetDataFromSnapshotOnce<Object>(kGreenSnapshotRuntimeIndex)
                   .ToLocal(&runtime)) {
            fatal_ = true;
          }
        } else if (!InstallGlobals(isolate, context) ||
                   !RunBootstrap(isolate, context).ToLocal(&runtime)) {
          fatal_ = true;
        }
        if (!fatal_) runtime_.Reset(isolate, runtime);
        // Until the first GC, ArrayBuffer allocations are checked against
        // the heap as it is now.
        v8::HeapStatistics stats;
        isolate->GetHeapStatistics(&stats);
        allocator_->set_heap_used(stats.used_heap_size());
      }
    }
    if (fatal_) return;
    state_ = State::kWarm;
  }

  // Runs the green thread's script. Always queued after Initialize().
  void Run(const Options& options) {
    if (state_ != State::kWarm) return;
    state_ = State::kRunning;
    max_cpu_time_ = options.max_cpu_time;
    max_task_cpu_time_ = options.max_task_cpu_time;
    RunTask(kDefaultInvocation, [&](Isolate* isolate, Local<Context> context) {
      if (CallRuntime(isolate, context, "start", 0, nullptr).IsEmpty()) {
        return;
      }
      if (options.testing_globals && (!DefineFunction(isolate,
                                                      context,
                                                      context->Global(),
                                                      "__greenThreadInfo",
                                                      InfoCallback) ||
                                      !DefineFunction(isolate,
                                                      context,
                                                      context->Global(),
                                                      "__greenThreadBlock",
                                                      BlockCallback))) {
        return;
      }
      if (!options.init.empty()) {
        Local<Value> init;
        if (!Deserialize(isolate, context, options.init, runtime_.Get(isolate))
                 .ToLocal(&init) ||
            CallRuntime(isolate, context, "initialize", 1, &init).IsEmpty()) {
          return;
        }
      }
      if (options.is_module) {
        Local<Value> argv[2];
        if (!ToV8Value(context, options.source, isolate).ToLocal(&argv[0]) ||
            !ToV8Value(context, options.filename, isolate).ToLocal(&argv[1])) {
          return;
        }
        USE(CallRuntime(isolate, context, "runMain", arraysize(argv), argv));
        return;
      }
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

    const bool cpu_limited = max_cpu_time_ > 0 || max_task_cpu_time_ > 0;
    if (cpu_limited) StartCpuAccounting();

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
    if (cpu_limited) StopCpuAccounting();
    if (out_of_memory_ && !fatal_) {
      fatal_ = true;
      PushEvent({kEventError, kErrorOutOfMemory, false, {}});
    }
    if (cpu_limit_exceeded_ && !fatal_) {
      fatal_ = true;
      PushEvent({kEventError, kErrorCpuLimit, false, {}});
    }
  }

  // CPU time accounting. Each task gets the smaller of the per-task limit and
  // what is left of the overall limit. While a task runs, the watchdog is
  // armed for the earliest point in time at which the task's budget could be
  // used up: a thread can't use more CPU time than wall time. It then
  // interrupts the isolate, which checks the CPU time of its thread and
  // either terminates the task or re-arms the watchdog.
  void StartCpuAccounting() {
    uint64_t budget = UINT64_MAX;
    if (max_task_cpu_time_ > 0) budget = max_task_cpu_time_;
    if (max_cpu_time_ > 0) {
      budget = std::min(
          budget,
          max_cpu_time_ > cpu_time_used_ ? max_cpu_time_ - cpu_time_used_ : 0);
    }
    task_cpu_budget_ = budget;
    task_cpu_start_ = ThreadCpuTime();
    in_cpu_limited_task_ = true;
    if (budget == 0) {
      cpu_limit_exceeded_ = true;
    } else {
      ArmWatchdog(budget);
    }
  }

  void StopCpuAccounting() {
    in_cpu_limited_task_ = false;
    Watchdog::Get()->Disarm(this);
    const uint64_t used = ThreadCpuTime() - task_cpu_start_;
    cpu_time_used_ += used;
    if (used >= task_cpu_budget_) cpu_limit_exceeded_ = true;
  }

  void ArmWatchdog(uint64_t remaining) {
    const uint64_t now = uv_hrtime();
    const uint64_t deadline =
        remaining > UINT64_MAX - now ? UINT64_MAX : now + remaining;
    Watchdog::Get()->Arm(this, deadline, [weak = weak_from_this()]() {
      std::shared_ptr<GreenIsolate> self = weak.lock();
      if (!self) return;
      {
        Mutex::ScopedLock lock(self->isolate_mutex_);
        if (self->isolate_ == nullptr) return;
        self->isolate_->RequestInterrupt(CheckCpuTime, self.get());
      }
      // V8 may have compiled the running code in a way that doesn't handle
      // interrupts, and then the CPU time is never checked.
      const uint64_t requested = ++self->cpu_checks_requested_;
      const uint64_t epoch = self->work_epoch_.load();
      Watchdog::Get()->Arm(
          &self->cpu_checks_requested_,
          DeadlineAfter(unresponsive_grace_period.load()),
          [weak = self->weak_from_this(), requested, epoch]() {
            if (std::shared_ptr<GreenIsolate> self = weak.lock()) {
              self->OnCpuCheckOverdue(requested, epoch);
            }
          });
    });
  }

  // On the watchdog thread, a grace period after the CPU time check
  // `requested` was requested.
  void OnCpuCheckOverdue(uint64_t requested, uint64_t epoch) {
    if (work_epoch_.load() != epoch ||
        cpu_checks_serviced_.load() >= requested) {
      return;
    }
    cpu_limit_exceeded_ = true;
    {
      Mutex::ScopedLock lock(isolate_mutex_);
      if (isolate_ != nullptr) TerminateExecution(isolate_);
    }
    // It has had its grace period already.
    OnUnresponsive(epoch);
  }

  // Runs on the thread that runs the isolate, with the isolate locked. It may
  // run in a later task than the one it was requested for.
  static void CheckCpuTime(Isolate* isolate, void* data) {
    GreenIsolate* self = static_cast<GreenIsolate*>(data);
    self->cpu_checks_serviced_ = self->cpu_checks_requested_.load();
    if (!self->in_cpu_limited_task_) return;
    const uint64_t used = ThreadCpuTime() - self->task_cpu_start_;
    if (used >= self->task_cpu_budget_) {
      self->cpu_limit_exceeded_ = true;
      self->TerminateExecution(isolate);
    } else {
      // Don't check more often than that, in case the CPU time clock is
      // coarse. A task may overrun its budget by about as much.
      constexpr uint64_t kMinCheckInterval = 1000000;
      self->ArmWatchdog(
          std::max(self->task_cpu_budget_ - used, kMinCheckInterval));
    }
  }

  // Enforces the memory limit for the heap and ArrayBuffers together.
  static void AfterGarbageCollection(Isolate* isolate,
                                     v8::GCType,
                                     v8::GCCallbackFlags,
                                     void* data) {
    GreenIsolate* self = static_cast<GreenIsolate*>(data);
    v8::HeapStatistics stats;
    isolate->GetHeapStatistics(&stats);
    self->allocator_->set_heap_used(stats.used_heap_size());
    if (stats.used_heap_size() + self->allocator_->allocated() >
        self->allocator_->limit()) {
      self->out_of_memory_ = true;
      self->TerminateExecution(isolate);
    }
  }

  // Asks V8 to stop running JS, and arms the backup watchdog in case it
  // doesn't, which can happen if V8 compiled the running code in a way that
  // doesn't handle interrupts. May be called from any thread.
  void TerminateExecution(Isolate* isolate) {
    if (!ignore_termination_) isolate->TerminateExecution();
    const uint64_t epoch = work_epoch_.load();
    // Nothing is running; anything that runs next checks CanRunJS().
    if (epoch % 2 == 0) return;
    if (backup_epoch_.exchange(epoch) == epoch) return;
    Watchdog::Get()->Arm(
        &backup_epoch_,
        DeadlineAfter(unresponsive_grace_period.load()),
        [weak = weak_from_this(), epoch]() {
          if (std::shared_ptr<GreenIsolate> self = weak.lock()) {
            self->OnUnresponsive(epoch);
          }
        });
  }

  // On the watchdog thread, once the green thread has had its grace period
  // to stop the work that was running when termination was requested.
  void OnUnresponsive(uint64_t epoch) {
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      if (work_epoch_.load() != epoch || abandoned_) return;
      // No more work is accepted. Should the work stop on its own after all,
      // the slice still shuts the isolate down.
      abandoned_ = true;
      stopped_ = true;
    }
    // It must never be freed: the platform and V8 keep pointers to it.
    {
      Mutex::ScopedLock lock(live_green_isolates_mutex);
      abandoned_green_isolates->push_back(shared_from_this());
    }
    if (cpu_limit_exceeded_) {
      PushEvent({kEventError, kErrorCpuLimit, false, {}});
    } else if (out_of_memory_) {
      PushEvent({kEventError, kErrorOutOfMemory, false, {}});
    }
    PushEvent({kEventAbandoned,
               static_cast<int32_t>(unresponsive_grace_period.load() / 1000000),
               true,
               {}});
    PushExit(1);
    {
      Mutex::ScopedLock lock(disposal_mutex_);
      disposal_cv_.Broadcast(lock);
    }
    cut_off_deadline_ = DeadlineAfter(cut_off_period.load());
    TryCutOff(epoch);
  }

  // On the watchdog thread. Tries to cut off the pool thread that is stuck in
  // work `epoch` until it no longer is, or aborts the process.
  void TryCutOff(uint64_t epoch) {
    if (work_epoch_.load() != epoch) return;
    if (cut_off_deadline_ != UINT64_MAX && uv_hrtime() >= cut_off_deadline_) {
      OnFatalError("node::green_thread::GreenIsolate::TryCutOff()",
                   "A green thread ignored termination, and the thread "
                   "running it could not be stopped.");
    }
    if (PoolThread* thread = running_thread_.load()) {
      thread->SendCutOffSignal(this);
    }
    Watchdog::Get()->Arm(&backup_epoch_,
                         uv_hrtime() + kCutOffRetryInterval,
                         [weak = weak_from_this(), epoch]() {
                           if (auto self = weak.lock()) self->TryCutOff(epoch);
                         });
  }

  // On `thread`, after the signal handler cut off `work`. The isolate is left
  // as it was, in the middle of running JS, and is never used again. Only
  // the state of this thread is cleaned up, so that it can run other
  // isolates.
  void OnCutOff(PoolThread* thread, Work&& work) {
    // Nothing that the work may have left inconsistent is destroyed.
    new Work(std::move(work));
    thread->set_running(nullptr);
    thread->set_cut_off_target(nullptr);
#ifdef NODE_GREEN_THREAD_CAN_CUT_OFF
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, CutOffSignal());
    pthread_sigmask(SIG_UNBLOCK, &set, nullptr);
#endif
    current_green_isolate = nullptr;
    for (int i = 0; i < 8 && v8::Isolate::TryGetCurrent() == isolate_; i++) {
      isolate_->Exit();
    }
    CHECK_NE(v8::Isolate::TryGetCurrent(), isolate_);
    in_cpu_limited_task_ = false;
    Watchdog::Get()->Disarm(this);
    Watchdog::Get()->Disarm(&backup_epoch_);
    // Timers live on pool threads' loops. Their records hold V8 handles, so
    // they stay.
    for (auto& [id, timer] : timers_) timer.handle->Close();
    {
      Mutex::ScopedLock lock(scheduler_mutex_);
      for (auto& [key, entry] : delayed_v8_tasks_) entry.first->Close();
      // `locked_` stays set, so nothing ever runs or shuts the isolate down.
      stopped_ = true;
    }
    work_epoch_++;
    {
      Mutex::ScopedLock lock(live_green_isolates_mutex);
      live_green_isolates.erase(this);
    }
    Mutex::ScopedLock lock(disposal_mutex_);
    cut_off_ = true;
    disposal_cv_.Broadcast(lock);
  }

  // May be called from any thread. Events are dropped while there is no host.
  void PushEvent(HostEvent&& event) {
    Mutex::ScopedLock lock(host_mutex_);
    if (host_) host_->Push(std::move(event));
  }

  // May be called from any thread. Only the first exit event is sent.
  void PushExit(int32_t code) {
    if (exit_pushed_.exchange(true)) return;
    PushEvent({kEventExit, code, true, {}});
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
        // Closes them, which closes the ports they are entangled with.
        ports_.clear();
        modules_.clear();
        modules_by_hash_.clear();
        contexts_.clear();
        pending_rejections_.clear();
        runtime_.Reset();
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
    PushExit(clean_exit ? 0 : 1);

    {
      Mutex::ScopedLock lock(live_green_isolates_mutex);
      live_green_isolates.erase(this);
      // Its work stopped after all. Callers of Shutdown() hold a reference.
      if (abandoned_) {
        std::erase_if(*abandoned_green_isolates, [this](const auto& green) {
          return green.get() == this;
        });
      }
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
                      const SerializedValue& data) {
    Local<Value> handler;
    if (!context->Global()
             ->Get(context, FIXED_ONE_BYTE_STRING(isolate, "onmessage"))
             .ToLocal(&handler) ||
        !handler->IsFunction()) {
      return;
    }
    Local<Value> value;
    Local<v8::Array> ports;
    if (!Deserialize(
             isolate, context, data, {}, has_memory_limit(), this, &ports)
             .ToLocal(&value)) {
      return;
    }
    Local<Object> event = Object::New(isolate);
    if (event->Set(context, FIXED_ONE_BYTE_STRING(isolate, "data"), value)
            .IsNothing() ||
        event->Set(context, FIXED_ONE_BYTE_STRING(isolate, "ports"), ports)
            .IsNothing()) {
      return;
    }
    Local<Value> argv[] = {event};
    USE(handler.As<Function>()->Call(
        context, context->Global(), arraysize(argv), argv));
  }

  void DeliverRpc(Isolate* isolate,
                  Local<Context> context,
                  const SerializedValue& data) {
    Local<Object> runtime = runtime_.Get(isolate);
    Local<Value> message;
    if (!Deserialize(isolate, context, data, runtime, has_memory_limit())
             .ToLocal(&message)) {
      return;
    }
    USE(CallRpcHook(isolate, context, runtime, "onRpc", message));
  }

  // Calls `runtime_[name](...argv)`.
  MaybeLocal<Value> CallRuntime(Isolate* isolate,
                                Local<Context> context,
                                const char* name,
                                int argc,
                                Local<Value> argv[]) {
    Local<Object> runtime = runtime_.Get(isolate);
    Local<Value> function;
    if (!runtime->Get(context, OneByteString(isolate, name))
             .ToLocal(&function)) {
      return {};
    }
    CHECK(function->IsFunction());
    return function.As<Function>()->Call(context, runtime, argc, argv);
  }

  // The ID of a port object, or 0 if `object` is not one.
  uint32_t PortId(Local<Object> object) {
    Isolate* isolate = isolate_;
    Local<Value> id;
    if (!object->GetPrivate(isolate->GetCurrentContext(), PortIdSymbol(isolate))
             .ToLocal(&id) ||
        !id->IsUint32()) {
      return 0;
    }
    return id.As<Uint32>()->Value();
  }

  GreenPort* FindPort(Local<Value> object) {
    if (!object->IsObject()) return nullptr;
    auto it = ports_.find(PortId(object.As<Object>()));
    return it == ports_.end() ? nullptr : it->second.get();
  }

  // PortAdapter.
  bool IsPort(Local<Object> object, bool* detached) override {
    const uint32_t id = PortId(object);
    if (id == 0) return false;
    auto it = ports_.find(id);
    *detached = it == ports_.end() || it->second->data() == nullptr;
    return true;
  }

  std::unique_ptr<MessagePortData> TakePort(Local<Object> object) override {
    auto it = ports_.find(PortId(object));
    CHECK(it != ports_.end());
    std::unique_ptr<MessagePortData> data = it->second->Detach();
    ports_.erase(it);
    return data;
  }

  MaybeLocal<Object> CreatePort(
      std::unique_ptr<MessagePortData> data) override {
    Isolate* isolate = isolate_;
    Local<Context> context = isolate->GetCurrentContext();
    const uint32_t id = next_port_id_++;
    Local<Value> argv[] = {Uint32::NewFromUnsigned(isolate, id)};
    Local<Value> object;
    if (!CallRuntime(
             isolate, context, "createMessagePort", arraysize(argv), argv)
             .ToLocal(&object)) {
      return {};
    }
    CHECK(object->IsObject());
    auto port = std::make_unique<GreenPort>(this, id);
    port->object.Reset(isolate, object.As<Object>());
    GreenPort* raw_port = port.get();
    ports_.emplace(id, std::move(port));
    raw_port->Attach(std::move(data));
    return object.As<Object>();
  }

  // Delivers some of the messages of a port, each in an invocation context of
  // its own, and queues delivering the others.
  void DrainPort(uint32_t id) {
    static constexpr size_t kMessageBudget = 16;
    auto it = ports_.find(id);
    if (it == ports_.end()) return;
    it->second->ClearNotified();
    for (size_t n = 0; n < kMessageBudget; n++) {
      it = ports_.find(id);
      if (it == ports_.end() || it->second->data() == nullptr ||
          ShouldShutdown()) {
        return;
      }
      GreenPort* port = it->second.get();
      std::shared_ptr<worker::Message> message =
          port->data()->TakeIncomingMessage(!port->started);
      if (!message) return;
      if (message->IsCloseMessage()) {
        ClosePort(id);
        return;
      }
      RunTask(kNewInvocation, [&](Isolate* isolate, Local<Context> context) {
        DeliverPortMessage(isolate, context, id, message.get());
      });
    }
    DrainPortLater(id);
  }

  // Dispatches a 'message' event on the port, or a 'messageerror' event if
  // the message can't be deserialized.
  void DeliverPortMessage(Isolate* isolate,
                          Local<Context> context,
                          uint32_t id,
                          worker::Message* message) {
    auto it = ports_.find(id);
    if (it == ports_.end()) return;
    Local<Object> port = it->second->object.Get(isolate);
    Local<Value> data;
    Local<v8::Array> ports;
    bool ok;
    {
      TryCatch try_catch(isolate);
      std::optional<SerializedValue> value = FromMessage(message);
      if (value) {
        ok = Deserialize(
                 isolate, context, *value, {}, has_memory_limit(), this, &ports)
                 .ToLocal(&data);
      } else {
        SerializerDelegate(isolate, context, false)
            .ThrowDataCloneError(FIXED_ONE_BYTE_STRING(
                isolate,
                "The message contains objects that green threads can't "
                "receive"));
        ok = false;
      }
      if (!ok) {
        if (!try_catch.HasCaught() || try_catch.HasTerminated()) {
          try_catch.ReThrow();
          return;
        }
        data = try_catch.Exception();
      }
    }
    Local<Value> argv[] = {
        port,
        data,
        ok ? ports.As<Value>() : v8::Undefined(isolate).As<Value>(),
        Boolean::New(isolate, ok),
    };
    USE(CallRuntime(
        isolate, context, "dispatchPortMessage", arraysize(argv), argv));
  }

  // Closes a port, which closes the port it is entangled with, and queues
  // its 'close' event.
  void ClosePort(uint32_t id) {
    auto it = ports_.find(id);
    if (it == ports_.end() || it->second->data() == nullptr) return;
    std::unique_ptr<MessagePortData> data = it->second->Detach();
    data->Disentangle();
    data.reset();
    Submit([id](GreenIsolate* self) {
      auto it = self->ports_.find(id);
      if (it == self->ports_.end()) return;
      std::unique_ptr<GreenPort> port = std::move(it->second);
      self->ports_.erase(it);
      self->RunTask(
          kNewInvocation, [&](Isolate* isolate, Local<Context> context) {
            Local<Value> argv[] = {port->object.Get(isolate)};
            USE(self->CallRuntime(
                isolate, context, "dispatchPortClose", arraysize(argv), argv));
          });
    });
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

  // Kinds of encoded thrown values. Must match internal/green_threads/rpc.
  enum EncodedErrorKind : int32_t { kEncodedValue = 0, kEncodedString = 3 };

  // Encodes a thrown value with the runtime's encodeError(), which keeps
  // the properties of errors. With `minimal`, the result can always be
  // cloned.
  MaybeLocal<Value> EncodeError(Isolate* isolate,
                                Local<Context> context,
                                Local<Value> error,
                                bool minimal) {
    if (!runtime_.IsEmpty()) {
      TryCatch try_catch(isolate);
      Local<Value> argv[] = {error, Boolean::New(isolate, minimal)};
      Local<Value> encoded;
      if (CallRuntime(isolate, context, "encodeError", arraysize(argv), argv)
              .ToLocal(&encoded)) {
        return encoded;
      }
      if (try_catch.HasTerminated()) return {};
    }
    // Before the runtime exists, or if it fails.
    Local<String> string;
    if (minimal) {
      TryCatch try_catch(isolate);
      if (!error->ToDetailString(context).ToLocal(&string)) {
        if (try_catch.HasTerminated()) return {};
        string = FIXED_ONE_BYTE_STRING(isolate, "Error");
      }
    }
    Local<Value> elements[] = {
        Integer::New(isolate, minimal ? kEncodedString : kEncodedValue),
        minimal ? string.As<Value>() : error,
    };
    return v8::Array::New(isolate, elements, arraysize(elements));
  }

  // Sends `value` to the host, or if `ok` is false, the thrown value
  // `value`, encoded with EncodeError(). If a value can't be cloned, sends
  // the clone error instead, with `ok` false.
  void PushValue(Isolate* isolate,
                 Local<Context> context,
                 EventType type,
                 int32_t id,
                 bool ok,
                 Local<Value> value) {
    TryCatch try_catch(isolate);
    std::optional<SerializedValue> bytes;
    if (ok) {
      bytes = Serialize(isolate, context, value, false);
      if (bytes || try_catch.HasTerminated()) {
        if (try_catch.HasTerminated()) return;
        PushEvent({type, id, true, std::move(*bytes)});
        return;
      }
      value = try_catch.Exception();
      try_catch.Reset();
      ok = false;
    }
    for (bool minimal : {false, true}) {
      Local<Value> encoded;
      if (EncodeError(isolate, context, value, minimal).ToLocal(&encoded)) {
        bytes = Serialize(isolate, context, encoded, false);
      }
      if (try_catch.HasTerminated()) return;
      try_catch.Reset();
      if (bytes) break;
    }
    PushEvent({type, id, false, bytes ? std::move(*bytes) : SerializedValue{}});
  }

  void ReportUncaughtException(Isolate* isolate,
                               Local<Context> context,
                               Local<Value> exception) {
    if (fatal_) return;
    fatal_ = true;
    PushValue(isolate, context, kEventError, kErrorException, false, exception);
    // Stop any further JS, including the remaining microtasks.
    TerminateExecution(isolate);
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
    // V8 also calls this when an ArrayBuffer allocation fails, e.g. because
    // of the memory limit, while the heap is nowhere near full. Let the
    // allocation fail with a RangeError then. The committed size is checked,
    // since fragmentation may keep the used size well below the limit when
    // the heap really is full.
    v8::HeapStatistics stats;
    self->isolate_->GetHeapStatistics(&stats);
    if (stats.total_heap_size() < current_heap_limit / 4 * 3) {
      return current_heap_limit;
    }
    self->out_of_memory_ = true;
    // Give the current GC some leeway, then stop running JS.
    self->TerminateExecution(self->isolate_);
    return current_heap_limit + kExtraHeapAllowance;
  }

  static bool DefineFunction(Isolate* isolate,
                             Local<Context> context,
                             Local<Object> target,
                             const char* name,
                             v8::FunctionCallback callback,
                             Local<Value> data = Local<Value>()) {
    Local<String> name_string = OneByteString(isolate, name);
    Local<Function> fn;
    if (!Function::New(context, callback, data, 0, ConstructorBehavior::kThrow)
             .ToLocal(&fn)) {
      return false;
    }
    fn->SetName(name_string);
    return target
        ->DefineOwnProperty(
            context, name_string, fn, PropertyAttribute::DontEnum)
        .IsJust();
  }

  static bool InstallGlobals(Isolate* isolate, Local<Context> context) {
    Local<Object> global = context->Global();
    auto define = [&](Local<Object> target,
                      const char* name,
                      v8::FunctionCallback callback,
                      Local<Value> data = Local<Value>()) {
      return DefineFunction(isolate, context, target, name, callback, data);
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

  // Like InitializePrimordials(), which also runs before there is an
  // Environment. Shares the sources and code cache of the first Environment
  // that loaded the binding, like workers do. Pool threads live until the
  // process exits.
  static builtins::BuiltinLoader* GetBuiltinLoader() {
    if (bootstrap_builtin_loader != nullptr) return bootstrap_builtin_loader;
    // NOLINTNEXTLINE(runtime/thread_local)
    thread_local builtins::BuiltinLoader* loader = []() {
      auto* loader = new builtins::BuiltinLoader();
      Mutex::ScopedLock lock(shared_builtin_loader_mutex);
      CHECK_NOT_NULL(shared_builtin_loader);
      loader->CopySourceAndCodeCacheReferenceFrom(shared_builtin_loader);
      return loader;
    }();
    return loader;
  }

  // Runs the JS part of the green thread runtime: its primordials, Node.js'
  // DOMException, then internal/green_threads/runtime/bootstrap, whose exports
  // are kept in runtime_.
  // Returns the exports of the bootstrap, or nothing if it failed.
  static MaybeLocal<Object> RunBootstrap(Isolate* isolate,
                                         Local<Context> context) {
    TryCatch try_catch(isolate);
    auto set = [&](Local<Object> target, const char* name, Local<Value> value) {
      return target->Set(context, OneByteString(isolate, name), value).IsJust();
    };
    Local<Object> per_context_exports = Object::New(isolate);
    Local<Object> primordials =
        Object::New(isolate, v8::Null(isolate), nullptr, nullptr, 0);

    // What internalBinding() returns in the runtime's modules. Private
    // symbols can only be passed to JS through templates.
    Local<ObjectTemplate> natives_template = ObjectTemplate::New(isolate);
    natives_template->Set(FIXED_ONE_BYTE_STRING(isolate, "rpcBrand"),
                          RpcBrand(isolate));
    natives_template->Set(FIXED_ONE_BYTE_STRING(isolate, "rpcSend"),
                          FunctionTemplate::New(isolate, RpcSendCallback));
    natives_template->Set(FIXED_ONE_BYTE_STRING(isolate, "portId"),
                          PortIdSymbol(isolate));
    natives_template->Set(
        FIXED_ONE_BYTE_STRING(isolate, "messageChannel"),
        FunctionTemplate::New(isolate, MessageChannelCallback));
    natives_template->Set(
        FIXED_ONE_BYTE_STRING(isolate, "portPostMessage"),
        FunctionTemplate::New(isolate, PortPostMessageCallback));
    natives_template->Set(FIXED_ONE_BYTE_STRING(isolate, "portStart"),
                          FunctionTemplate::New(isolate, PortStartCallback));
    natives_template->Set(FIXED_ONE_BYTE_STRING(isolate, "portClose"),
                          FunctionTemplate::New(isolate, PortCloseCallback));
    static constexpr std::pair<const char*, v8::FunctionCallback>
        kModuleNatives[] = {
            {"compileModule", CompileModuleCallback},
            {"createJsonModule", CreateJsonModuleCallback},
            {"moduleRequests", ModuleRequestsCallback},
            {"linkModule", LinkModuleCallback},
            {"instantiateModule", InstantiateModuleCallback},
            {"evaluateModule", EvaluateModuleCallback},
            {"moduleNamespace", ModuleNamespaceCallback},
            {"resolveUrl", ResolveUrlCallback},
        };
    for (const auto& [name, callback] : kModuleNatives) {
      natives_template->Set(OneByteString(isolate, name),
                            FunctionTemplate::New(isolate, callback));
    }
    Local<ObjectTemplate> private_symbols_template =
        ObjectTemplate::New(isolate);
    private_symbols_template->Set(
        FIXED_ONE_BYTE_STRING(isolate, "transfer_mode_private_symbol"),
        v8::Private::ForApi(
            isolate,
            FIXED_ONE_BYTE_STRING(isolate, "node:green_thread:transfer_mode")));
    Local<Object> natives;
    Local<Object> private_symbols;
    Local<Object> per_isolate_symbols = Object::New(isolate);
    Local<Object> bindings = Object::New(isolate);
    Local<Object> modules = Object::New(isolate);
    Local<v8::Array> data = v8::Array::New(isolate, kRequireDataLength);
    Local<Function> internal_binding;
    Local<Function> require;
    if (!natives_template->NewInstance(context).ToLocal(&natives) ||
        !private_symbols_template->NewInstance(context).ToLocal(
            &private_symbols) ||
        !set(per_isolate_symbols,
             "messaging_clone_symbol",
             v8::Symbol::New(isolate)) ||
        !set(per_isolate_symbols,
             "messaging_deserialize_symbol",
             v8::Symbol::New(isolate)) ||
        !set(bindings, "green_thread_runtime", natives) ||
        !set(bindings, "per_context", per_context_exports) ||
        !Function::New(context, InternalBindingCallback, bindings)
             .ToLocal(&internal_binding) ||
        !Function::New(context, RequireCallback, data).ToLocal(&require) ||
        data->Set(context, kRequireDataModules, modules).IsNothing() ||
        data->Set(context, kRequireDataPrimordials, primordials).IsNothing() ||
        data->Set(context, kRequireDataInternalBinding, internal_binding)
            .IsNothing() ||
        data->Set(context, kRequireDataRequire, require).IsNothing() ||
        Require(isolate,
                context,
                data,
                FIXED_ONE_BYTE_STRING(
                    isolate, "internal/green_threads/runtime/primordials"))
            .IsEmpty()) {
      ReportBootstrapError(isolate, context, try_catch);
      return {};
    }

    Local<Value> args[] = {
        per_context_exports, primordials, private_symbols, per_isolate_symbols};
    Local<Value> bootstrap_exports;
    if (GetBuiltinLoader()
            ->CompileAndCallWith(context,
                                 "internal/per_context/domexception",
                                 arraysize(args),
                                 args,
                                 nullptr)
            .IsEmpty() ||
        !Require(isolate,
                 context,
                 data,
                 FIXED_ONE_BYTE_STRING(
                     isolate, "internal/green_threads/runtime/bootstrap"))
             .ToLocal(&bootstrap_exports)) {
      ReportBootstrapError(isolate, context, try_catch);
      return {};
    }
    CHECK(bootstrap_exports->IsObject());
    return bootstrap_exports.As<Object>();
  }

 public:
  // The indexes of the bootstrapped context in the green snapshot, and of
  // the bootstrap's exports in the data of a bootstrapped context, in the
  // green snapshot and in Node.js' own.
  static constexpr size_t kGreenSnapshotContextIndex = 0;
  static constexpr size_t kGreenSnapshotRuntimeIndex = 0;

 private:
  // The callbacks of the functions that InstallGlobals() and RunBootstrap()
  // create, which snapshots of a bootstrapped context refer to.
  static std::vector<v8::FunctionCallback> BootstrapCallbacks() {
    return {
        PostMessageCallback,       CloseCallback,
        QueueMicrotaskCallback,    SetTimeoutCallback,
        SetIntervalCallback,       ClearTimerCallback,
        ConsoleCallback,           RpcSendCallback,
        MessageChannelCallback,    PortPostMessageCallback,
        PortStartCallback,         PortCloseCallback,
        CompileModuleCallback,     CreateJsonModuleCallback,
        ModuleRequestsCallback,    LinkModuleCallback,
        InstantiateModuleCallback, EvaluateModuleCallback,
        ModuleNamespaceCallback,   ResolveUrlCallback,
        InternalBindingCallback,   RequireCallback,
    };
  }

  // The external references of the green snapshot.
  static const intptr_t* SnapshotExternalReferences() {
    static const std::vector<intptr_t> references = []() {
      std::vector<intptr_t> references;
      for (v8::FunctionCallback callback : BootstrapCallbacks()) {
        references.push_back(reinterpret_cast<intptr_t>(callback));
      }
      references.push_back(0);
      return references;
    }();
    return references.data();
  }

 public:
  // For Node.js' snapshot, which has a bootstrapped context too.
  static void RegisterBootstrapCallbacks(ExternalReferenceRegistry* registry) {
    for (v8::FunctionCallback callback : BootstrapCallbacks()) {
      registry->Register(callback);
    }
  }

  // A new context in which the runtime is bootstrapped, using `loader`, and
  // the exports of its bootstrap. For snapshots made outside pool threads.
  static MaybeLocal<Context> NewBootstrappedContext(
      Isolate* isolate,
      builtins::BuiltinLoader* loader,
      Local<Object>* runtime) {
    CurrentScope current_scope(nullptr);
    bootstrap_builtin_loader = loader;
    auto reset = OnScopeLeave([] { bootstrap_builtin_loader = nullptr; });
    Local<Context> context = Context::New(isolate);
    if (context.IsEmpty()) return {};
    Context::Scope context_scope(context);
    if (!InstallGlobals(isolate, context) ||
        !RunBootstrap(isolate, context).ToLocal(runtime)) {
      return {};
    }
    return context;
  }

  // Creates the green snapshot (see GreenSnapshot()) with `loader`, outside
  // a pool thread. Empty if that failed.
  static v8::StartupData CreateGreenSnapshotWith(
      MultiIsolatePlatform* platform, builtins::BuiltinLoader* loader) {
    bootstrap_builtin_loader = loader;
    auto reset = OnScopeLeave([] { bootstrap_builtin_loader = nullptr; });
    return CreateGreenSnapshot(platform);
  }

 private:
  // The startup snapshot of isolates in groups that only green isolates use:
  // V8's own snapshot, with a context in which the runtime is bootstrapped.
  // This saves each green thread running the bootstrap, and compiling it,
  // since V8 rejects Node.js' code cache in isolates that don't use Node.js'
  // snapshot. Built along with Node.js' snapshot, or, in builds without one,
  // created on a pool thread the first time it's needed. nullptr if creating
  // it failed.
  static const v8::StartupData* GreenSnapshot(MultiIsolatePlatform* platform) {
    if (const v8::StartupData* embedded =
            SnapshotBuilder::GetEmbeddedGreenSnapshot()) {
      return embedded;
    }
    static Mutex mutex;
    static bool created = false;
    static v8::StartupData blob{nullptr, 0};
    Mutex::ScopedLock lock(mutex);
    if (!created) {
      created = true;
      blob = CreateGreenSnapshot(platform);
    }
    return blob.data != nullptr ? &blob : nullptr;
  }

  static v8::StartupData CreateGreenSnapshot(MultiIsolatePlatform* platform) {
    // The bootstrap doesn't run in a green isolate.
    CurrentScope current_scope(nullptr);
    std::unique_ptr<ArrayBufferAllocator> allocator =
        ArrayBufferAllocator::Create();
    Isolate::CreateParams params;
    params.array_buffer_allocator = allocator.get();
    params.external_references = SnapshotExternalReferences();
    Isolate* isolate = Isolate::Allocate(v8::IsolateGroup::Create());
    if (isolate == nullptr) return {nullptr, 0};
    SnapshotPlatformDelegate delegate;
    // Before the isolate is initialized, which may use the platform.
    platform->RegisterIsolate(isolate, &delegate);
    params.cpp_heap =
        v8::CppHeap::Create(platform, v8::CppHeapCreateParams{{}}).release();

    v8::StartupData blob{nullptr, 0};
    {
      v8::SnapshotCreator creator(isolate, params);
      Locker locker(isolate);
      if (current_pool_thread != nullptr) {
        isolate->SetStackLimit(current_pool_thread->stack_limit());
      }
      bool bootstrapped = false;
      {
        HandleScope handle_scope(isolate);
        // V8 requires a default context. It is never used.
        Local<Context> default_context = Context::New(isolate);
        Local<Context> context = Context::New(isolate);
        if (!default_context.IsEmpty() && !context.IsEmpty()) {
          creator.SetDefaultContext(default_context);
          Context::Scope context_scope(context);
          Local<Object> runtime;
          if (InstallGlobals(isolate, context) &&
              RunBootstrap(isolate, context).ToLocal(&runtime)) {
            CHECK_EQ(creator.AddData(context, runtime),
                     kGreenSnapshotRuntimeIndex);
            CHECK_EQ(creator.AddContext(context), kGreenSnapshotContextIndex);
            bootstrapped = true;
          }
        }
      }
      if (bootstrapped) {
        blob = creator.CreateBlob(
            v8::SnapshotCreator::FunctionCodeHandling::kKeep);
      }
    }
    isolate->Deinitialize();
    delegate.Clear();
    platform->UnregisterIsolate(isolate);
    Isolate::Free(isolate);

    // Otherwise V8 would keep the hash seed of the snapshot, the same for
    // all green threads, which would make hash flooding possible.
    if (blob.data != nullptr && !blob.CanBeRehashed()) {
      delete[] blob.data;
      blob = {nullptr, 0};
    }
    return blob;
  }

  // A bug in the runtime, which runs before any user code.
  static bool ReportBootstrapError(Isolate* isolate,
                                   Local<Context> context,
                                   const TryCatch& try_catch) {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      Local<Value> stack;
      if (!try_catch.StackTrace(context).ToLocal(&stack)) {
        stack = try_catch.Exception();
      }
      Utf8Value message(isolate, stack);
      FPrintF(stderr, "Green thread bootstrap failed: %s\n", *message);
    }
    return false;
  }

  enum RequireData {
    kRequireDataModules,
    kRequireDataPrimordials,
    kRequireDataInternalBinding,
    kRequireDataRequire,
    kRequireDataLength,
  };

  // require(id) in the runtime's modules. Only loads the runtime's own
  // modules, once each.
  static MaybeLocal<Value> Require(Isolate* isolate,
                                   Local<Context> context,
                                   Local<v8::Array> data,
                                   Local<String> id) {
    Local<Value> modules_value;
    if (!data->Get(context, kRequireDataModules).ToLocal(&modules_value)) {
      return {};
    }
    Local<Object> modules = modules_value.As<Object>();
    Local<Value> cached;
    if (!modules->Get(context, id).ToLocal(&cached)) return {};
    if (!cached->IsUndefined()) return cached;

    Utf8Value id_string(isolate, id);
    if (!id_string.ToStringView().starts_with("internal/green_threads/") ||
        !GetBuiltinLoader()->Exists(*id_string)) {
      isolate->ThrowException(Exception::Error(String::Concat(
          isolate, FIXED_ONE_BYTE_STRING(isolate, "No such module: "), id)));
      return {};
    }
    Local<Object> exports = Object::New(isolate);
    Local<Object> module = Object::New(isolate);
    Local<Value> primordials, internal_binding, require;
    Local<String> exports_string = FIXED_ONE_BYTE_STRING(isolate, "exports");
    if (module->Set(context, exports_string, exports).IsNothing() ||
        !data->Get(context, kRequireDataPrimordials).ToLocal(&primordials) ||
        !data->Get(context, kRequireDataInternalBinding)
             .ToLocal(&internal_binding) ||
        !data->Get(context, kRequireDataRequire).ToLocal(&require)) {
      return {};
    }
    Local<Value> args[] = {exports,
                           require,
                           module,
                           v8::Undefined(isolate),
                           internal_binding,
                           primordials};
    Local<Value> result;
    if (GetBuiltinLoader()
            ->CompileAndCallWith(
                context, *id_string, arraysize(args), args, nullptr)
            .IsEmpty() ||
        !module->Get(context, exports_string).ToLocal(&result) ||
        modules->Set(context, id, result).IsNothing()) {
      return {};
    }
    return result;
  }

  static void RequireCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!args[0]->IsString()) {
      isolate->ThrowException(Exception::TypeError(
          FIXED_ONE_BYTE_STRING(isolate, "The module id must be a string")));
      return;
    }
    Local<Value> result;
    if (Require(isolate,
                isolate->GetCurrentContext(),
                args.Data().As<v8::Array>(),
                args[0].As<String>())
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }
  }

  static void InternalBindingCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    Local<Value> binding;
    if (!args[0]->IsString() ||
        !args.Data().As<Object>()->Get(context, args[0]).ToLocal(&binding)) {
      return;
    }
    if (binding->IsUndefined()) {
      isolate->ThrowException(
          Exception::Error(FIXED_ONE_BYTE_STRING(isolate, "No such binding")));
      return;
    }
    args.GetReturnValue().Set(binding);
  }

  static GreenIsolate* Current() {
    CHECK_NOT_NULL(current_green_isolate);
    return current_green_isolate;
  }

  // postMessage(value[, transfer])
  static void PostMessageCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    Local<Value> transfer;
    if (!GetTransferList(context, args[1]).ToLocal(&transfer)) return;
    std::optional<SerializedValue> bytes = Serialize(
        isolate, context, args[0], false, {}, self->Transfer(transfer));
    if (!bytes) return;
    self->PushEvent({kEventMessage, 0, true, std::move(*bytes)});
  }

  TransferOptions Transfer(Local<Value> list, Local<Object> source_port = {}) {
    TransferOptions options;
    options.list = list;
    options.ports = this;
    options.source_port = source_port;
    if (has_memory_limit()) options.limited_allocator = allocator_;
    return options;
  }

  uint32_t AddModule(Isolate* isolate,
                     Local<v8::Module> module,
                     std::string url,
                     Local<Value> json = {}) {
    const uint32_t id = next_module_id_++;
    ModuleRecord& record = modules_[id];
    record.module.Reset(isolate, module);
    record.url = std::move(url);
    if (!json.IsEmpty()) record.json.Reset(isolate, json);
    modules_by_hash_.emplace(module->GetIdentityHash(), id);
    return id;
  }

  ModuleRecord* FindModule(Isolate* isolate, Local<v8::Module> module) {
    auto [begin, end] = modules_by_hash_.equal_range(module->GetIdentityHash());
    for (auto it = begin; it != end; ++it) {
      ModuleRecord& record = modules_.at(it->second);
      if (record.module.Get(isolate) == module) return &record;
    }
    return nullptr;
  }

  // The module with the ID in args[0]. Throws if there is none.
  static ModuleRecord* ModuleArg(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    GreenIsolate* self = Current();
    auto it = args[0]->IsUint32()
                  ? self->modules_.find(args[0].As<Uint32>()->Value())
                  : self->modules_.end();
    if (it == self->modules_.end()) {
      isolate->ThrowException(Exception::TypeError(
          FIXED_ONE_BYTE_STRING(isolate, "Invalid module")));
      return nullptr;
    }
    return &it->second;
  }

  // compileModule(source, url): returns the ID of the module.
  static void CompileModuleCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    CHECK(args[0]->IsString());
    CHECK(args[1]->IsString());
    ScriptOrigin origin(
        args[1], 0, 0, false, -1, Local<Value>(), false, false, true);
    ScriptCompiler::Source source(args[0].As<String>(), origin);
    Local<v8::Module> module;
    if (!ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module)) {
      return;
    }
    Utf8Value url(isolate, args[1]);
    args.GetReturnValue().Set(Uint32::NewFromUnsigned(
        isolate, Current()->AddModule(isolate, module, url.ToString())));
  }

  static MaybeLocal<Value> EvaluateJsonModule(Local<Context> context,
                                              Local<v8::Module> module) {
    Isolate* isolate = Isolate::GetCurrent();
    ModuleRecord* record = Current()->FindModule(isolate, module);
    CHECK_NOT_NULL(record);
    if (module
            ->SetSyntheticModuleExport(
                isolate,
                FIXED_ONE_BYTE_STRING(isolate, "default"),
                record->json.Get(isolate))
            .IsNothing()) {
      return {};
    }
    Local<v8::Promise::Resolver> resolver;
    if (!v8::Promise::Resolver::New(context).ToLocal(&resolver) ||
        resolver->Resolve(context, v8::Undefined(isolate)).IsNothing()) {
      return {};
    }
    return resolver->GetPromise();
  }

  // createJsonModule(url, value): returns the ID of a module whose default
  // export is `value`.
  static void CreateJsonModuleCallback(
      const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    CHECK(args[0]->IsString());
    Local<String> export_names[] = {FIXED_ONE_BYTE_STRING(isolate, "default")};
    Local<v8::Module> module = v8::Module::CreateSyntheticModule(
        isolate,
        args[0].As<String>(),
        export_names,
        [](Local<Context> context,
           Local<v8::Module> module) -> MaybeLocal<v8::Promise> {
          Local<Value> promise;
          if (!EvaluateJsonModule(context, module).ToLocal(&promise)) return {};
          return promise.As<v8::Promise>();
        });
    Utf8Value url(isolate, args[0]);
    args.GetReturnValue().Set(Uint32::NewFromUnsigned(
        isolate,
        Current()->AddModule(isolate, module, url.ToString(), args[1])));
  }

  // moduleRequests(id): returns [specifier, attributes] for each of the
  // module's requests, where `attributes` is an object.
  static void ModuleRequestsCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    ModuleRecord* record = ModuleArg(args);
    if (record == nullptr) return;
    Local<v8::Module> module = record->module.Get(isolate);
    LocalVector<Value> requests(isolate);
    if (!module->IsSyntheticModule()) {
      Local<v8::FixedArray> module_requests = module->GetModuleRequests();
      for (int i = 0; i < module_requests->Length(); i++) {
        Local<v8::ModuleRequest> request =
            module_requests->Get(i).As<v8::ModuleRequest>();
        Local<Object> attributes;
        if (!ImportAttributesToObject(
                 context, request->GetImportAttributes(), 3)
                 .ToLocal(&attributes)) {
          return;
        }
        Local<Value> pair[] = {request->GetSpecifier(), attributes};
        requests.push_back(v8::Array::New(isolate, pair, arraysize(pair)));
      }
    }
    args.GetReturnValue().Set(
        v8::Array::New(isolate, requests.data(), requests.size()));
  }

  // An object with the attributes in `attributes`, whose entries have
  // `stride` elements, starting with the key and the value.
  static MaybeLocal<Object> ImportAttributesToObject(
      Local<Context> context, Local<v8::FixedArray> attributes, int stride) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Object> object =
        Object::New(isolate, v8::Null(isolate), nullptr, nullptr, 0);
    for (int i = 0; i + 1 < attributes->Length(); i += stride) {
      Local<Value> key = attributes->Get(i).As<Value>();
      Local<Value> value = attributes->Get(i + 1).As<Value>();
      if (object->Set(context, key, value).IsNothing()) return {};
    }
    return object;
  }

  // linkModule(id, dependencyIds): sets the modules that the module's
  // requests resolve to.
  static void LinkModuleCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    ModuleRecord* record = ModuleArg(args);
    if (record == nullptr) return;
    CHECK(args[1]->IsArray());
    Local<v8::Array> ids = args[1].As<v8::Array>();
    std::vector<uint32_t> dependencies;
    for (uint32_t i = 0; i < ids->Length(); i++) {
      Local<Value> id;
      if (!ids->Get(context, i).ToLocal(&id)) return;
      CHECK(id->IsUint32());
      CHECK(Current()->modules_.contains(id.As<Uint32>()->Value()));
      dependencies.push_back(id.As<Uint32>()->Value());
    }
    record->dependencies = std::move(dependencies);
    record->linked = true;
  }

  static MaybeLocal<v8::Module> ResolveModule(Local<Context> context,
                                              size_t index,
                                              Local<v8::Module> referrer) {
    Isolate* isolate = Isolate::GetCurrent();
    GreenIsolate* self = Current();
    ModuleRecord* record = self->FindModule(isolate, referrer);
    if (record == nullptr || !record->linked ||
        index >= record->dependencies.size()) {
      isolate->ThrowException(Exception::Error(
          FIXED_ONE_BYTE_STRING(isolate, "The module is not linked")));
      return {};
    }
    return self->modules_.at(record->dependencies[index]).module.Get(isolate);
  }

  // instantiateModule(id): links the module's graph, which the loader has
  // linked.
  static void InstantiateModuleCallback(
      const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    ModuleRecord* record = ModuleArg(args);
    if (record == nullptr) return;
    USE(record->module.Get(isolate)->InstantiateModule(
        isolate->GetCurrentContext(), ResolveModule));
  }

  // evaluateModule(id): returns a promise that settles once the module is
  // evaluated.
  static void EvaluateModuleCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    ModuleRecord* record = ModuleArg(args);
    if (record == nullptr) return;
    Local<v8::Promise> promise;
    if (record->module.Get(isolate)
            ->Evaluate(isolate->GetCurrentContext())
            .ToLocal(&promise)) {
      args.GetReturnValue().Set(promise);
    }
  }

  static void ModuleNamespaceCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    ModuleRecord* record = ModuleArg(args);
    if (record == nullptr) return;
    args.GetReturnValue().Set(
        record->module.Get(isolate)->GetModuleNamespace());
  }

  // resolveUrl(input[, base]): returns the URL's href, or undefined if it
  // is not a valid URL.
  static void ResolveUrlCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    CHECK(args[0]->IsString());
    Utf8Value input(isolate, args[0]);
    std::optional<ada::url_aggregator> url;
    if (args[1]->IsString()) {
      Utf8Value base_input(isolate, args[1]);
      auto base = ada::parse<ada::url_aggregator>(base_input.ToStringView());
      if (!base) return;
      auto result =
          ada::parse<ada::url_aggregator>(input.ToStringView(), &*base);
      if (result) url = std::move(*result);
    } else {
      auto result = ada::parse<ada::url_aggregator>(input.ToStringView());
      if (result) url = std::move(*result);
    }
    if (!url) return;
    Local<Value> href;
    if (ToV8Value(isolate->GetCurrentContext(), url->get_href(), isolate)
            .ToLocal(&href)) {
      args.GetReturnValue().Set(href);
    }
  }

  // import(): the runtime's module loader imports the module.
  static MaybeLocal<v8::Promise> ImportModuleDynamically(
      Local<Context> context,
      Local<v8::Data> host_defined_options,
      Local<Value> resource_name,
      Local<String> specifier,
      Local<v8::FixedArray> import_attributes) {
    Isolate* isolate = Isolate::GetCurrent();
    GreenIsolate* self = Current();
    Local<Object> attributes;
    if (!ImportAttributesToObject(context, import_attributes, 2)
             .ToLocal(&attributes)) {
      return {};
    }
    Local<Value> argv[] = {
        specifier,
        resource_name->IsString() ? resource_name : v8::Undefined(isolate),
        attributes,
    };
    Local<Value> promise;
    if (!self->CallRuntime(
                 isolate, context, "importModule", arraysize(argv), argv)
             .ToLocal(&promise)) {
      return {};
    }
    CHECK(promise->IsPromise());
    return promise.As<v8::Promise>();
  }

  static void InitializeImportMeta(Local<Context> context,
                                   Local<v8::Module> module,
                                   Local<Object> meta) {
    Isolate* isolate = Isolate::GetCurrent();
    ModuleRecord* record = Current()->FindModule(isolate, module);
    if (record == nullptr) return;
    Local<Value> url;
    if (!ToV8Value(context, record->url, isolate).ToLocal(&url)) return;
    USE(meta->Set(context, FIXED_ONE_BYTE_STRING(isolate, "url"), url));
  }

  // messageChannel(): returns two entangled ports.
  static void MessageChannelCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    auto data1 = std::make_unique<MessagePortData>(nullptr);
    auto data2 = std::make_unique<MessagePortData>(nullptr);
    MessagePortData::Entangle(data1.get(), data2.get());
    Local<Object> port1;
    Local<Object> port2;
    if (!self->CreatePort(std::move(data1)).ToLocal(&port1) ||
        !self->CreatePort(std::move(data2)).ToLocal(&port2)) {
      return;
    }
    Local<Value> ports[] = {port1, port2};
    args.GetReturnValue().Set(v8::Array::New(isolate, ports, arraysize(ports)));
  }

  // portPostMessage(port, value, transfer). Like Node.js' ports, ports that
  // are closed serialize the value, and then drop it.
  static void PortPostMessageCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    CHECK(args[0]->IsObject());
    Local<Value> transfer;
    if (!GetTransferList(context, args[2]).ToLocal(&transfer)) return;
    std::optional<SerializedValue> value =
        Serialize(isolate,
                  context,
                  args[1],
                  false,
                  {},
                  self->Transfer(transfer, args[0].As<Object>()));
    if (!value) return;
    GreenPort* port = self->FindPort(args[0]);
    if (port == nullptr || port->data() == nullptr) return;
    USE(port->data()->Dispatch(ToMessage(std::move(*value))));
  }

  static void PortStartCallback(const FunctionCallbackInfo<Value>& args) {
    GreenPort* port = Current()->FindPort(args[0]);
    if (port == nullptr || port->started) return;
    port->started = true;
    if (port->data() != nullptr) port->Notify();
  }

  static void PortCloseCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    if (!args[0]->IsObject()) return;
    self->ClosePort(self->PortId(args[0].As<Object>()));
  }

  // rpcSend(message, isCall): sends an RPC message to the host. The
  // invocation context of a call is kept until its result arrives, and the
  // result continues in it.
  static void RpcSendCallback(const FunctionCallbackInfo<Value>& args) {
    GreenIsolate* self = Current();
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    std::optional<SerializedValue> bytes = Serialize(
        isolate, context, args[0], false, self->runtime_.Get(isolate));
    if (!bytes) return;
    uint32_t context_id = 0;
    if (args[1]->IsTrue()) {
      context_id = self->CurrentInvocationContextId(isolate);
      self->contexts_.at(context_id).pending++;
    }
    self->PushEvent(
        {kEventRpc, static_cast<int32_t>(context_id), true, std::move(*bytes)});
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
  // Blocks in C++ forever, where a thread can't be cut off.
  static void BlockCallback(const FunctionCallbackInfo<Value>& args) {
    for (;;) uv_sleep(1000);
  }

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
        FIXED_ONE_BYTE_STRING(isolate, "isolateGroup"),
        FIXED_ONE_BYTE_STRING(isolate, "usesNodeSnapshot"),
        FIXED_ONE_BYTE_STRING(isolate, "heapUsed"),
    };
    v8::HeapStatistics heap;
    isolate->GetHeapStatistics(&heap);
    Local<Value> values[] = {
        Number::New(isolate, current_pool_thread->index()),
        Number::New(isolate, GreenThreadPool::Get()->size()),
        Number::New(isolate, context_id),
        Number::New(isolate, self->contexts_.at(context_id).thread->index()),
        Number::New(isolate, self->contexts_.size()),
        Number::New(isolate, self->timers_.size()),
        Number::New(isolate, static_cast<double>(self->group_id_)),
        Boolean::New(isolate, self->uses_node_snapshot_),
        Number::New(isolate, static_cast<double>(heap.used_heap_size())),
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

    std::optional<SerializedValue> bytes;
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
    self->PushEvent({kEventConsole, level, true, std::move(*bytes)});
  }

  MultiIsolatePlatform* const platform_;
  std::shared_ptr<GreenTaskRunner> task_runner_;
  std::function<void(bool)> on_initialized_;

  Mutex host_mutex_;
  std::shared_ptr<HostChannel> host_;
  std::atomic<bool> exit_pushed_{false};

  // Written in slices; read by Terminate() on any thread.
  Mutex isolate_mutex_;
  Isolate* isolate_ = nullptr;

  std::atomic<bool> terminate_requested_{false};
  std::atomic<PoolThread*> last_thread_{nullptr};

  Mutex disposal_mutex_;
  ConditionVariable disposal_cv_;
  bool disposed_ = false;

  // Backup watchdog. Odd while a work item runs.
  std::atomic<uint64_t> work_epoch_{0};
  std::atomic<PoolThread*> running_thread_{nullptr};
  // The work item that the backup watchdog was last armed for.
  std::atomic<uint64_t> backup_epoch_{0};
  std::atomic<bool> abandoned_{false};
  std::atomic<bool> cut_off_{false};
  std::atomic<bool> ignore_termination_{false};
  uint64_t cut_off_deadline_ = 0;
  std::pair<uintptr_t, size_t> code_range_{0, 0};
  std::pair<uintptr_t, size_t> embedded_code_range_{0, 0};

  // Guarded by scheduler_mutex_.
  Mutex scheduler_mutex_;
  std::deque<QueuedWork> queue_;
  bool locked_ = false;
  bool scheduled_ = false;
  // Until Prepare()'s work has run. See set_low_priority().
  bool low_priority_ = false;
  bool stopped_ = false;
  uint64_t next_delayed_v8_task_id_ = 0;
  DelayedV8Tasks delayed_v8_tasks_;

  // Only accessed in slices.
  State state_ = State::kPending;
  Global<Context> context_;
  // The exports of internal/green_threads/runtime/bootstrap.
  Global<Object> runtime_;
  std::shared_ptr<LimitedAllocator> allocator_;
  std::unordered_map<uint32_t, std::unique_ptr<GreenPort>> ports_;
  uint32_t next_port_id_ = 1;

  // See ModuleRecord.
  std::unordered_map<uint32_t, ModuleRecord> modules_;
  std::unordered_multimap<int, uint32_t> modules_by_hash_;
  uint32_t next_module_id_ = 1;
  uint64_t group_id_ = 0;
  bool uses_node_snapshot_ = true;
  uint64_t max_cpu_time_ = 0;
  uint64_t max_task_cpu_time_ = 0;
  uint64_t cpu_time_used_ = 0;
  uint64_t task_cpu_start_ = 0;
  uint64_t task_cpu_budget_ = 0;
  bool in_cpu_limited_task_ = false;
  // Set on the watchdog thread too.
  std::atomic<bool> cpu_limit_exceeded_{false};
  std::atomic<uint64_t> cpu_checks_requested_{0};
  std::atomic<uint64_t> cpu_checks_serviced_{0};
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

void GreenPort::Notify() {
  if (!notified_.exchange(true)) green_->DrainPortLater(id_);
}

void GreenTaskRunner::Post(std::unique_ptr<Task> task,
                           double delay_in_seconds) {
  if (std::shared_ptr<GreenIsolate> owner = owner_.lock()) {
    owner->PostV8Task(std::move(task), delay_in_seconds);
  }
}

#ifdef NODE_GREEN_THREAD_CAN_CUT_OFF
uintptr_t ProgramCounter(void* context) {
  const mcontext_t& mcontext = static_cast<ucontext_t*>(context)->uc_mcontext;
#if defined(__x86_64__)
  return static_cast<uintptr_t>(mcontext.gregs[REG_RIP]);
#else
  return static_cast<uintptr_t>(mcontext.pc);
#endif
}

// Cuts off the work that the receiving pool thread runs, if it belongs to
// the green isolate that is to be cut off, and the thread is running code
// that V8 generated, which holds no locks. Otherwise, the backup watchdog
// tries again later.
void OnCutOffSignal(int signal, siginfo_t* info, void* context) {
  PoolThread* thread = current_pool_thread;
  if (thread == nullptr) return;
  GreenIsolate* target = thread->cut_off_target();
  if (target == nullptr || thread->running() != target) return;
  if (!target->IsGeneratedCode(ProgramCounter(context))) return;
  siglongjmp(*thread->cut_off_jump(), 1);
}

// Whether the signal handler could be installed.
bool InstallCutOffSignalHandler() {
  static const bool installed = []() {
    struct sigaction old_action;
    if (sigaction(CutOffSignal(), nullptr, &old_action) != 0 ||
        old_action.sa_handler != SIG_DFL) {
      return false;
    }
    struct sigaction action {};
    action.sa_sigaction = OnCutOffSignal;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    return sigaction(CutOffSignal(), &action, nullptr) == 0;
  }();
  return installed;
}
#endif

bool PoolThread::SendCutOffSignal(GreenIsolate* target) {
#ifdef NODE_GREEN_THREAD_CAN_CUT_OFF
  if (!InstallCutOffSignalHandler()) return false;
  set_cut_off_target(target);
  return pthread_kill(tid_, CutOffSignal()) == 0;
#else
  return false;
#endif
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

// Isolates that were created ahead of time, with the default isolate options,
// so that starting a green thread doesn't have to wait for one. Used isolates
// are never returned; the pool is refilled instead. Created on first use and
// intentionally never destroyed.
class WarmPool {
 public:
  static WarmPool* Get() {
    static WarmPool* pool = new WarmPool();
    return pool;
  }

  // Stops refilling for good. Once this returns, all isolates that the pool
  // created are known to TerminateAll(). May be called from any thread but a
  // pool thread.
  void Shutdown() {
    Mutex::ScopedLock lock(mutex_);
    shut_down_ = true;
    size_ = 0;
    waiters_.clear();
  }

  // Sets the size of the pool. Once it holds that many initialized isolates,
  // an event is pushed to `waiter`, unless it is null. May be called from any
  // thread but a pool thread.
  void SetSize(size_t size,
               MultiIsolatePlatform* platform,
               std::shared_ptr<HostChannel> waiter) {
    std::deque<Entry> removed;
    {
      Mutex::ScopedLock lock(mutex_);
      if (shut_down_) return;
      if (platform_ != platform) {
        removed.swap(idle_);
        platform_ = platform;
      }
      size_ = size;
      while (idle_.size() > size_) {
        removed.push_back(std::move(idle_.back()));
        idle_.pop_back();
      }
      if (waiter) waiters_.push_back(std::move(waiter));
      RefillLocked();
      MaybeNotifyLocked();
    }
    for (Entry& entry : removed) entry.green->Terminate();
  }

  // Returns nullptr if no isolate is available. May be called from any
  // thread but a pool thread.
  std::shared_ptr<GreenIsolate> Take(MultiIsolatePlatform* platform) {
    Mutex::ScopedLock lock(mutex_);
    if (platform_ != platform) return nullptr;
    // An isolate that is still being initialized would make the green thread
    // wait for background work.
    auto it = std::find_if(
        idle_.begin(), idle_.end(), [](const Entry& e) { return e.ready; });
    if (it == idle_.end()) return nullptr;
    std::shared_ptr<GreenIsolate> green = std::move(it->green);
    idle_.erase(it);
    RefillLocked();
    return green;
  }

 private:
  // Refilling happens in the background, so that it doesn't delay green
  // threads that are starting, which may be what emptied the pool.
  void RefillLocked() {
    for (; idle_.size() + refilling_ < size_; refilling_++) {
      GreenThreadPool::Get()->PostBackground([this]() { RefillOne(); });
    }
  }

  void RefillOne() {
    Mutex::ScopedLock lock(mutex_);
    refilling_--;
    if (shut_down_ || idle_.size() + refilling_ >= size_) return;
    auto green = std::make_shared<GreenIsolate>(platform_);
    green->set_on_initialized(
        [this, key = green.get()](bool ok) { OnInitialized(key, ok); });
    green->set_low_priority();
    green->Prepare({});
    idle_.push_back({std::move(green), false});
  }

  // Runs on a pool thread.
  void OnInitialized(const GreenIsolate* key, bool ok) {
    Mutex::ScopedLock lock(mutex_);
    auto it = std::find_if(idle_.begin(), idle_.end(), [&](const Entry& e) {
      return e.green.get() == key;
    });
    // It may have been taken already.
    if (it == idle_.end()) return;
    if (ok) {
      it->ready = true;
    } else {
      idle_.erase(it);
      RefillLocked();
    }
    MaybeNotifyLocked();
  }

  void MaybeNotifyLocked() {
    if (waiters_.empty()) return;
    const size_t ready = std::count_if(
        idle_.begin(), idle_.end(), [](const Entry& e) { return e.ready; });
    if (ready < size_) return;
    for (auto& waiter : waiters_)
      waiter->Push({kEventWarmPoolFull, 0, true, {}});
    waiters_.clear();
  }

  struct Entry {
    std::shared_ptr<GreenIsolate> green;
    // Whether its isolate has been initialized.
    bool ready;
  };

  Mutex mutex_;
  size_t size_ = 0;
  size_t refilling_ = 0;
  bool shut_down_ = false;
  MultiIsolatePlatform* platform_ = nullptr;
  std::deque<Entry> idle_;
  std::vector<std::shared_ptr<HostChannel>> waiters_;
};

}  // namespace

bool TerminateAll() {
  WarmPool::Get()->Shutdown();
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
  // Without the last resort, a pool thread may never be cut off. Since V8 is
  // not disposed if green threads were abandoned, the process can then exit
  // while it still runs.
  const bool wait_for_threads = cut_off_period.load() != UINT64_MAX;
  for (auto& green : green_isolates) green->TerminateAndWait(wait_for_threads);
  Mutex::ScopedLock lock(live_green_isolates_mutex);
  return abandoned_green_isolates->empty();
}

v8::StartupData CreateSnapshotBlob(MultiIsolatePlatform* platform) {
  if (!v8::IsolateGroup::CanCreateNewGroups()) return {nullptr, 0};
  builtins::BuiltinLoader loader;
  return GreenIsolate::CreateGreenSnapshotWith(platform, &loader);
}

bool AddContextToSnapshot(Isolate* isolate,
                          v8::SnapshotCreator* creator,
                          Local<Context> context,
                          Local<Object> runtime) {
  if (creator->AddData(context, runtime) !=
      GreenIsolate::kGreenSnapshotRuntimeIndex) {
    return false;
  }
  return creator->AddContext(context) == SnapshotData::kNodeGreenContextIndex;
}

MaybeLocal<Context> NewBootstrappedContext(Isolate* isolate,
                                           builtins::BuiltinLoader* loader,
                                           Local<Object>* runtime) {
  return GreenIsolate::NewBootstrappedContext(isolate, loader, runtime);
}

namespace {

// A V8 isolate group that green threads can share.
class IsolateGroupWrap final : public BaseObject {
 public:
  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env) {
    Isolate* isolate = env->isolate();
    Local<FunctionTemplate> constructor = NewFunctionTemplate(isolate, New);
    constructor->InstanceTemplate()->SetInternalFieldCount(
        IsolateGroupWrap::kInternalFieldCount);
    return constructor;
  }

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
    registry->Register(New);
  }

  const v8::IsolateGroup& group() const { return group_; }
  uint64_t id() const { return id_; }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(IsolateGroupWrap)
  SET_SELF_SIZE(IsolateGroupWrap)

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    CHECK(args.IsConstructCall());
    CHECK(v8::IsolateGroup::CanCreateNewGroups());
    new IsolateGroupWrap(Environment::GetCurrent(args), args.This());
  }

  IsolateGroupWrap(Environment* env, Local<Object> object)
      : BaseObject(env, object),
        group_(v8::IsolateGroup::Create()),
        id_(next_isolate_group_id++) {
    MakeWeak();
  }

  // Green isolates hold their own references to the group.
  v8::IsolateGroup group_;
  const uint64_t id_;
};

// Calls its `onready` method once the warm pool is full, then can be closed.
class WarmPoolWaiter final : public HandleWrap {
 public:
  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env) {
    Isolate* isolate = env->isolate();
    Local<FunctionTemplate> constructor = NewFunctionTemplate(isolate, New);
    constructor->InstanceTemplate()->SetInternalFieldCount(
        WarmPoolWaiter::kInternalFieldCount);
    constructor->Inherit(HandleWrap::GetConstructorTemplate(env));
    return constructor;
  }

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
    registry->Register(New);
  }

  const std::shared_ptr<HostChannel>& channel() const { return channel_; }

  void Close(Local<Value> close_callback) override {
    channel_->Close();
    HandleWrap::Close(close_callback);
  }

  ~WarmPoolWaiter() override { channel_->Close(); }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(WarmPoolWaiter)
  SET_SELF_SIZE(WarmPoolWaiter)

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    CHECK(args.IsConstructCall());
    new WarmPoolWaiter(Environment::GetCurrent(args), args.This());
  }

  WarmPoolWaiter(Environment* env, Local<Object> object)
      : HandleWrap(env,
                   object,
                   reinterpret_cast<uv_handle_t*>(&async_),
                   AsyncWrap::PROVIDER_GREENTHREAD) {
    CHECK_EQ(uv_async_init(env->event_loop(), &async_, OnAsyncCallback), 0);
    channel_ = std::make_shared<HostChannel>(&async_);
  }

  static void OnAsyncCallback(uv_async_t* handle) {
    WarmPoolWaiter* waiter = ContainerOf(&WarmPoolWaiter::async_, handle);
    waiter->OnAsync();
  }

  void OnAsync() {
    if (channel_->Take().empty() || IsHandleClosing()) return;
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    MakeCallback(FIXED_ONE_BYTE_STRING(env()->isolate(), "onready").As<Name>(),
                 0,
                 nullptr);
  }

  uv_async_t async_;
  std::shared_ptr<HostChannel> channel_;
};

// The host side of a green thread.
// Node.js' MessagePorts, for messages between a host and its green threads.
class HostPortAdapter final : public PortAdapter {
 public:
  explicit HostPortAdapter(Environment* env) : env_(env) {}

  bool IsPort(Local<Object> object, bool* detached) override {
    if (!worker::GetMessagePortConstructorTemplate(env_->isolate_data())
             ->HasInstance(object)) {
      return false;
    }
    worker::MessagePort* port = BaseObject::Unwrap<worker::MessagePort>(object);
    *detached = port == nullptr || port->GetTransferMode() !=
                                       BaseObject::TransferMode::kTransferable;
    return true;
  }

  std::unique_ptr<MessagePortData> TakePort(Local<Object> object) override {
    worker::MessagePort* port = BaseObject::Unwrap<worker::MessagePort>(object);
    return static_unique_pointer_cast<MessagePortData>(
        port->TransferForMessaging());
  }

  MaybeLocal<Object> CreatePort(
      std::unique_ptr<MessagePortData> data) override {
    worker::MessagePort* port =
        worker::MessagePort::New(env_, env_->context(), std::move(data));
    if (port == nullptr) return {};
    return port->object();
  }

 private:
  Environment* env_;
};

class GreenThreadWrap final : public HandleWrap {
 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context,
                         void* priv) {
    Environment* env = Environment::GetCurrent(context);
    Isolate* isolate = env->isolate();
    {
      Mutex::ScopedLock lock(shared_builtin_loader_mutex);
      if (shared_builtin_loader == nullptr) {
        shared_builtin_loader = new builtins::BuiltinLoader();
        shared_builtin_loader->CopySourceAndCodeCacheReferenceFrom(
            env->builtin_loader());
      }
    }
    Local<FunctionTemplate> constructor = NewFunctionTemplate(isolate, New);
    constructor->InstanceTemplate()->SetInternalFieldCount(
        GreenThreadWrap::kInternalFieldCount);
    constructor->Inherit(HandleWrap::GetConstructorTemplate(env));

    SetProtoMethod(isolate, constructor, "postMessage", PostMessage);
    SetProtoMethod(isolate, constructor, "rpc", Rpc);
    SetProtoMethod(isolate, constructor, "evaluate", Evaluate);
    SetProtoMethod(isolate, constructor, "terminate", Terminate);

    SetConstructorFunction(context, target, "GreenThread", constructor);
    SetMethod(context, target, "setWarmPoolSize", SetWarmPoolSize);
    SetMethod(
        context, target, "setUnresponsiveTimeouts", SetUnresponsiveTimeouts);
    SetConstructorFunction(context,
                           target,
                           "WarmPoolWaiter",
                           WarmPoolWaiter::GetConstructorTemplate(env));
    SetConstructorFunction(context,
                           target,
                           "IsolateGroup",
                           IsolateGroupWrap::GetConstructorTemplate(env));
    target
        ->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "isolateGroupsSupported"),
              Boolean::New(isolate, v8::IsolateGroup::CanCreateNewGroups()))
        .Check();
#ifdef NODE_GREEN_THREAD_CAN_CUT_OFF
    constexpr bool can_cut_off_threads = true;
#else
    constexpr bool can_cut_off_threads = false;
#endif
    target
        ->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "canCutOffThreads"),
              Boolean::New(isolate, can_cut_off_threads))
        .Check();
    {
      // Private symbols can only be passed to JS through templates.
      Local<ObjectTemplate> brand_template = ObjectTemplate::New(isolate);
      Local<String> name = FIXED_ONE_BYTE_STRING(isolate, "rpcBrand");
      brand_template->Set(name, RpcBrand(isolate));
      Local<Value> brand = brand_template->NewInstance(context)
                               .ToLocalChecked()
                               ->Get(context, name)
                               .ToLocalChecked();
      target->Set(context, name, brand).Check();
    }

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
    V("kEventAbandoned", kEventAbandoned)
    V("kEventRpc", kEventRpc)
    V("kErrorException", kErrorException)
    V("kErrorOutOfMemory", kErrorOutOfMemory)
    V("kErrorCpuLimit", kErrorCpuLimit)
    V("kFlagTestingGlobals", kFlagTestingGlobals)
    V("kFlagTestingIgnoreTermination", kFlagTestingIgnoreTermination)
    V("kIsolateGroupDefault", static_cast<int32_t>(IsolateGroupKind::kDefault))
    V("kIsolateGroupNew", static_cast<int32_t>(IsolateGroupKind::kNew))
    V("kIsolateGroupShared", static_cast<int32_t>(IsolateGroupKind::kShared))
#undef V
  }

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
    registry->Register(New);
    registry->Register(PostMessage);
    registry->Register(Rpc);
    registry->Register(Evaluate);
    registry->Register(Terminate);
    registry->Register(SetWarmPoolSize);
    registry->Register(SetUnresponsiveTimeouts);
    WarmPoolWaiter::RegisterExternalReferences(registry);
    IsolateGroupWrap::RegisterExternalReferences(registry);
    GreenIsolate::RegisterBootstrapCallbacks(registry);
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
  // setUnresponsiveTimeouts(gracePeriodMs, abortTimeoutMs). Either may be
  // undefined, to keep the current value. Infinity means never.
  static void SetUnresponsiveTimeouts(const FunctionCallbackInfo<Value>& args) {
    auto nanoseconds = [](Local<Value> value) {
      const double milliseconds = value.As<Number>()->Value();
      CHECK_GE(milliseconds, 0);
      if (std::isinf(milliseconds)) return uint64_t{UINT64_MAX};
      return static_cast<uint64_t>(milliseconds) * uint64_t{1000000};
    };
    if (args[0]->IsNumber()) unresponsive_grace_period = nanoseconds(args[0]);
    if (args[1]->IsNumber()) cut_off_period = nanoseconds(args[1]);
  }

  // setWarmPoolSize(size, waiter)
  static void SetWarmPoolSize(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    CHECK(args[0]->IsUint32());
    WarmPoolWaiter* waiter;
    ASSIGN_OR_RETURN_UNWRAP(&waiter, args[1]);
    WarmPool::Get()->SetSize(args[0].As<Uint32>()->Value(),
                             env->isolate_data()->platform(),
                             waiter->channel());
  }

  // new GreenThread(source, filename, maxOldGenerationSizeMb,
  //                 maxYoungGenerationSizeMb, flags[, maxMemoryMb,
  //                 maxCpuTimeMs, maxTaskCpuTimeMs])
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
    options.testing_ignore_termination =
        (args[4].As<Uint32>()->Value() & kFlagTestingIgnoreTermination) != 0;
    auto milliseconds = [](Local<Value> value) -> uint64_t {
      constexpr double kMaxMilliseconds = 1e12;
      if (!value->IsNumber()) return 0;
      const double milliseconds = value.As<Number>()->Value();
      if (!(milliseconds > 0)) return 0;
      return static_cast<uint64_t>(
          std::max(std::min(milliseconds, kMaxMilliseconds) * 1e6, 1.0));
    };
    if (args[5]->IsNumber()) options.max_memory = megabytes(args[5]);
    options.max_cpu_time = milliseconds(args[6]);
    options.max_task_cpu_time = milliseconds(args[7]);
    // args[8]: an IsolateGroupKind, or undefined for the default. args[9]: the
    // IsolateGroup for kShared.
    if (args[8]->IsInt32()) {
      options.group =
          static_cast<IsolateGroupKind>(args[8].As<Int32>()->Value());
      CHECK(options.group == IsolateGroupKind::kDefault ||
            v8::IsolateGroup::CanCreateNewGroups());
      if (options.group == IsolateGroupKind::kShared) {
        IsolateGroupWrap* group;
        ASSIGN_OR_RETURN_UNWRAP(&group, args[9]);
        options.shared_group = group->group();
        options.group_id = group->id();
      }
    }
    // args[10]: the hooks of the host's RpcSession. args[11]: what the
    // runtime's initialize() gets, if anything, which is serialized with
    // them. args[12]: whether the source is an ES module.
    Local<Object> rpc_hooks;
    if (args[10]->IsObject()) {
      rpc_hooks = args[10].As<Object>();
      if (!args[11]->IsUndefined()) {
        std::optional<SerializedValue> bytes = Serialize(
            env->isolate(), env->context(), args[11], true, rpc_hooks);
        if (!bytes) return;
        options.init = std::move(*bytes);
      }
    }
    options.is_module = args[12]->IsTrue();

    auto* wrap = new GreenThreadWrap(env, args.This(), std::move(options));
    if (!rpc_hooks.IsEmpty()) wrap->rpc_hooks_.Reset(env->isolate(), rpc_hooks);
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
    MultiIsolatePlatform* platform = env->isolate_data()->platform();
    if (options.CanUseDefaultIsolate()) {
      green_ = WarmPool::Get()->Take(platform);
    }
    if (!green_) {
      green_ = std::make_shared<GreenIsolate>(platform);
      green_->Prepare(options);
    }
    green_->Activate(channel_, std::move(options));
  }

  // Stops the green thread and waits until its isolate is disposed, so that
  // it can never outlive the host Environment.
  void Detach() {
    if (!green_) return;
    channel_->Close();
    green_->TerminateAndWait(false);
    green_.reset();
  }

  // rpc(message, contextId)
  static void Rpc(const FunctionCallbackInfo<Value>& args) {
    GreenThreadWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
    CHECK(args[1]->IsUint32());
    if (!wrap->green_ || wrap->rpc_hooks_.IsEmpty()) return;
    Environment* env = wrap->env();
    std::optional<SerializedValue> bytes =
        Serialize(env->isolate(),
                  env->context(),
                  args[0],
                  true,
                  wrap->rpc_hooks_.Get(env->isolate()));
    if (!bytes) return;
    wrap->green_->PostRpc(std::move(*bytes), args[1].As<Uint32>()->Value());
  }

  static void PostMessage(const FunctionCallbackInfo<Value>& args) {
    GreenThreadWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
    if (!wrap->green_) return;
    Environment* env = wrap->env();
    Local<Value> transfer;
    if (!GetTransferList(env->context(), args[1]).ToLocal(&transfer)) return;
    HostPortAdapter ports(env);
    TransferOptions options;
    options.list = transfer;
    options.ports = &ports;
    std::optional<SerializedValue> bytes =
        Serialize(env->isolate(), env->context(), args[0], true, {}, options);
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
        Local<Object> rpc_hooks;
        if (event.type == kEventRpc && !rpc_hooks_.IsEmpty()) {
          rpc_hooks = rpc_hooks_.Get(isolate);
        }
        HostPortAdapter ports(env);
        if (!Deserialize(
                 isolate, context, event.payload, rpc_hooks, false, &ports)
                 .ToLocal(&value)) {
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
  Global<Object> rpc_hooks_;
};

}  // namespace

}  // namespace green_thread
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    green_thread, node::green_thread::GreenThreadWrap::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    green_thread,
    node::green_thread::GreenThreadWrap::RegisterExternalReferences)
