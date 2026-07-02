#ifndef SRC_CORO_UV_AWAITABLE_H_
#define SRC_CORO_UV_AWAITABLE_H_

#include <coroutine>
#include <cstring>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "uv.h"

namespace node {
namespace coro {

// ---------------------------------------------------------------------------
// UvFsAwaitable — Awaitable that wraps a single uv_fs_* async operation.
//
// Created via the UvFs() factory function.  Usage:
//
//   ssize_t fd = co_await UvFs(loop, uv_fs_open, path, O_RDONLY, 0644);
//   if (fd < 0) { /* handle error: fd is the negative uv_errno_t */ }
//
// The uv_fs_t request struct is embedded directly in this object, which
// lives in the coroutine frame across the suspension point.  This means
// no separate heap allocation is needed for the request.
//
// On await_resume(), uv_fs_req_cleanup() is called automatically.
// ---------------------------------------------------------------------------

template <typename Fn, typename... Args>
class UvFsAwaitable {
 public:
  UvFsAwaitable(uv_loop_t* loop, Fn fn, Args... args)
      : loop_(loop), fn_(fn), args_(std::move(args)...) {
    memset(&req_, 0, sizeof(req_));
  }

  ~UvFsAwaitable() {
    // Safety net: if the coroutine is destroyed without resuming
    // (e.g. cancellation), still clean up any libuv-internal state.
    if (needs_cleanup_) uv_fs_req_cleanup(&req_);
  }

  // Move is safe before dispatch (req_.data isn't set until await_suspend).
  UvFsAwaitable(UvFsAwaitable&& o) noexcept
      : loop_(o.loop_),
        fn_(o.fn_),
        args_(std::move(o.args_)),
        req_(o.req_),
        handle_(o.handle_),
        needs_cleanup_(std::exchange(o.needs_cleanup_, false)) {
    memset(&o.req_, 0, sizeof(o.req_));
  }
  UvFsAwaitable(const UvFsAwaitable&) = delete;
  UvFsAwaitable& operator=(const UvFsAwaitable&) = delete;
  UvFsAwaitable& operator=(UvFsAwaitable&&) = delete;

  bool await_ready() noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) noexcept {
    handle_ = h;
    req_.data = this;

    // Dispatch: call uv_fs_*(loop, &req_, ...args..., OnComplete)
    int err = std::apply(
        [this](auto&&... a) {
          return fn_(loop_, &req_, std::forward<decltype(a)>(a)..., OnComplete);
        },
        args_);

    if (err < 0) {
      // Synchronous dispatch failure — store the error in req_.result
      // so await_resume sees it, and resume immediately.
      req_.result = err;
      needs_cleanup_ = false;  // nothing was dispatched
      h.resume();
    } else {
      needs_cleanup_ = true;
    }
  }

  // Returns req->result: the fd for open, byte count for read/write,
  // 0 for success, or a negative uv_errno_t on error.
  ssize_t await_resume() noexcept {
    ssize_t result = req_.result;
    uv_fs_req_cleanup(&req_);
    needs_cleanup_ = false;
    return result;
  }

  // Access the raw request (useful before cleanup for stat, readdir, etc.)
  const uv_fs_t& req() const { return req_; }

  // Returns a pointer to the underlying uv_req_t for cancellation support.
  // Used by TrackedAwaitable to register with the request counter and
  // cancellation infrastructure.
  uv_req_t* cancelable_req() { return reinterpret_cast<uv_req_t*>(&req_); }

 private:
  static void OnComplete(uv_fs_t* req) {
    auto* self = static_cast<UvFsAwaitable*>(req->data);
    self->handle_.resume();
  }

