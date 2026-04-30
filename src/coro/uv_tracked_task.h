#ifndef SRC_CORO_UV_TRACKED_TASK_H_
#define SRC_CORO_UV_TRACKED_TASK_H_

#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <list>
#include <optional>
#include <type_traits>
#include <utility>

#include "async_context_frame.h"
#include "async_wrap.h"
#include "env-inl.h"
#include "node_internals.h"
#include "tracing/traced_value.h"
#include "v8.h"

#include "coro/uv_task.h"  // for detail::PromiseBase

namespace node {
namespace coro {

// ---------------------------------------------------------------------------
// CoroFrameAllocator -- thread-local free-list allocator for coroutine
// frames.  Since all coroutines run on the event loop thread, this is
// single-threaded and lock-free.
//
// Freed frames are kept on a per-size-class free list instead of being
// returned to malloc.  Size classes are rounded up to the next 256-byte
// boundary (coroutine frames are typically 500-2000 bytes).
// ---------------------------------------------------------------------------

namespace detail {

struct FreeBlock {
  FreeBlock* next;
  size_t size;
};

inline constexpr size_t kMaxCachedFrameSize = 4096;
inline constexpr size_t kSizeClassGranularity = 256;
inline constexpr size_t kNumSizeClasses =
    kMaxCachedFrameSize / kSizeClassGranularity;
inline constexpr size_t kMaxCachedPerBucket = 32;

inline size_t SizeClassIndex(size_t n) {
  return (n + kSizeClassGranularity - 1) / kSizeClassGranularity - 1;
}

inline size_t RoundUpToSizeClass(size_t n) {
  return (SizeClassIndex(n) + 1) * kSizeClassGranularity;
}

struct FreeBucket {
  FreeBlock* head = nullptr;
  size_t count = 0;
};

inline FreeBucket& GetBucket(size_t index) {
  static thread_local FreeBucket buckets[kNumSizeClasses] = {};
  return buckets[index];
}

inline void* CoroFrameAlloc(size_t n) {
  if (n <= kMaxCachedFrameSize) {
    size_t idx = SizeClassIndex(n);
    FreeBucket& bucket = GetBucket(idx);
    if (bucket.head != nullptr) {
      FreeBlock* block = bucket.head;
      bucket.head = block->next;
      bucket.count--;
      return block;
    }
    return ::operator new(RoundUpToSizeClass(n));
  }
  return ::operator new(n);
}

inline void CoroFrameFree(void* p, size_t n) {
  if (n <= kMaxCachedFrameSize) {
    size_t idx = SizeClassIndex(n);
    FreeBucket& bucket = GetBucket(idx);
    if (bucket.count >= kMaxCachedPerBucket) {
      ::operator delete(p, RoundUpToSizeClass(n));
      return;
    }
    auto* block = static_cast<FreeBlock*>(p);
    block->next = bucket.head;
    bucket.head = block;
    bucket.count++;
    return;
  }
  ::operator delete(p, n);
}

// Provider name lookup for trace events.  Matches the static array in
// async_wrap.cc but accessible from this header.
inline const char* ProviderName(AsyncWrap::ProviderType provider) {
  static const char* const names[] = {
#define V(PROVIDER) #PROVIDER,
      NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
  };
  return names[provider];
}

}  // namespace detail

// ---------------------------------------------------------------------------
// UvTrackedTask<T, Provider> -- A coroutine return type with full
// integration:
//
//   - InternalCallbackScope per resume segment (async_hooks, context frame,
//     microtask + nextTick draining)
//   - Event loop liveness via request_waiting_ counter
//   - Cancellation support via cancelable_req()
//   - Trace event emission
//   - Fatal error on silently swallowed exceptions
//   - Thread-local frame allocator for reduced malloc overhead
//   - Lazy resource object creation (only when hooks need it)
//   - Pre-cached per-Isolate type name string via IsolateData
//
// The Provider template parameter is an AsyncWrap::ProviderType enum value
// that identifies the async resource type visible to async_hooks and trace
// events.
// ---------------------------------------------------------------------------

template <typename T, AsyncWrap::ProviderType Provider>
class UvTrackedTask;

namespace detail {

// TrackedAwaitable -- wraps any awaitable with InternalCallbackScope
// management, request counter, and cancellation tracking.
template <typename Inner, typename Promise>
struct TrackedAwaitable {
  Inner inner_;
  Promise* promise_;

  bool await_ready() noexcept { return inner_.await_ready(); }

  template <typename H>
  auto await_suspend(H h) noexcept {
    promise_->on_suspend();

    if constexpr (requires { inner_.cancelable_req(); }) {
      promise_->on_dispatch(inner_.cancelable_req());
    }

    return inner_.await_suspend(h);
  }

