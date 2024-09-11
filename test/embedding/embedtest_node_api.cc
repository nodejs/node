#include "embedtest_node_api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };\n"
    "require('vm').runInThisContext(process.argv[1]);";

static int32_t RunNodeInstance(node_embedding_platform platform);

extern "C" int32_t test_main_node_api(int32_t argc, char* argv[]) {
  CHECK(node_embedding_on_error(HandleTestError, argv[0]));

  node_embedding_platform platform;
  CHECK(node_embedding_create_platform(NODE_EMBEDDING_VERSION, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  CHECK(node_embedding_platform_set_flags(
      platform, node_embedding_platform_disable_node_options_env));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  CHECK_EXIT_CODE(RunNodeInstance(platform));

  CHECK(node_embedding_delete_platform(platform));
  return 0;
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

int32_t callMe(napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  CHECK(napi_get_global(env, &global));
  CHECK(napi_create_string_utf8(env, "callMe", NAPI_AUTO_LENGTH, &key));
  CHECK(napi_get_property(env, global, key, &cb));

  napi_valuetype cb_type;
  CHECK(napi_typeof(env, cb, &cb_type));

  // Only evaluate callMe if it was registered as a function.
  if (cb_type == napi_function) {
    napi_value undef;
    CHECK(napi_get_undefined(env, &undef));
    napi_value arg;
    CHECK(napi_create_string_utf8(env, "called", NAPI_AUTO_LENGTH, &arg));
    napi_value result;
    CHECK(napi_call_function(env, undef, cb, 1, &arg, &result));

    char buf[32];
    size_t len;
    CHECK(napi_get_value_string_utf8(env, result, buf, 32, &len));
    if (strcmp(buf, "called you") != 0) {
      FAIL("Invalid value received: %s\n", buf);
    }
    printf("%s", buf);
  } else if (cb_type != napi_undefined) {
    FAIL("Invalid callMe value\n");
  }

  return 0;
}

char callback_buf[32];
size_t callback_buf_len;
napi_value c_cb(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;

  napi_get_cb_info(env, info, &argc, &arg, NULL, NULL);
  napi_get_value_string_utf8(env, arg, callback_buf, 32, &callback_buf_len);
  return NULL;
}

int32_t waitMe(napi_env env, node_embedding_runtime runtime) {
  napi_value global;
  napi_value cb;
  napi_value key;

  CHECK(napi_get_global(env, &global));
  CHECK(napi_create_string_utf8(env, "waitMe", NAPI_AUTO_LENGTH, &key));
  CHECK(napi_get_property(env, global, key, &cb));

  napi_valuetype cb_type;
  CHECK(napi_typeof(env, cb, &cb_type));

  // Only evaluate waitMe if it was registered as a function.
  if (cb_type == napi_function) {
    napi_value undef;
    CHECK(napi_get_undefined(env, &undef));
    napi_value args[2];
    CHECK(napi_create_string_utf8(env, "waited", NAPI_AUTO_LENGTH, &args[0]));
    CHECK(napi_create_function(
        env, "wait_cb", strlen("wait_cb"), c_cb, NULL, &args[1]));

    napi_value result;
    memset(callback_buf, 0, 32);
    CHECK(napi_call_function(env, undef, cb, 2, args, &result));
    if (strcmp(callback_buf, "waited you") == 0) {
      FAIL("Anachronism detected: %s\n", callback_buf);
    }

    CHECK(node_embedding_runtime_run_event_loop(runtime));

    if (strcmp(callback_buf, "waited you") != 0) {
      FAIL("Invalid value received: %s\n", callback_buf);
    }
    printf("%s", callback_buf);
  } else if (cb_type != napi_undefined) {
    FAIL("Invalid waitMe value\n");
  }

  return 0;
}

int32_t waitMeWithCheese(napi_env env, node_embedding_runtime runtime) {
  napi_value global;
  napi_value cb;
  napi_value key;

  CHECK(napi_get_global(env, &global));
  CHECK(napi_create_string_utf8(env, "waitPromise", NAPI_AUTO_LENGTH, &key));
  CHECK(napi_get_property(env, global, key, &cb));

  napi_valuetype cb_type;
  CHECK(napi_typeof(env, cb, &cb_type));

  // Only evaluate waitPromise if it was registered as a function.
  if (cb_type == napi_function) {
    napi_value undef;
    napi_get_undefined(env, &undef);
    napi_value arg;
    bool result_type;

    CHECK(napi_create_string_utf8(env, "waited", NAPI_AUTO_LENGTH, &arg));

    memset(callback_buf, 0, 32);
    napi_value promise;
    napi_value result;
    CHECK(napi_call_function(env, undef, cb, 1, &arg, &promise));

    if (strcmp(callback_buf, "waited with cheese") == 0) {
      FAIL("Anachronism detected: %s\n", callback_buf);
    }

    CHECK(napi_is_promise(env, promise, &result_type));

    if (!result_type) {
      FAIL("Result is not a Promise\n");
    }

    node_embedding_promise_state promise_state;
    CHECK(node_embedding_runtime_await_promise(
        runtime, promise, &promise_state, &result, nullptr));

    const char* expected;
    if (promise_state == node_embedding_promise_state_fulfilled)
      expected = "waited with cheese";
    else
      expected = "waited without cheese";

    CHECK(napi_get_value_string_utf8(
        env, result, callback_buf, 32, &callback_buf_len));
    if (strcmp(callback_buf, expected) != 0) {
      FAIL("Invalid value received: %s\n", callback_buf);
    }
    printf("%s", callback_buf);
  } else if (cb_type != napi_undefined) {
    FAIL("Invalid waitPromise value\n");
  }

  return 0;
}

int32_t RunNodeInstance(node_embedding_platform platform) {
  node_embedding_runtime runtime;
  CHECK(node_embedding_create_runtime(platform, &runtime));
  CHECK(node_embedding_runtime_set_node_api_version(runtime, NAPI_VERSION));
  CHECK(node_embedding_runtime_initialize(runtime, main_script));
  napi_env env;
  CHECK(node_embedding_runtime_get_node_api_env(runtime, &env));

  CHECK(node_embedding_runtime_open_scope(runtime));
  CHECK_EXIT_CODE(callMe(env));
  CHECK_EXIT_CODE(waitMe(env, runtime));
  CHECK_EXIT_CODE(waitMeWithCheese(env, runtime));
  CHECK(node_embedding_runtime_close_scope(runtime));

  CHECK(node_embedding_delete_runtime(runtime));

  return 0;
}