  uv_loop_t* loop_;
  Fn fn_;
  std::tuple<Args...> args_;
  uv_fs_t req_;
  std::coroutine_handle<> handle_;
  bool needs_cleanup_ = false;
};

// ---------------------------------------------------------------------------
// UvFs() — Factory function for creating a UvFsAwaitable.
//
// Deduces the libuv function signature and argument types automatically.
// The callback parameter is NOT passed — it's injected by the awaitable.
//
// Examples:
//   co_await UvFs(loop, uv_fs_open, "/tmp/test", O_RDONLY, 0644);
//   co_await UvFs(loop, uv_fs_close, fd);
//   co_await UvFs(loop, uv_fs_stat, "/tmp/test");
//   co_await UvFs(loop, uv_fs_read, fd, &iov, 1, offset);
// ---------------------------------------------------------------------------

template <typename Fn, typename... Args>
auto UvFs(uv_loop_t* loop, Fn fn, Args... args) {
  return UvFsAwaitable<Fn, Args...>(loop, fn, std::move(args)...);
}

// ---------------------------------------------------------------------------
// UvFsStatAwaitable — Like UvFsAwaitable but await_resume returns the
// uv_stat_t instead of the raw result code.  Used for stat/fstat/lstat.
//
// Usage:
//   auto [err, stat] = co_await UvFsStat(loop, uv_fs_stat, path);
// ---------------------------------------------------------------------------

template <typename Fn, typename... Args>
class UvFsStatAwaitable {
 public:
  UvFsStatAwaitable(uv_loop_t* loop, Fn fn, Args... args)
      : loop_(loop), fn_(fn), args_(std::move(args)...) {
    memset(&req_, 0, sizeof(req_));
  }

  ~UvFsStatAwaitable() {
    if (needs_cleanup_) uv_fs_req_cleanup(&req_);
  }

  UvFsStatAwaitable(UvFsStatAwaitable&& o) noexcept
      : loop_(o.loop_),
        fn_(o.fn_),
        args_(std::move(o.args_)),
        req_(o.req_),
        handle_(o.handle_),
        needs_cleanup_(std::exchange(o.needs_cleanup_, false)) {
    memset(&o.req_, 0, sizeof(o.req_));
  }
  UvFsStatAwaitable(const UvFsStatAwaitable&) = delete;
  UvFsStatAwaitable& operator=(const UvFsStatAwaitable&) = delete;
  UvFsStatAwaitable& operator=(UvFsStatAwaitable&&) = delete;

  bool await_ready() noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) noexcept {
    handle_ = h;
    req_.data = this;

    int err = std::apply(
        [this](auto&&... a) {
          return fn_(loop_, &req_, std::forward<decltype(a)>(a)..., OnComplete);
        },
        args_);

    if (err < 0) {
      req_.result = err;
      needs_cleanup_ = false;
      h.resume();
    } else {
      needs_cleanup_ = true;
    }
  }

  struct StatResult {
    int error;       // 0 on success, negative uv_errno_t on failure
    uv_stat_t stat;  // valid only when error == 0
  };

  StatResult await_resume() noexcept {
    StatResult r{};
    if (req_.result < 0) {
      r.error = static_cast<int>(req_.result);
    } else {
      r.error = 0;
      r.stat = req_.statbuf;
    }
    uv_fs_req_cleanup(&req_);
    needs_cleanup_ = false;
    return r;
  }

  uv_req_t* cancelable_req() { return reinterpret_cast<uv_req_t*>(&req_); }

 private:
  static void OnComplete(uv_fs_t* req) {
    auto* self = static_cast<UvFsStatAwaitable*>(req->data);
    self->handle_.resume();
  }

  uv_loop_t* loop_;
  Fn fn_;
  std::tuple<Args...> args_;
  uv_fs_t req_;
  std::coroutine_handle<> handle_;
  bool needs_cleanup_ = false;
};

template <typename Fn, typename... Args>
auto UvFsStat(uv_loop_t* loop, Fn fn, Args... args) {
  return UvFsStatAwaitable<Fn, Args...>(loop, fn, std::move(args)...);
}

// ---------------------------------------------------------------------------
// UvWork — Awaitable that wraps uv_queue_work.
//
// Runs a callable on the libuv thread pool.  The coroutine suspends
// until the work is complete, then resumes on the event loop thread.
//
// Usage:
//   int status = co_await UvWork(loop, [&]() {
//     // expensive computation on thread pool
//   });
// ---------------------------------------------------------------------------

template <typename WorkFn>
class UvWorkAwaitable {
 public:
  UvWorkAwaitable(uv_loop_t* loop, WorkFn work_fn)
      : loop_(loop), work_fn_(std::move(work_fn)) {
    memset(&req_, 0, sizeof(req_));
  }

  ~UvWorkAwaitable() = default;