  auto await_resume() noexcept(noexcept(inner_.await_resume())) {
    if constexpr (requires { inner_.cancelable_req(); }) {
      promise_->on_io_complete();
    }

    promise_->on_resume();
    return inner_.await_resume();
  }
};

// TrackedPromiseBase -- extends PromiseBase with full Node.js integration.
struct TrackedPromiseBase : PromiseBase {
  Environment* env_ = nullptr;
  AsyncWrap::ProviderType provider_ = AsyncWrap::PROVIDER_NONE;
  double async_id_ = -1;
  double trigger_async_id_ = -1;

  v8::Global<v8::Value> context_frame_;
  v8::Global<v8::Object> resource_;

  std::optional<v8::HandleScope> handle_scope_;
  std::optional<InternalCallbackScope> scope_;

  uv_req_t* active_req_ = nullptr;

  std::list<TrackedPromiseBase*>::iterator coro_list_iter_;

  static void* operator new(size_t n) { return CoroFrameAlloc(n); }
  static void operator delete(void* p, size_t n) { CoroFrameFree(p, n); }

  void init_tracking(Environment* env, AsyncWrap::ProviderType provider) {
    env_ = env;
    provider_ = provider;
    v8::Isolate* isolate = env->isolate();
    v8::HandleScope handle_scope(isolate);

    async_id_ = env->new_async_id();
    trigger_async_id_ = env->get_default_trigger_async_id();

    context_frame_.Reset(isolate, async_context_frame::current(isolate));

    env->coro_task_list()->push_back(this);
    coro_list_iter_ = std::prev(env->coro_task_list()->end());

    // Trace event: use the provider name from the enum.
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(TRACING_CATEGORY_NODE1(async_hooks),
                                      ProviderName(provider),
                                      static_cast<int64_t>(async_id_),
                                      "triggerAsyncId",
                                      static_cast<int64_t>(trigger_async_id_));

    AsyncHooks* hooks = env->async_hooks();
    bool needs_resource =
        hooks->fields()[AsyncHooks::kInit] > 0 ||
        hooks->fields()[AsyncHooks::kUsesExecutionAsyncResource] > 0;

    if (needs_resource) {
      resource_.Reset(isolate, v8::Object::New(isolate));
    }

    if (hooks->fields()[AsyncHooks::kInit] > 0) {
      // The type name V8 string is pre-cached per Isolate in
      // IsolateData::async_wrap_providers_, created at Isolate startup.
      // Zero per-coroutine allocation cost.
      v8::Local<v8::String> type =
          env->isolate_data()->async_wrap_provider(provider);
      AsyncWrap::EmitAsyncInit(
          env, resource_.Get(isolate), type, async_id_, trigger_async_id_);
    }
  }

  void on_resume() noexcept {
    if (env_ == nullptr) return;

    handle_scope_.emplace(env_->isolate());

    v8::Global<v8::Object>* res_ptr =
        resource_.IsEmpty() ? nullptr : &resource_;
    scope_.emplace(env_,
                   res_ptr,
                   async_context{async_id_, trigger_async_id_},
                   InternalCallbackScope::kNoFlags,
                   context_frame_.Get(env_->isolate()));
  }

  void on_suspend() noexcept {
    if (!scope_.has_value()) return;
    scope_->Close();
    scope_.reset();
    handle_scope_.reset();
  }

  void on_dispatch(uv_req_t* req) noexcept {
    active_req_ = req;
    if (env_ != nullptr) {
      env_->IncreaseWaitingRequestCounter();
    }
  }

  void on_io_complete() noexcept {
    active_req_ = nullptr;
    if (env_ != nullptr) {
      env_->DecreaseWaitingRequestCounter();
    }
  }

  void on_destroy() noexcept {
    if (env_ == nullptr) return;

    env_->coro_task_list()->erase(coro_list_iter_);

    AsyncWrap::EmitDestroy(env_, async_id_);

    TRACE_EVENT_NESTABLE_ASYNC_END0(TRACING_CATEGORY_NODE1(async_hooks),
                                    ProviderName(provider_),
                                    static_cast<int64_t>(async_id_));

    context_frame_.Reset();
    resource_.Reset();
  }

  void Cancel() {
    if (active_req_ != nullptr) {
      uv_cancel(active_req_);
    }
  }
};

template <typename Promise>
struct TrackedInitialAwaiter {
  bool await_ready() noexcept { return false; }
  void await_suspend(std::coroutine_handle<>) noexcept {}
  void await_resume() noexcept { promise_.on_resume(); }

  Promise& promise_;
};

template <typename Promise>
struct TrackedFinalAwaiter {
  bool await_ready() noexcept {
    promise_.on_suspend();

    if (promise_.detached_ && promise_.exception_) {
      std::fprintf(stderr,
                   "FATAL: Unhandled C++ exception in detached coroutine\n");
      std::fflush(stderr);
      std::terminate();
    }

    promise_.on_destroy();
    return promise_.detached_;
  }

  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise>) noexcept {
    if (auto cont = promise_.continuation_) return cont;
    return std::noop_coroutine();
  }

