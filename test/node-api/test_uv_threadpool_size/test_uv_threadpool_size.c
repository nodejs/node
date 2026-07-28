#undef NDEBUG
#include <assert.h>
#include <node_api.h>
#include <stdlib.h>
#include <uv.h>
#include "../../js-native-api/common.h"

typedef struct {
  uv_mutex_t mutex;
  uint32_t threadpool_size;
  uint32_t n_tasks_started;
  uint32_t n_tasks_exited;
  uint32_t n_tasks_finalized;
  bool observed_saturation;
} async_shared_data;

typedef struct {
  uint32_t task_id;
  async_shared_data* shared_data;
  napi_async_work request;
} async_carrier;

static inline bool all_tasks_started(async_shared_data* d) {
  assert(d->n_tasks_started <= d->threadpool_size + 1);
  return d->n_tasks_started == d->threadpool_size + 1;
}

static inline bool all_tasks_exited(async_shared_data* d) {
  assert(d->n_tasks_exited <= d->n_tasks_started);
  return all_tasks_started(d) && d->n_tasks_exited == d->n_tasks_started;
}

static inline bool all_tasks_finalized(async_shared_data* d) {
  assert(d->n_tasks_finalized <= d->n_tasks_exited);
  return all_tasks_exited(d) && d->n_tasks_finalized == d->n_tasks_exited;
}

static inline bool still_saturating(async_shared_data* d) {
  return d->n_tasks_started < d->threadpool_size;
}

static inline bool threadpool_saturated(async_shared_data* d) {
  return d->n_tasks_started == d->threadpool_size && d->n_tasks_exited == 0;
}

static inline bool threadpool_desaturating(async_shared_data* d) {
  return d->n_tasks_started >= d->threadpool_size && d->n_tasks_exited != 0;
}

static inline void print_info(const char* label, async_carrier* c) {
  async_shared_data* d = c->shared_data;
  printf("%s task_id=%u n_tasks_started=%u n_tasks_exited=%u "
         "n_tasks_finalized=%u observed_saturation=%d\n",
         label,
         c->task_id,
         d->n_tasks_started,
         d->n_tasks_exited,
         d->n_tasks_finalized,
         d->observed_saturation);
}

static void Execute(napi_env env, void* data) {
  async_carrier* c = (async_carrier*)data;
  async_shared_data* d = c->shared_data;

  // As long as fewer than threadpool_size async tasks have been started, more
  // should be started (eventually). Only once that happens should scheduled
  // async tasks remain queued.
  uv_mutex_lock(&d->mutex);
  bool should_be_concurrent = still_saturating(d);
  d->n_tasks_started++;
  assert(d->n_tasks_started <= d->threadpool_size + 1);

  print_info("start", c);

  if (should_be_concurrent) {
    // Wait for the thread pool to be saturated. This is not an elegant way of
    // doing so, but it really does not matter much here.
    while (still_saturating(d)) {
      print_info("waiting", c);
      uv_mutex_unlock(&d->mutex);
      uv_sleep(100);
      uv_mutex_lock(&d->mutex);
    }

    // One async task will observe that the threadpool is saturated, that is,
    // that threadpool_size tasks have been started and none have exited yet.
    // That task will be the first to exit.
    if (!d->observed_saturation) {
      assert(threadpool_saturated(d));
      d->observed_saturation = true;
    } else {
      assert(threadpool_saturated(d) || threadpool_desaturating(d));
    }
  } else {
    // If this task is not among the first threadpool_size tasks, it should not
    // have been started unless other tasks have already finished.
    assert(threadpool_desaturating(d));
  }

  print_info("exit", c);

  // Allow other tasks to access the shared data. If the thread pool is actually
  // larger than threadpool_size, this allows an extraneous task to start, which
  // will lead to an assertion error.
  uv_mutex_unlock(&d->mutex);
  uv_sleep(1000);
  uv_mutex_lock(&d->mutex);

  d->n_tasks_exited++;
  uv_mutex_unlock(&d->mutex);
}

static void Complete(napi_env env, napi_status status, void* data) {
  async_carrier* c = (async_carrier*)data;
  async_shared_data* d = c->shared_data;

  if (status != napi_ok) {
    napi_throw_type_error(env, NULL, "Execute callback failed.");
    return;
  }

  uv_mutex_lock(&d->mutex);
  assert(threadpool_desaturating(d));
  d->n_tasks_finalized++;
  print_info("finalize", c);
  if (all_tasks_finalized(d)) {
    uv_mutex_unlock(&d->mutex);
    uv_mutex_destroy(&d->mutex);
    free(d);
  } else {
    uv_mutex_unlock(&d->mutex);
  }

  NODE_API_CALL_RETURN_VOID(env, napi_delete_async_work(env, c->request));
  free(c);
}

static napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value this;
  void* data;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, &this, &data));
  NODE_API_ASSERT(env, argc >= 1, "Not enough arguments, expected 1.");

  async_shared_data* shared_data = calloc(1, sizeof(async_shared_data));
  assert(shared_data != NULL);
  int ret = uv_mutex_init(&shared_data->mutex);
  assert(ret == 0);

  napi_valuetype t;
  NODE_API_CALL(env, napi_typeof(env, argv[0], &t));
  NODE_API_ASSERT(
      env, t == napi_number, "Wrong first argument, integer expected.");
  NODE_API_CALL(
      env, napi_get_value_uint32(env, argv[0], &shared_data->threadpool_size));

  napi_value resource_name;
  NODE_API_CALL(env,
                napi_create_string_utf8(
                    env, "TestResource", NAPI_AUTO_LENGTH, &resource_name));

  for (uint32_t i = 0; i <= shared_data->threadpool_size; i++) {
    async_carrier* carrier = malloc(sizeof(async_carrier));
    assert(carrier != NULL);
    carrier->task_id = i;
    carrier->shared_data = shared_data;
    NODE_API_CALL(env,
                  napi_create_async_work(env,
                                         NULL,
                                         resource_name,
                                         Execute,
                                         Complete,
                                         carrier,
                                         &carrier->request));
    NODE_API_CALL(env, napi_queue_async_work(env, carrier->request));
  }

  return NULL;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc = DECLARE_NODE_API_PROPERTY("test", Test);
  NODE_API_CALL(env, napi_define_properties(env, exports, 1, &desc));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