  UvWorkAwaitable(const UvWorkAwaitable&) = delete;
  UvWorkAwaitable& operator=(const UvWorkAwaitable&) = delete;
  UvWorkAwaitable(UvWorkAwaitable&&) = delete;
  UvWorkAwaitable& operator=(UvWorkAwaitable&&) = delete;

  bool await_ready() noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) noexcept {
    handle_ = h;
    req_.data = this;

    int err = uv_queue_work(loop_, &req_, OnWork, OnAfterWork);
    if (err < 0) {
      status_ = err;
      h.resume();
    }
  }

  // Returns 0 on success, UV_ECANCELED if cancelled, or negative error.
  int await_resume() noexcept { return status_; }

  uv_req_t* cancelable_req() { return reinterpret_cast<uv_req_t*>(&req_); }

 private:
  static void OnWork(uv_work_t* req) {
    auto* self = static_cast<UvWorkAwaitable*>(req->data);
    self->work_fn_();
  }

  static void OnAfterWork(uv_work_t* req, int status) {
    auto* self = static_cast<UvWorkAwaitable*>(req->data);
    self->status_ = status;
    self->handle_.resume();
  }

  uv_loop_t* loop_;
  WorkFn work_fn_;
  uv_work_t req_;
  std::coroutine_handle<> handle_;
  int status_ = 0;
};

template <typename WorkFn>
auto UvWork(uv_loop_t* loop, WorkFn&& work_fn) {
  return UvWorkAwaitable<std::decay_t<WorkFn>>(loop,
                                               std::forward<WorkFn>(work_fn));
}

// ---------------------------------------------------------------------------
// UvGetAddrInfo — Awaitable wrapper for uv_getaddrinfo.
//
// Usage:
//   auto [status, info] = co_await UvGetAddrInfo(loop, "example.com", "80",
//                                                 &hints);
//   if (status == 0) {
//     // use info...
//     uv_freeaddrinfo(info);
//   }
// ---------------------------------------------------------------------------

class UvGetAddrInfoAwaitable {
 public:
  UvGetAddrInfoAwaitable(uv_loop_t* loop,
                         const char* node,
                         const char* service,
                         const struct addrinfo* hints)
      : loop_(loop), node_(node), service_(service), hints_(hints) {
    memset(&req_, 0, sizeof(req_));
  }

  ~UvGetAddrInfoAwaitable() = default;

  UvGetAddrInfoAwaitable(const UvGetAddrInfoAwaitable&) = delete;
  UvGetAddrInfoAwaitable& operator=(const UvGetAddrInfoAwaitable&) = delete;

  bool await_ready() noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) noexcept {
    handle_ = h;
    req_.data = this;

    int err = uv_getaddrinfo(loop_, &req_, OnComplete, node_, service_, hints_);
    if (err < 0) {
      status_ = err;
      addrinfo_ = nullptr;
      h.resume();
    }
  }

  struct AddrInfoResult {
    int status;                 // 0 on success, negative uv_errno_t on error
    struct addrinfo* addrinfo;  // caller must uv_freeaddrinfo() on success
  };

  AddrInfoResult await_resume() noexcept { return {status_, addrinfo_}; }

  uv_req_t* cancelable_req() { return reinterpret_cast<uv_req_t*>(&req_); }

 private:
  static void OnComplete(uv_getaddrinfo_t* req,
                         int status,
                         struct addrinfo* res) {
    auto* self = static_cast<UvGetAddrInfoAwaitable*>(req->data);
    self->status_ = status;
    self->addrinfo_ = res;
    self->handle_.resume();
  }

  uv_loop_t* loop_;
  const char* node_;
  const char* service_;
  const struct addrinfo* hints_;
  uv_getaddrinfo_t req_;
  std::coroutine_handle<> handle_;
  int status_ = 0;
  struct addrinfo* addrinfo_ = nullptr;
};

inline auto UvGetAddrInfo(uv_loop_t* loop,
                          const char* node,
                          const char* service,
                          const struct addrinfo* hints = nullptr) {
  return UvGetAddrInfoAwaitable(loop, node, service, hints);
}

}  // namespace coro
}  // namespace node

#endif  // SRC_CORO_UV_AWAITABLE_H_
