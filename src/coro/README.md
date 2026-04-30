# C++20 Coroutine support for libuv

This directory contains an experimental C++20 coroutine layer for writing
asynchronous libuv operations as sequential C++ code using `co_await`.

The primary goal is to allow multi-step async operations (such as
open + stat + read + close) to be written as straight-line C++ instead of
callback chains, while maintaining full integration with Node.js async\_hooks,
AsyncLocalStorage, microtask draining, and environment lifecycle management.

## File overview

* `uv_task.h` -- `UvTask<T>`: The lightweight, untracked coroutine return type.
  No V8 or Node.js dependencies. Suitable for internal C++ coroutines that do
  not need async\_hooks visibility or task queue draining.

* `uv_tracked_task.h` -- `UvTrackedTask<T, Provider>`: The fully-integrated
  coroutine return type. Each resume-to-suspend segment is wrapped in an
  `InternalCallbackScope`, giving it the same semantics as any other callback
  entry into Node.js. The `Provider` template parameter is an
  `AsyncWrap::ProviderType` enum value that identifies the async resource type
  visible to `async_hooks.createHook()` and trace events, using the same
  type registry as the rest of Node.js.

* `uv_awaitable.h` -- Awaitable wrappers for libuv async operations:
  `UvFsAwaitable` (fs operations), `UvFsStatAwaitable` (stat-family),
  `UvWorkAwaitable` (thread pool work), and `UvGetAddrInfoAwaitable`
  (DNS resolution). Each embeds the libuv request struct directly in the
  coroutine frame, avoiding separate heap allocations. Each also exposes a
  `cancelable_req()` method returning the underlying `uv_req_t*` for
  cancellation support during environment teardown.

* `uv_promise.h` -- Helpers for bridging coroutines to JavaScript Promises:
  `MakePromise()`, `ResolvePromise()`, `RejectPromiseWithUVError()`. The
  resolve and reject helpers guard against calling V8 APIs when the
  environment is shutting down (`can_call_into_js()` check).

## Usage

### Basic pattern (binding function)

```cpp
// The coroutine. The return type carries the provider type as
// a compile-time template argument.
static coro::UvTrackedTask<void, AsyncWrap::PROVIDER_FSREQPROMISE>
DoAccessImpl(
    Environment* env,
    v8::Global<v8::Promise::Resolver> resolver,
    std::string path,
    int mode) {
  ssize_t result = co_await coro::UvFs(
      env->event_loop(), uv_fs_access, path.c_str(), mode);
  if (result < 0)
    coro::RejectPromiseWithUVError(env, resolver, result, "access",
                                   path.c_str());
  else
    coro::ResolvePromiseUndefined(env, resolver);
}

// The binding entry point (called from JavaScript).
static void Access(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  // ... parse args, check permissions ...

  auto resolver = coro::MakePromise(env, args);
  auto task = DoAccessImpl(env, std::move(resolver), path, mode);
  task.InitTracking(env);   // assigns async_id, captures context, emits init
  task.Start();              // begins execution (fire-and-forget)
}
```

### Multi-step operations

Multiple libuv calls within a single coroutine are sequential co\_await
expressions. The intermediate steps (between two co\_await points) are pure C++
with no V8 overhead:

```cpp
static coro::UvTrackedTask<void, AsyncWrap::PROVIDER_COROREADFILE>
ReadFileImpl(
    Environment* env,
    v8::Global<v8::Promise::Resolver> resolver,
    std::string path) {
  ssize_t fd = co_await coro::UvFs(
      env->event_loop(), uv_fs_open, path.c_str(), O_RDONLY, 0);
  if (fd < 0) { /* reject and co_return */ }

  auto [err, stat] = co_await coro::UvFsStat(
      env->event_loop(), uv_fs_fstat, static_cast<uv_file>(fd));
  // ... read, close, resolve ...
}
```

### Coroutine composition

`UvTask<T>` and `UvTrackedTask<T, Provider>` can be co\_awaited from other
coroutines. This allows factoring common operations into reusable helpers:

```cpp
UvTask<ssize_t> OpenFile(uv_loop_t* loop, const char* path, int flags) {
  co_return co_await UvFs(loop, uv_fs_open, path, flags, 0);
}

UvTrackedTask<void, AsyncWrap::PROVIDER_FSREQPROMISE>
OuterCoroutine(Environment* env, ...) {
  ssize_t fd = co_await OpenFile(env->event_loop(), path, O_RDONLY);
  // ...
}
```

## Lifecycle

### UvTask (untracked)

`UvTask<T>` uses lazy initialization. The coroutine does not run until it is
either co\_awaited from another coroutine (symmetric transfer) or explicitly
started with `Start()`. When `Start()` is called, the coroutine runs until its
first `co_await`, then control returns to the caller. The coroutine frame
self-destructs when the coroutine completes.

### UvTrackedTask (tracked)

`UvTrackedTask<T, Provider>` follows the same lazy/fire-and-forget pattern
but adds three phases around `Start()`:

1. **Creation**: The coroutine frame is allocated from the thread-local
   free-list (see "Frame allocator" below). The coroutine is suspended at
   `initial_suspend` (lazy).

2. **`InitTracking(env)`**: Assigns an `async_id`, captures the current
   `async_context_frame` (for AsyncLocalStorage propagation), emits a trace
   event using the provider name from the `ProviderType` enum, and registers
   in the Environment's coroutine task list for cancellation during teardown.
   If async\_hooks listeners are active (`kInit > 0` or
   `kUsesExecutionAsyncResource > 0`), a resource object is created for
   `executionAsyncResource()` and the `init` hook is emitted. The type name
   V8 string comes from `IsolateData::async_wrap_providers_`, which is
   pre-cached at Isolate startup with zero per-coroutine allocation cost.

