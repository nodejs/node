#include <node_api.h>
#include <stdlib.h>
#include "../../js-native-api/common.h"

typedef struct {
  napi_threadsafe_function tsfn;
} Holder;

static void CallJs(napi_env env, napi_value js_cb, void* context, void* data) {}

// Runs while the napi_env tears down, reached from the thread-safe function
// dropping its own napi_env reference. Releasing the thread-safe function
// from here reenters TSFN finalization.
static void FinalizeHolder(napi_env env, void* data, void* hint) {
  Holder* holder = data;
  napi_status status =
      napi_release_threadsafe_function(holder->tsfn, napi_tsfn_abort);
  if (status != napi_ok) {
    abort();
  }
  free(holder);
}

NAPI_MODULE_INIT() {
  napi_value name;
  napi_value external;
  Holder* holder = malloc(sizeof(*holder));

  NODE_API_CALL(
      env,
      napi_create_string_utf8(env, "tsfn_teardown", NAPI_AUTO_LENGTH, &name));

  // The initial thread count is never released, so the thread-safe function is
  // still ref-ed by another thread when the environment tears down.
  NODE_API_CALL(env,
                napi_create_threadsafe_function(env,
                                                NULL,
                                                NULL,
                                                name,
                                                0,
                                                1,
                                                NULL,
                                                NULL,
                                                NULL,
                                                CallJs,
                                                &holder->tsfn));
  // Allow the worker uv_loop to exit.
  NODE_API_CALL(env, napi_unref_threadsafe_function(env, holder->tsfn));

  // Held by the module exports, which the module cache keeps alive, so the
  // napi_external finalizer runs during napi_env teardown.
  NODE_API_CALL(
      env, napi_create_external(env, holder, FinalizeHolder, NULL, &external));
  NODE_API_CALL(env, napi_set_named_property(env, exports, "holder", external));

  return exports;
}
