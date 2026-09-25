#include <js_native_api.h>
#include <node_api.h>
#include <node_api_types.h>

#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>

template <typename R, auto func, typename... Args>
inline auto call(const char* name, Args&&... args) -> R {
  napi_status status;
  if constexpr (std::is_same_v<R, void>) {
    status = func(std::forward<Args>(args)...);
    if (status == napi_ok) {
      return;
    }
  } else {
    R ret;
    status = func(std::forward<Args>(args)..., &ret);
    if (status == napi_ok) {
      return ret;
    }
  }
  std::fprintf(stderr, "%s: %d\n", name, status);
  std::abort();
}

#define NAPI_CALL(ret_type, func, ...)                                         \
  call<ret_type, func>(#func, ##__VA_ARGS__)

class Context {
 public:
  enum class State { kCreated, kCalled } state = State::kCreated;
};

void tsfn_callback(napi_env env,
                   napi_value js_cb,
                   void* ctx_p,
                   void* /* data */) {
  auto ctx = static_cast<Context*>(ctx_p);
  assert(ctx->state == Context::State::kCreated);
  ctx->state = Context::State::kCalled;
}

void tsfn_finalize(napi_env env,
                   void* /* finalize_data */,
                   void* finalize_hint) {
  auto ctx = static_cast<Context*>(finalize_hint);
  assert(ctx->state == Context::State::kCalled);
  delete ctx;
}

auto run(napi_env env, napi_callback_info info) -> napi_value {
  auto global = NAPI_CALL(napi_value, napi_get_global, env);
  auto undefined = NAPI_CALL(napi_value, napi_get_undefined, env);
  auto ctx = new Context();
  auto tsfn = NAPI_CALL(napi_threadsafe_function,
                        napi_create_threadsafe_function,
                        env,
                        nullptr,
                        global,
                        undefined,
                        0,
                        1 /* initial_thread_count */,
                        nullptr,
                        tsfn_finalize,
                        ctx,
                        tsfn_callback);

  NAPI_CALL(
      void, napi_call_threadsafe_function, tsfn, nullptr, napi_tsfn_blocking);

  NAPI_CALL(void, napi_unref_threadsafe_function, env, tsfn);

  NAPI_CALL(void,
            napi_release_threadsafe_function,
            tsfn,
            napi_threadsafe_function_release_mode::napi_tsfn_abort);
  return NAPI_CALL(napi_value, napi_get_undefined, env);
}

napi_value init(napi_env env, napi_value exports) {
  return NAPI_CALL(
      napi_value, napi_create_function, env, nullptr, 0, run, nullptr);
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
