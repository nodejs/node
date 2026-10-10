#include <node_api.h>
#include <stdint.h>
#include <stdlib.h>
#include <uv.h>

typedef struct {
  uv_timer_t timer;
  napi_env env;
  int64_t n;
  napi_ref fn;
  napi_ref done;
} State;

static void OnClose(uv_handle_t* handle) {
  free(handle->data);
}

// napi_make_callback without async context, n times from a libuv timer, then
// done: every call opens a top-level callback scope, like an I/O callback.
static void OnTimer(uv_timer_t* handle) {
  State* state = (State*) handle->data;
  napi_env env = state->env;
  napi_handle_scope scope;
  napi_value fn, done, recv;
  napi_open_handle_scope(env, &scope);
  napi_get_reference_value(env, state->fn, &fn);
  napi_get_reference_value(env, state->done, &done);
  napi_get_global(env, &recv);
  for (int64_t i = 0; i < state->n; i++) {
    napi_handle_scope inner;
    napi_open_handle_scope(env, &inner);
    napi_make_callback(env, NULL, recv, fn, 0, NULL, NULL);
    napi_close_handle_scope(env, inner);
  }
  napi_make_callback(env, NULL, recv, done, 0, NULL, NULL);
  napi_delete_reference(env, state->fn);
  napi_delete_reference(env, state->done);
  napi_close_handle_scope(env, scope);
  uv_close((uv_handle_t*) &state->timer, OnClose);
}

// run(n, fn, done)
static napi_value Run(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  State* state = (State*) calloc(1, sizeof(State));
  state->env = env;
  napi_get_value_int64(env, argv[0], &state->n);
  napi_create_reference(env, argv[1], 1, &state->fn);
  napi_create_reference(env, argv[2], 1, &state->done);
  uv_loop_t* loop;
  napi_get_uv_event_loop(env, &loop);
  state->timer.data = state;
  uv_timer_init(loop, &state->timer);
  uv_timer_start(&state->timer, OnTimer, 0, 0);
  return NULL;
}

NAPI_MODULE_INIT() {
  napi_value run;
  napi_create_function(env, "run", NAPI_AUTO_LENGTH, Run, NULL, &run);
  napi_set_named_property(env, exports, "run", run);
  return exports;
}