3. **`Start()`**: Marks the task as detached (fire-and-forget) and resumes
   the coroutine. Each resume-to-suspend segment is wrapped in an
   `InternalCallbackScope` that provides:
   * async\_hooks `before`/`after` events
   * `async_context_frame` save/restore (AsyncLocalStorage)
   * Microtask and `process.nextTick` draining on close
   * `request_waiting_` counter management for event loop liveness

4. **Completion**: At `final_suspend`, the last `InternalCallbackScope` is
   closed (draining task queues), the async\_hooks `destroy` event is emitted,
   the task is unregistered from the Environment, and the coroutine frame is
   returned to the thread-local free-list. If a detached coroutine has a
   captured C++ exception that was never observed, `std::terminate()` is
   called rather than silently discarding it.

## How the awaitable dispatch works

The `UvFs()` factory function returns a `UvFsAwaitable` that embeds a `uv_fs_t`
directly in the coroutine frame. When the coroutine hits `co_await`:

1. `await_transform()` on the promise wraps it in a `TrackedAwaitable`.
2. `TrackedAwaitable::await_suspend()`:
   * Closes the current `InternalCallbackScope` (drains microtasks/nextTick).
   * Records the `uv_req_t*` for cancellation support (via `cancelable_req()`).
   * Increments `request_waiting_` (event loop liveness).
   * Calls the inner `await_suspend()`, which dispatches the libuv call with
     `req_.data = this` pointing back to the awaitable.
3. The coroutine is suspended. Control returns to the event loop.
4. When the libuv operation completes, `OnComplete()` calls
   `handle_.resume()` to resume the coroutine.
5. `TrackedAwaitable::await_resume()`:
   * Decrements `request_waiting_`.
   * Clears the cancellation pointer.
   * Opens a new `InternalCallbackScope` for the next segment.
   * Returns the result (e.g., `req_.result` for fs operations).

The liveness counter and cancellation tracking are conditional on the inner
awaitable having a `cancelable_req()` method (checked at compile time via a
`requires` expression). When co\_awaiting another `UvTask` or `UvTrackedTask`
(coroutine composition), these steps are skipped.

## Environment teardown

During `Environment::CleanupHandles()`, the coroutine task list is iterated and
`Cancel()` is called on each active task. This calls `uv_cancel()` on the
in-flight libuv request (if any), which causes the libuv callback to fire with
`UV_ECANCELED`. The coroutine resumes, sees the error, and completes normally.
The `request_waiting_` counter ensures the teardown loop waits for all
coroutine I/O to finish before destroying the Environment.

## Frame allocator

Coroutine frames are allocated from a thread-local free-list rather than going
through `malloc`/`free` on every creation and destruction. This is implemented
via `promise_type::operator new` and `operator delete` in `TrackedPromiseBase`,
which route through `CoroFrameAlloc()` and `CoroFrameFree()`.

The free-list uses size-class buckets with 256-byte granularity, covering
frames up to 4096 bytes (which covers typical coroutine frames). Frames larger
than 4096 bytes fall through to the global `operator new`. Since all coroutines
run on the event loop thread, the free-list requires no locking.

Each bucket has a high-water mark of 32 cached frames. When a frame is freed
and its bucket is already at capacity, the frame is returned directly to the
system allocator instead of being cached. This bounds the retained memory
per bucket to at most 32 \* bucket\_size bytes (e.g., 32 \* 1024 = 32KB for the
1024-byte size class), preventing unbounded growth after a burst of concurrent
coroutines.

After the first coroutine of a given size class completes, subsequent
coroutines of the same size class are allocated from the free-list with zero
`malloc` overhead.

## Allocation comparison with ReqWrap

For a single async operation (e.g., `fsPromises.access`):

|                      | ReqWrap pattern | Coroutine (no hooks) | Coroutine (hooks active) |
| -------------------- | --------------- | -------------------- | ------------------------ |
| C++ heap allocations | 3               | 0 (free-list hit)    | 0 (free-list hit)        |
| V8 heap objects      | 7               | 2 (resolver+promise) | 3 (+ resource object)    |
| Total allocations    | 10              | 2                    | 3                        |

For a multi-step operation (open + stat + read + close):

|                               | 4x ReqWrap | Single coroutine (no hooks) | Single coroutine (hooks active) |
| ----------------------------- | ---------- | --------------------------- | ------------------------------- |
| C++ heap allocations          | 12         | 0 (free-list hit)           | 0 (free-list hit)               |
| V8 heap objects               | 28         | 2                           | 3                               |
| Total allocations             | 40         | 2                           | 3                               |
| InternalCallbackScope entries | 4          | 5 (one per segment)         | 5                               |

The coroutine frame embeds the `uv_fs_t` (\~440 bytes) directly. The compiler
may overlay non-simultaneously-live awaitables in the frame, so a multi-step
coroutine does not necessarily pay N times the `uv_fs_t` cost.

## Known limitations

* **Heap snapshot visibility**: The coroutine frame is not visible to V8 heap
  snapshots or `MemoryRetainer`. The thread-local free-list allocator reduces
  malloc pressure but does not provide V8 with per-frame memory accounting.
  The exact frame contents are not inspectable from heap snapshot tooling.

* **Snapshot serialization**: `UvTrackedTask` holds `v8::Global` handles that
  cannot be serialized into a startup snapshot. There is currently no safety
  check to prevent snapshotting while coroutines are active. In practice this
  is not a problem because snapshots are taken at startup before I/O begins.

* **Free-list retention**: The thread-local free-list retains up to 32 frames
  per size class bucket after a burst of concurrent coroutines. These frames
  are held until reused or the thread exits. The bound is configurable via
  `kMaxCachedPerBucket`.
