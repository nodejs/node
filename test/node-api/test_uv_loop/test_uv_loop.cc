#include <node_api.h>
#include <uv.h>
#include <utility>
#include <memory>
#include <assert.h>
#include "../../js-native-api/common.h"

template <typename T>
void* SetImmediate(napi_env env, T&& cb) {
  T* ptr = new T(std::move(cb));
  uv_loop_t* loop = nullptr;
  uv_check_t* check = new uv_check_t;
  check->data = ptr;
  NAPI_ASSERT(env,
              napi_get_uv_event_loop(env, &loop) == napi_ok,
              "can get event loop");
  uv_check_init(loop, check);
  uv_check_start(check, [](uv_check_t* check) {
    std::unique_ptr<T> ptr {static_cast<T*>(check->data)};
    T cb = std::move(*ptr);
    uv_close(reinterpret_cast<uv_handle_t*>(check), [](uv_handle_t* handle) {
      delete reinterpret_cast<uv_check_t*>(handle);
    });

    assert(cb() != nullptr);
  });
  // Idle handle is needed only to stop the event loop from blocking in poll.
  uv_idle_t* idle = new uv_idle_t;
  uv_idle_init(loop, idle);
  uv_idle_start(idle, [](uv_idle_t* idle) {
    uv_close(reinterpret_cast<uv_handle_t*>(idle), [](uv_handle_t* handle) {
      delete reinterpret_cast<uv_check_t*>(handle);
    });
  });

  return nullptr;
}

static char dummy;

napi_value SetImmediateBinding(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value _this;
  void* data;
  NAPI_CALL(env,
    napi_get_cb_info(env, info, &argc, argv, &_this, &data));
  NAPI_ASSERT(env, argc >= 1, "Not enough arguments, expected 1.");

  napi_valuetype t;
  NAPI_CALL(env, napi_typeof(env, argv[0], &t));
  NAPI_ASSERT(env, t == napi_function,
      "Wrong first argument, function expected.");

  napi_ref cbref;
  NAPI_CALL(env,
    napi_create_reference(env, argv[0], 1, &cbref));

  SetImmediate(env, [=]() -> char* {
    napi_value undefined;
    napi_value callback;
    napi_handle_scope scope;
    NAPI_CALL(env, napi_open_handle_scope(env, &scope));
    NAPI_CALL(env, napi_get_undefined(env, &undefined));
    NAPI_CALL(env, napi_get_reference_value(env, cbref, &callback));
    NAPI_CALL(env, napi_delete_reference(env, cbref));
    NAPI_CALL(env,
        napi_call_function(env, undefined, callback, 0, nullptr, nullptr));
    NAPI_CALL(env, napi_close_handle_scope(env, scope));
    return &dummy;
  });

  return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    DECLARE_NAPI_PROPERTY("SetImmediate", SetImmediateBinding)
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
