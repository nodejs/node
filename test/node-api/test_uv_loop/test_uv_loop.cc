#include "../../js-native-api/common.h"

#include <node_api.h>
#include <uv.h>

#include <cassert>
#include <memory>
#include <utility>

template <typename T>
void* SetImmediate(node_api_env env, T&& cb) {
  T* ptr = new T(std::move(cb));
  uv_loop_t* loop = nullptr;
  uv_check_t* check = new uv_check_t;
  check->data = ptr;
  NODE_API_ASSERT(env,
                  node_api_get_uv_event_loop(env, &loop) == node_api_ok,
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

node_api_value
SetImmediateBinding(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value argv[1];
  node_api_value _this;
  void* data;
  NODE_API_CALL(env,
    node_api_get_cb_info(env, info, &argc, argv, &_this, &data));
  NODE_API_ASSERT(env, argc >= 1, "Not enough arguments, expected 1.");

  node_api_valuetype t;
  NODE_API_CALL(env, node_api_typeof(env, argv[0], &t));
  NODE_API_ASSERT(env, t == node_api_function,
      "Wrong first argument, function expected.");

  node_api_ref cbref;
  NODE_API_CALL(env,
    node_api_create_reference(env, argv[0], 1, &cbref));

  SetImmediate(env, [=]() -> char* {
    node_api_value undefined;
    node_api_value callback;
    node_api_handle_scope scope;
    NODE_API_CALL(env, node_api_open_handle_scope(env, &scope));
    NODE_API_CALL(env, node_api_get_undefined(env, &undefined));
    NODE_API_CALL(env, node_api_get_reference_value(env, cbref, &callback));
    NODE_API_CALL(env, node_api_delete_reference(env, cbref));
    NODE_API_CALL(env,
        node_api_call_function(env, undefined, callback, 0, nullptr, nullptr));
    NODE_API_CALL(env, node_api_close_handle_scope(env, scope));
    return &dummy;
  });

  return nullptr;
}

node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("SetImmediate", SetImmediateBinding)
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