  void await_resume() noexcept {}

  Promise& promise_;
};

}  // namespace detail

// ===========================================================================
// UvTrackedTask<T, Provider> -- tracked coroutine that produces a T value
// ===========================================================================
template <typename T, AsyncWrap::ProviderType Provider>
class UvTrackedTask {
 public:
  struct promise_type : detail::TrackedPromiseBase {
    std::optional<T> result_;

    UvTrackedTask get_return_object() {
      return UvTrackedTask{
          std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    auto initial_suspend() noexcept {
      return detail::TrackedInitialAwaiter<promise_type>{*this};
    }

    auto final_suspend() noexcept {
      return detail::TrackedFinalAwaiter<promise_type>{*this};
    }

    void return_value(T value) { result_.emplace(std::move(value)); }

    template <typename Awaitable>
    auto await_transform(Awaitable&& aw) {
      return detail::TrackedAwaitable<std::decay_t<Awaitable>, promise_type>{
          std::forward<Awaitable>(aw), this};
    }
  };

  bool await_ready() const noexcept { return false; }

  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> caller) noexcept {
    handle_.promise().continuation_ = caller;
    return handle_;
  }

  T await_resume() {
    handle_.promise().rethrow_if_exception();
    return std::move(*handle_.promise().result_);
  }

  void InitTracking(Environment* env) {
    handle_.promise().init_tracking(env, Provider);
  }

  void Start() {
    handle_.promise().detached_ = true;
    auto h = std::exchange(handle_, nullptr);
    h.resume();
  }

  bool Done() const { return handle_ && handle_.done(); }

  ~UvTrackedTask() {
    if (handle_) handle_.destroy();
  }

  UvTrackedTask(UvTrackedTask&& o) noexcept
      : handle_(std::exchange(o.handle_, nullptr)) {}
  UvTrackedTask& operator=(UvTrackedTask&& o) noexcept {
    if (this != &o) {
      if (handle_) handle_.destroy();
      handle_ = std::exchange(o.handle_, nullptr);
    }
    return *this;
  }

  UvTrackedTask(const UvTrackedTask&) = delete;
  UvTrackedTask& operator=(const UvTrackedTask&) = delete;

 private:
  explicit UvTrackedTask(std::coroutine_handle<promise_type> h) : handle_(h) {}
  std::coroutine_handle<promise_type> handle_;
};

// ===========================================================================
// Partial specialization for T = void
// ===========================================================================
template <AsyncWrap::ProviderType Provider>
class UvTrackedTask<void, Provider> {
 public:
  struct promise_type : detail::TrackedPromiseBase {
    UvTrackedTask get_return_object() {
      return UvTrackedTask{
          std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    auto initial_suspend() noexcept {
      return detail::TrackedInitialAwaiter<promise_type>{*this};
    }

    auto final_suspend() noexcept {
      return detail::TrackedFinalAwaiter<promise_type>{*this};
    }

    void return_void() {}

    template <typename Awaitable>
    auto await_transform(Awaitable&& aw) {
      return detail::TrackedAwaitable<std::decay_t<Awaitable>, promise_type>{
          std::forward<Awaitable>(aw), this};
    }
  };

  bool await_ready() const noexcept { return false; }

  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> caller) noexcept {
    handle_.promise().continuation_ = caller;
    return handle_;
  }

  void await_resume() { handle_.promise().rethrow_if_exception(); }

  void InitTracking(Environment* env) {
    handle_.promise().init_tracking(env, Provider);
  }

  void Start() {
    handle_.promise().detached_ = true;
    auto h = std::exchange(handle_, nullptr);
    h.resume();
  }

  bool Done() const { return handle_ && handle_.done(); }

  ~UvTrackedTask() {
    if (handle_) handle_.destroy();
  }

  UvTrackedTask(UvTrackedTask&& o) noexcept
      : handle_(std::exchange(o.handle_, nullptr)) {}
  UvTrackedTask& operator=(UvTrackedTask&& o) noexcept {
    if (this != &o) {
      if (handle_) handle_.destroy();
      handle_ = std::exchange(o.handle_, nullptr);
    }
    return *this;
  }

  UvTrackedTask(const UvTrackedTask&) = delete;
  UvTrackedTask& operator=(const UvTrackedTask&) = delete;

 private:
  explicit UvTrackedTask(std::coroutine_handle<promise_type> h) : handle_(h) {}
  std::coroutine_handle<promise_type> handle_;
};

}  // namespace coro
}  // namespace node

#endif  // SRC_CORO_UV_TRACKED_TASK_H_
