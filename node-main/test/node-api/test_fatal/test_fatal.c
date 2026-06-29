// For the purpose of this test we use libuv's threading library. When deciding
// on a threading library for a new project it bears remembering that in the
// future libuv may introduce API changes which may render it non-ABI-stable,
// which, in turn, may affect the ABI stability of the project despite its use
// of N-API.
#include <uv.h>
#include <node_api.h>
#include "../../js-native-api/common.h"

static uv_thread_t uv_thread;

static void work_thread(void* data) {
  napi_fatal_error("work_thread", NAPI_AUTO_LENGTH,
      "foobar", NAPI_AUTO_LENGTH);
}

static napi_value Test(napi_env env, napi_callback_info info) {
  napi_fatal_error("test_fatal::Test", NAPI_AUTO_LENGTH,
                   "fatal message", NAPI_AUTO_LENGTH);
  return NULL;
}

static napi_value TestThread(napi_env env, napi_callback_info info) {
  NODE_API_ASSERT(env,
      (uv_thread_create(&uv_thread, work_thread, NULL) == 0),
      "Thread creation");
  return NULL;
}

static napi_value TestStringLength(napi_env env, napi_callback_info info) {
  napi_fatal_error("test_fatal::TestStringLength", 16, "fatal message", 13);
  return NULL;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test),
    DECLARE_NODE_API_PROPERTY("TestStringLength", TestStringLength),
    DECLARE_NODE_API_PROPERTY("TestThread", TestThread),
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
