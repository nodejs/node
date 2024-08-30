#include "embedtest_node_api.h"

#define NAPI_EXPERIMENTAL
#include <node_api_embedding.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Note: This file is being referred to from doc/api/embedding.md, and excerpts
// from it are included in the documentation. Try to keep these in sync.

static int32_t RunNodeInstance();

const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "globalThis.embedVars = { nön_ascıı: '🏳️‍🌈' };\n"
    "require('vm').runInThisContext(process.argv[1]);";

static const char* exe_name;

static void NAPI_CDECL get_errors(void* data,
                                  size_t count,
                                  const char* errors[]) {
  for (size_t i = 0; i < count && i < 30; ++i) {
    fprintf(stderr, "%s: %s\n", exe_name, errors[i]);
  }
}

int32_t test_main_node_api(int32_t argc, char* argv[]) {
  exe_name = argv[0];
  bool early_return = false;
  int32_t exit_code = 0;
  node_api_init_once_per_process(argc,
                                 argv,
                                 node_api_platform_no_flags,
                                 get_errors,
                                 NULL,
                                 &early_return,
                                 &exit_code);
  if (early_return) {
    return exit_code;
  }

  CHECK_EXIT_CODE(RunNodeInstance());

  CHECK(node_api_uninit_once_per_process());
  return 0;
}

int32_t callMe(napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  CHECK(node_api_open_env_scope(env));

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

  CHECK(node_api_close_env_scope(env));
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

int32_t waitMe(napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  CHECK(node_api_open_env_scope(env));

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

    CHECK(node_api_run_env(env));

    if (strcmp(callback_buf, "waited you") != 0) {
      FAIL("Invalid value received: %s\n", callback_buf);
    }
    printf("%s", callback_buf);
  } else if (cb_type != napi_undefined) {
    FAIL("Invalid waitMe value\n");
  }

  CHECK(node_api_close_env_scope(env));
  return 0;
}

int32_t waitMeWithCheese(napi_env env) {
  napi_value global;
  napi_value cb;
  napi_value key;

  CHECK(node_api_open_env_scope(env));

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

    napi_status r = node_api_await_promise(env, promise, &result);
    if (r != napi_ok && r != napi_pending_exception) {
      FAIL("Failed awaiting promise: %d\n", r);
    }

    const char* expected;
    if (r == napi_ok)
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

  CHECK(node_api_close_env_scope(env));
  return 0;
}

int32_t RunNodeInstance() {
  node_api_env_options options;
  CHECK(node_api_create_env_options(&options));
  napi_env env;
  CHECK(node_api_create_env(
      options, NULL, NULL, main_script, NAPI_VERSION, &env));
  CHECK(node_api_delete_env_options(options));

  CHECK_EXIT_CODE(callMe(env));
  CHECK_EXIT_CODE(waitMe(env));
  CHECK_EXIT_CODE(waitMeWithCheese(env));

  int32_t exit_code;
  CHECK(node_api_delete_env(env, &exit_code));

  return exit_code;
}
