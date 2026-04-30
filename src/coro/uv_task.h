#ifndef SRC_CORO_UV_TASK_H_
#define SRC_CORO_UV_TASK_H_

#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

namespace node {
namespace coro {

// ---------------------------------------------------------------------------
// UvTask<T> — A coroutine return type for libuv-based async operations.
//
// Supports two usage modes:
//
//   1. Composition: co_await a UvTask from another coroutine.
//      UvTask<int> inner() { co_return 42; }
//      UvTask<void> outer() { int v = co_await inner(); }
//
//   2. Fire-and-forget: call Start() from non-coroutine code.
//      The coroutine self-destructs when it completes.
//      auto task = my_coroutine(args...);
//      task.Start();  // runs until first co_await, then returns
//
// The task uses lazy initialization (suspends at initial_suspend).
// Execution begins when the task is co_awaited or Start() is called.
// ---------------------------------------------------------------------------

template <typename T = void>
class UvTask;

namespace detail {

// FinalAwaiter handles coroutine completion.  If the coroutine was
// co_awaited by a parent, symmetric-transfer to the parent.  If it
// was Start()ed (fire-and-forget / detached), allow the frame to
// self-destruct.
template <typename Promise>
struct FinalAwaiter {
  bool await_ready() noexcept {
    // If detached, "ready" means don't suspend — the frame is destroyed
    // automatically when final_suspend's co_await doesn't suspend.
    return promise_.detached_;
  }

  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<Promise>) noexcept {
    // Not detached — symmetric transfer to the continuation (parent
    // coroutine), or noop if there is none.
    if (auto cont = promise_.continuation_) return cont;
    return std::noop_coroutine();
  }

  void await_resume() noexcept {}

  Promise& promise_;
};

// Common promise base — holds the continuation, detached flag, and
// exception state shared by both T and void specializations.
struct PromiseBase {
  std::coroutine_handle<> continuation_;
  std::exception_ptr exception_;
  bool detached_ = false;

  std::suspend_always initial_suspend() noexcept { return {}; }

  void unhandled_exception() noexcept { exception_ = std::current_exception(); }

  void rethrow_if_exception() {
    if (exception_) std::rethrow_exception(exception_);
  }
};

}  // namespace detail

// ===========================================================================
// UvTask<T> — coroutine that produces a T value
// ===========================================================================
template <typename T>
class UvTask {
 public:
  struct promise_type : detail::PromiseBase {
    std::optional<T> result_;

    UvTask get_return_object() {
      return UvTask{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    auto final_suspend() noexcept {
      return detail::FinalAwaiter<promise_type>{*this};
    }

    void return_value(T value) { result_.emplace(std::move(value)); }
  };

  // -- Awaitable interface (for co_await from another coroutine) --

  bool await_ready() const noexcept { return false; }

  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> caller) noexcept {
    handle_.promise().continuation_ = caller;
    return handle_;  // symmetric transfer — start this task
  }

  T await_resume() {
    handle_.promise().rethrow_if_exception();
    return std::move(*handle_.promise().result_);
  }

  // -- Fire-and-forget interface (for non-coroutine callers) --

  // Start the coroutine.  It will run until its first co_await
  // suspension point, then control returns here.  When the coroutine
  // eventually completes, the frame self-destructs.
  void Start() {
    handle_.promise().detached_ = true;
    auto h = std::exchange(handle_, nullptr);
    h.resume();
  }

  bool Done() const { return handle_ && handle_.done(); }

  // -- Lifecycle --

  ~UvTask() {
    if (handle_) handle_.destroy();
  }

  UvTask(UvTask&& o) noexcept : handle_(std::exchange(o.handle_, nullptr)) {}
  UvTask& operator=(UvTask&& o) noexcept {
    if (this != &o) {
      if (handle_) handle_.destroy();
      handle_ = std::exchange(o.handle_, nullptr);
    }
    return *this;
  }

  UvTask(const UvTask&) = delete;
  UvTask& operator=(const UvTask&) = delete;

 private:
  explicit UvTask(std::coroutine_handle<promise_type> h) : handle_(h) {}
  std::coroutine_handle<promise_type> handle_;
};

// ===========================================================================
// UvTask<void> — coroutine that produces no value
// ===========================================================================
template <>
class UvTask<void> {
 public:
  struct promise_type : detail::PromiseBase {
    UvTask get_return_object() {
      return UvTask{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    auto final_suspend() noexcept {
      return detail::FinalAwaiter<promise_type>{*this};
    }

    void return_void() {}
  };

  bool await_ready() const noexcept { return false; }

  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> caller) noexcept {
    handle_.promise().continuation_ = caller;
    return handle_;
  }

  void await_resume() { handle_.promise().rethrow_if_exception(); }

  void Start() {
    handle_.promise().detached_ = true;
    auto h = std::exchange(handle_, nullptr);
    h.resume();
  }

  bool Done() const { return handle_ && handle_.done(); }

  ~UvTask() {
    if (handle_) handle_.destroy();
  }

  UvTask(UvTask&& o) noexcept : handle_(std::exchange(o.handle_, nullptr)) {}
  UvTask& operator=(UvTask&& o) noexcept {
    if (this != &o) {
      if (handle_) handle_.destroy();
      handle_ = std::exchange(o.handle_, nullptr);
    }
    return *this;
  }

  UvTask(const UvTask&) = delete;
  UvTask& operator=(const UvTask&) = delete;

 private:
  explicit UvTask(std::coroutine_handle<promise_type> h) : handle_(h) {}
  std::coroutine_handle<promise_type> handle_;
};

}  // namespace coro
}  // namespace node

#endif  // SRC_CORO_UV_TASK_H_
