#include <node_api.h>
#include "../../js-native-api/common.h"

#define THREAD_COUNT 3

static void CallJs(napi_env env, napi_value cb, void* context, void* data) {
  NODE_API_BASIC_ASSERT_RETURN_VOID(false, "The queue stays empty");
}

static void Finalize(napi_env env, void* data, void* hint) {
  napi_ref callback_ref = data;
  napi_value callback;
  napi_value undefined;
  NODE_API_CALL_RETURN_VOID(
      env, napi_get_reference_value(env, callback_ref, &callback));
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, callback_ref));
  NODE_API_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(
      env, napi_call_function(env, undefined, callback, 0, NULL, NULL));
}

static napi_value Run(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  bool abort;
  napi_ref callback_ref;
  napi_threadsafe_function tsfn;
  NODE_API_CALL(env, napi_get_value_bool(env, args[0], &abort));
  NODE_API_CALL(env, napi_create_reference(env, args[1], 1, &callback_ref));
  napi_value name;
  NODE_API_CALL(env,
                napi_create_string_utf8(
                    env, "tsfn_thread_count", NAPI_AUTO_LENGTH, &name));
  NODE_API_CALL(env,
                napi_create_threadsafe_function(env,
                                                NULL,
                                                NULL,
                                                name,
                                                0,
                                                THREAD_COUNT,
                                                callback_ref,
                                                Finalize,
                                                NULL,
                                                CallJs,
                                                &tsfn));
  if (abort) {
    NODE_API_CALL(env, napi_release_threadsafe_function(tsfn, napi_tsfn_abort));
    for (int i = 1; i < THREAD_COUNT; i++) {
      napi_status status =
          napi_call_threadsafe_function(tsfn, NULL, napi_tsfn_nonblocking);
      NODE_API_ASSERT(
          env, status == napi_closing, "Call after abort returns napi_closing");
    }
  } else {
    for (int i = 0; i < THREAD_COUNT; i++) {
      NODE_API_CALL(env,
                    napi_release_threadsafe_function(tsfn, napi_tsfn_release));
    }
  }
  return NULL;
}

NAPI_MODULE_INIT() {
  napi_value run;
  NODE_API_CALL(
      env, napi_create_function(env, "run", NAPI_AUTO_LENGTH, Run, NULL, &run));
  return run;
}
