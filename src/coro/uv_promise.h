#ifndef SRC_CORO_UV_PROMISE_H_
#define SRC_CORO_UV_PROMISE_H_

#include "env-inl.h"
#include "node_errors.h"
#include "v8.h"

namespace node {
namespace coro {

// ---------------------------------------------------------------------------
// ResolvePromise / RejectPromise — helpers for resolving/rejecting a
// v8::Global<v8::Promise::Resolver> from inside a coroutine.
//
// When used inside a UvTrackedTask, the InternalCallbackScope from
// on_resume() already provides a HandleScope and will drain microtasks
// and nextTick on close (in on_suspend / final_suspend).
//
// These guard against calling V8 APIs when the environment is shutting
// down (can_call_into_js() is false).
// ---------------------------------------------------------------------------

inline void ResolvePromise(Environment* env,
                           v8::Global<v8::Promise::Resolver>& resolver,
                           v8::Local<v8::Value> value) {
  if (!env->can_call_into_js()) return;
  v8::HandleScope scope(env->isolate());
  auto local = resolver.Get(env->isolate());
  USE(local->Resolve(env->context(), value));
}

inline void ResolvePromiseUndefined(
    Environment* env, v8::Global<v8::Promise::Resolver>& resolver) {
  ResolvePromise(env, resolver, v8::Undefined(env->isolate()));
}

inline void RejectPromiseWithUVError(
    Environment* env,
    v8::Global<v8::Promise::Resolver>& resolver,
    int uv_err,
    const char* syscall,
    const char* path = nullptr) {
  if (!env->can_call_into_js()) return;
  v8::HandleScope scope(env->isolate());
  auto local = resolver.Get(env->isolate());
  v8::Local<v8::Value> exception =
      UVException(env->isolate(), uv_err, syscall, nullptr, path);
  USE(local->Reject(env->context(), exception));
}

// ---------------------------------------------------------------------------
// MakePromise — Creates a v8::Promise::Resolver, sets the return value
// on `args`, and returns a Global handle for use in the coroutine.
// ---------------------------------------------------------------------------

inline v8::Global<v8::Promise::Resolver> MakePromise(
    Environment* env, const v8::FunctionCallbackInfo<v8::Value>& args) {
  auto resolver = v8::Promise::Resolver::New(env->context()).ToLocalChecked();
  args.GetReturnValue().Set(resolver->GetPromise());
  return v8::Global<v8::Promise::Resolver>(env->isolate(), resolver);
}

// ---------------------------------------------------------------------------
// CoroDispatch — Creates a Promise, constructs the coroutine task by
// calling `impl(env, resolver, extra_args...)`, initializes tracking,
// and starts the coroutine in fire-and-forget mode.
//
// Usage:
//   CoroDispatch(env, args, CoroAccessImpl,
//                std::string(*path, path.length()), mode);
// ---------------------------------------------------------------------------

template <typename Impl, typename... Args>
void CoroDispatch(Environment* env,
                  const v8::FunctionCallbackInfo<v8::Value>& args,
                  Impl&& impl,
                  Args&&... extra_args) {
  auto resolver = MakePromise(env, args);
  auto task = impl(env, std::move(resolver), std::forward<Args>(extra_args)...);
  task.InitTracking(env);
  task.Start();
}

}  // namespace coro
}  // namespace node

#endif  // SRC_CORO_UV_PROMISE_H_
