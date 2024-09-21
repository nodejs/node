#include "embedtest_node_api.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };\n"
    "require('vm').runInThisContext(process.argv[1]);";

void CallMe(node_embedding_runtime runtime, napi_env env);
void WaitMe(node_embedding_runtime runtime, napi_env env);
void WaitMeWithCheese(node_embedding_runtime runtime, napi_env env);

extern "C" int32_t test_main_node_api(int32_t argc, char* argv[]) {
  node_embedding_on_error(HandleTestError, argv[0]);

  CHECK_EXIT_CODE(RunMain(
      argc,
      argv,
      [&](node_embedding_platform_config platform_config) {
        CHECK_EXIT_CODE(node_embedding_platform_set_flags(
            platform_config, node_embedding_platform_disable_node_options_env));
        return node_embedding_exit_code_ok;
      },
      [&](node_embedding_platform platform,
          node_embedding_runtime_config runtime_config) {
        CHECK_EXIT_CODE(node_embedding_runtime_on_start_execution(
            runtime_config,
            [](void* cb_data,
               node_embedding_runtime runtime,
               napi_env env,
               napi_value process,
               napi_value require,
               napi_value run_cjs) -> napi_value {
              napi_status status{};
              napi_value script, undefined, result;
              NODE_API_CALL(napi_create_string_utf8(
                  env, main_script, NAPI_AUTO_LENGTH, &script));
              NODE_API_CALL(napi_get_undefined(env, &undefined));
              NODE_API_CALL(napi_call_function(
                  env, undefined, run_cjs, 1, &script, &result));
              return result;
            },
            nullptr));
        return node_embedding_exit_code_ok;
      },
      [&](node_embedding_runtime runtime, napi_env env) {
        CallMe(runtime, env);
        WaitMe(runtime, env);
        WaitMeWithCheese(runtime, env);
      }));

  return node_embedding_exit_code_ok;
}

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value) {
  size_t str_size = 0;
  napi_status status =
      napi_get_value_string_utf8(env, value, nullptr, 0, &str_size);
  if (status != napi_ok) {
    return status;
  }
  size_t offset = str.size();
  str.resize(offset + str_size);
  status = napi_get_value_string_utf8(
      env, value, &str[0] + offset, str_size + 1, &str_size);
  return status;
}

void GetAndThrowLastErrorMessage(napi_env env) {
  const napi_extended_error_info* error_info;
  napi_get_last_error_info(env, &error_info);
  bool is_pending;
  const char* err_message = error_info->error_message;
  napi_is_exception_pending((env), &is_pending);
  /* If an exception is already pending, don't rethrow it */
  if (!is_pending) {
    const char* error_message =
        err_message != nullptr ? err_message : "empty error message";
    napi_throw_error((env), nullptr, error_message);
  }
}

void ThrowLastErrorMessage(napi_env env, const char* message) {
  bool is_pending;
  napi_is_exception_pending(env, &is_pending);
  /* If an exception is already pending, don't rethrow it */
  if (!is_pending) {
    const char* error_message =
        message != nullptr ? message : "empty error message";
    napi_throw_error(env, nullptr, error_message);
  }
}

std::string FormatString(const char* format, ...) {
  va_list args1;
  va_start(args1, format);
  va_list args2;
  va_copy(args2, args1);  // Required for some compilers like GCC.
  std::string result(std::vsnprintf(nullptr, 0, format, args1), '\0');
  va_end(args1);
  std::vsnprintf(&result[0], result.size() + 1, format, args2);
  va_end(args2);
  return result;
}

void CallMe(node_embedding_runtime runtime, napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_create_string_utf8(env, "callMe", NAPI_AUTO_LENGTH, &key));
  NODE_API_CALL_RETURN_VOID(napi_get_property(env, global, key, &cb));

  napi_valuetype cb_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, cb, &cb_type));

  // Only evaluate callMe if it was registered as a function.
  if (cb_type == napi_function) {
    napi_value undef;
    NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undef));
    napi_value arg;
    NODE_API_CALL_RETURN_VOID(
        napi_create_string_utf8(env, "called", NAPI_AUTO_LENGTH, &arg));
    napi_value result;
    NODE_API_CALL_RETURN_VOID(
        napi_call_function(env, undef, cb, 1, &arg, &result));

    char buf[32];
    size_t len;
    NODE_API_CALL_RETURN_VOID(
        napi_get_value_string_utf8(env, result, buf, 32, &len));
    if (strcmp(buf, "called you") != 0) {
      NODE_API_FAIL_RETURN_VOID("Invalid value received: %s\n", buf);
    }
    printf("%s", buf);
  } else if (cb_type != napi_undefined) {
    NODE_API_FAIL_RETURN_VOID("Invalid callMe value\n");
  }
}

char callback_buf[32];
size_t callback_buf_len;
napi_value c_cb(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;
  NODE_API_CALL(napi_get_cb_info(env, info, &argc, &arg, nullptr, nullptr));
  NODE_API_CALL(napi_get_value_string_utf8(
      env, arg, callback_buf, 32, &callback_buf_len));
  return nullptr;
}

void WaitMe(node_embedding_runtime runtime, napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      napi_create_string_utf8(env, "waitMe", NAPI_AUTO_LENGTH, &key));
  NODE_API_CALL_RETURN_VOID(napi_get_property(env, global, key, &cb));

  napi_valuetype cb_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, cb, &cb_type));

  // Only evaluate waitMe if it was registered as a function.
  if (cb_type == napi_function) {
    napi_value undef;
    NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undef));
    napi_value args[2];
    NODE_API_CALL_RETURN_VOID(
        napi_create_string_utf8(env, "waited", NAPI_AUTO_LENGTH, &args[0]));
    NODE_API_CALL_RETURN_VOID(napi_create_function(
        env, "wait_cb", strlen("wait_cb"), c_cb, NULL, &args[1]));

    napi_value result;
    memset(callback_buf, 0, 32);
    NODE_API_CALL_RETURN_VOID(
        napi_call_function(env, undef, cb, 2, args, &result));
    if (strcmp(callback_buf, "waited you") == 0) {
      NODE_API_FAIL_RETURN_VOID("Anachronism detected: %s\n", callback_buf);
    }

    node_embedding_run_event_loop(
        runtime, node_embedding_event_loop_run_default, nullptr);

    if (strcmp(callback_buf, "waited you") != 0) {
      NODE_API_FAIL_RETURN_VOID("Invalid value received: %s\n", callback_buf);
    }
    printf("%s", callback_buf);
  } else if (cb_type != napi_undefined) {
    NODE_API_FAIL_RETURN_VOID("Invalid waitMe value\n");
  }
}

void WaitMeWithCheese(node_embedding_runtime runtime, napi_env env) {
  enum class PromiseState {
    kPending,
    kFulfilled,
    kRejected,
  };

  PromiseState promise_state = PromiseState::kPending;
  napi_value global{}, wait_promise{}, undefined{};
  napi_value on_fulfilled{}, on_rejected{};
  napi_value then_args[2] = {};
  const char* expected{};

  NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));

  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, global, "waitPromise", &wait_promise));

  napi_valuetype wait_promise_type;
  NODE_API_CALL_RETURN_VOID(napi_typeof(env, wait_promise, &wait_promise_type));

  // Only evaluate waitPromise if it was registered as a function.
  if (wait_promise_type == napi_undefined) {
    return;
  } else if (wait_promise_type != napi_function) {
    NODE_API_FAIL_RETURN_VOID("Invalid waitPromise value\n");
  }

  napi_value arg;
  NODE_API_CALL_RETURN_VOID(
      napi_create_string_utf8(env, "waited", NAPI_AUTO_LENGTH, &arg));

  memset(callback_buf, 0, 32);
  napi_value promise;
  NODE_API_CALL_RETURN_VOID(
      napi_call_function(env, undefined, wait_promise, 1, &arg, &promise));

  if (strcmp(callback_buf, "waited with cheese") == 0) {
    NODE_API_FAIL_RETURN_VOID("Anachronism detected: %s\n", callback_buf);
  }

  bool is_promise;
  NODE_API_CALL_RETURN_VOID(napi_is_promise(env, promise, &is_promise));
  if (!is_promise) {
    NODE_API_FAIL_RETURN_VOID("Result is not a Promise\n");
  }

  NODE_API_CALL_RETURN_VOID(napi_create_function(
      env,
      "onFulfilled",
      NAPI_AUTO_LENGTH,
      [](napi_env env, napi_callback_info info) -> napi_value {
        size_t argc = 1;
        napi_value result;
        void* data;
        napi_get_cb_info(env, info, &argc, &result, nullptr, &data);
        napi_get_value_string_utf8(
            env, result, callback_buf, 32, &callback_buf_len);
        *static_cast<PromiseState*>(data) = PromiseState::kFulfilled;
        return nullptr;
      },
      &promise_state,
      &on_fulfilled));
  NODE_API_CALL_RETURN_VOID(napi_create_function(
      env,
      "rejected",
      NAPI_AUTO_LENGTH,
      [](napi_env env, napi_callback_info info) -> napi_value {
        size_t argc = 1;
        napi_value result;
        void* data;
        napi_get_cb_info(env, info, &argc, &result, nullptr, &data);
        napi_get_value_string_utf8(
            env, result, callback_buf, 32, &callback_buf_len);
        *static_cast<PromiseState*>(data) = PromiseState::kRejected;
        return nullptr;
      },
      &promise_state,
      &on_rejected));
  napi_value then;
  NODE_API_CALL_RETURN_VOID(
      napi_get_named_property(env, promise, "then", &then));
  then_args[0] = on_fulfilled;
  then_args[1] = on_rejected;
  NODE_API_CALL_RETURN_VOID(
      napi_call_function(env, promise, then, 2, then_args, nullptr));

  while (promise_state == PromiseState::kPending) {
    node_embedding_run_event_loop(
        runtime, node_embedding_event_loop_run_nowait, nullptr);
  }

  expected = (promise_state == PromiseState::kFulfilled)
                 ? "waited with cheese"
                 : "waited without cheese";

  if (strcmp(callback_buf, expected) != 0) {
    NODE_API_FAIL_RETURN_VOID("Invalid value received: %s\n", callback_buf);
  }
  printf("%s", callback_buf);
}
