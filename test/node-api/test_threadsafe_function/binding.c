// For the purpose of this test we use libuv's threading library. When deciding
// on a threading library for a new project it bears remembering that in the
// future libuv may introduce API changes which may render it non-ABI-stable,
// which, in turn, may affect the ABI stability of the project despite its use
// of N-API.
#include <uv.h>
#include <node_api.h>
#include "../../js-native-api/common.h"

#define ARRAY_LENGTH 10
#define MAX_QUEUE_SIZE 2

static uv_thread_t uv_threads[2];
static napi_threadsafe_function ts_fn;

typedef struct {
  napi_threadsafe_function_call_mode block_on_full;
  napi_threadsafe_function_release_mode abort;
  bool start_secondary;
  napi_ref js_finalize_cb;
  uint32_t max_queue_size;
} ts_fn_hint;

static ts_fn_hint ts_info;

// Thread data to transmit to JS
static int ints[ARRAY_LENGTH];

static void secondary_thread(void* data) {
  napi_threadsafe_function ts_fn = data;

  if (napi_release_threadsafe_function(ts_fn, napi_tsfn_release) != napi_ok) {
    napi_fatal_error("secondary_thread", NAPI_AUTO_LENGTH,
        "napi_release_threadsafe_function failed", NAPI_AUTO_LENGTH);
  }
}

// Source thread producing the data
static void data_source_thread(void* data) {
  napi_threadsafe_function ts_fn = data;
  int index;
  void* hint;
  ts_fn_hint *ts_fn_info;
  napi_status status;
  bool queue_was_full = false;
  bool queue_was_closing = false;

  if (napi_get_threadsafe_function_context(ts_fn, &hint) != napi_ok) {
    napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
        "napi_get_threadsafe_function_context failed", NAPI_AUTO_LENGTH);
  }

  ts_fn_info = (ts_fn_hint *)hint;

  if (ts_fn_info != &ts_info) {
    napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
      "thread-safe function hint is not as expected", NAPI_AUTO_LENGTH);
  }

  if (ts_fn_info->start_secondary) {
    if (napi_acquire_threadsafe_function(ts_fn) != napi_ok) {
      napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
        "napi_acquire_threadsafe_function failed", NAPI_AUTO_LENGTH);
    }

    if (uv_thread_create(&uv_threads[1], secondary_thread, ts_fn) != 0) {
      napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
        "failed to start secondary thread", NAPI_AUTO_LENGTH);
    }
  }

  for (index = ARRAY_LENGTH - 1; index > -1 && !queue_was_closing; index--) {
    status = napi_call_threadsafe_function(ts_fn, &ints[index],
        ts_fn_info->block_on_full);
    if (ts_fn_info->max_queue_size == 0) {
      // Let's make this thread really busy for 200 ms to give the main thread a
      // chance to abort.
      uint64_t start = uv_hrtime();
      for (; uv_hrtime() - start < 200000000;);
    }
    switch (status) {
      case napi_queue_full:
        queue_was_full = true;
        index++;
        // fall through

      case napi_ok:
        continue;

      case napi_closing:
        queue_was_closing = true;
        break;

      default:
        napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
            "napi_call_threadsafe_function failed", NAPI_AUTO_LENGTH);
    }
  }

  // Assert that the enqueuing of a value was refused at least once, if this is
  // a non-blocking test run.
  if (!ts_fn_info->block_on_full && !queue_was_full) {
    napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
        "queue was never full", NAPI_AUTO_LENGTH);
  }

  // Assert that the queue was marked as closing at least once, if this is an
  // aborting test run.
  if (ts_fn_info->abort == napi_tsfn_abort && !queue_was_closing) {
    napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
      "queue was never closing", NAPI_AUTO_LENGTH);
  }

  if (!queue_was_closing &&
      napi_release_threadsafe_function(ts_fn, napi_tsfn_release) != napi_ok) {
    napi_fatal_error("data_source_thread", NAPI_AUTO_LENGTH,
        "napi_release_threadsafe_function failed", NAPI_AUTO_LENGTH);
  }
}

// Getting the data into JS
static void call_js(napi_env env, napi_value cb, void* hint, void* data) {
  if (!(env == NULL || cb == NULL)) {
    napi_value argv, undefined;
    NAPI_CALL_RETURN_VOID(env, napi_create_int32(env, *(int*)data, &argv));
    NAPI_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined));
    NAPI_CALL_RETURN_VOID(env, napi_call_function(env, undefined, cb, 1, &argv,
        NULL));
  }
}

static napi_ref alt_ref;
// Getting the data into JS with the alternative referece
static void call_ref(napi_env env, napi_value _, void* hint, void* data) {
  if (!(env == NULL || alt_ref == NULL)) {
    napi_value fn, argv, undefined;
    NAPI_CALL_RETURN_VOID(env, napi_get_reference_value(env, alt_ref, &fn));
    NAPI_CALL_RETURN_VOID(env, napi_create_int32(env, *(int*)data, &argv));
    NAPI_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined));
    NAPI_CALL_RETURN_VOID(env, napi_call_function(env, undefined, fn, 1, &argv,
        NULL));
  }
}

// Cleanup
static napi_value StopThread(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  napi_valuetype value_type;
  NAPI_CALL(env, napi_typeof(env, argv[0], &value_type));
  NAPI_ASSERT(env, value_type == napi_function,
      "StopThread argument is a function");
  NAPI_ASSERT(env, (ts_fn != NULL), "Existing threadsafe function");
  NAPI_CALL(env,
      napi_create_reference(env, argv[0], 1, &(ts_info.js_finalize_cb)));
  bool abort;
  NAPI_CALL(env, napi_get_value_bool(env, argv[1], &abort));
  NAPI_CALL(env,
      napi_release_threadsafe_function(ts_fn,
          abort ? napi_tsfn_abort : napi_tsfn_release));
  ts_fn = NULL;
  return NULL;
}

// Join the thread and inform JS that we're done.
static void join_the_threads(napi_env env, void *data, void *hint) {
  uv_thread_t *the_threads = data;
  ts_fn_hint *the_hint = hint;
  napi_value js_cb, undefined;

  uv_thread_join(&the_threads[0]);
  if (the_hint->start_secondary) {
    uv_thread_join(&the_threads[1]);
  }

  NAPI_CALL_RETURN_VOID(env,
      napi_get_reference_value(env, the_hint->js_finalize_cb, &js_cb));
  NAPI_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined));
  NAPI_CALL_RETURN_VOID(env,
      napi_call_function(env, undefined, js_cb, 0, NULL, NULL));
  NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env,
      the_hint->js_finalize_cb));
  if (alt_ref != NULL) {
    NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, alt_ref));
    alt_ref = NULL;
  }
}

static napi_value StartThreadInternal(napi_env env,
                                      napi_callback_info info,
                                      napi_threadsafe_function_call_js cb,
                                      bool block_on_full,
                                      bool alt_ref_js_cb) {

  size_t argc = 4;
  napi_value argv[4];

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  if (alt_ref_js_cb) {
    NAPI_CALL(env, napi_create_reference(env, argv[0], 1, &alt_ref));
    argv[0] = NULL;
  }

  ts_info.block_on_full =
      (block_on_full ? napi_tsfn_blocking : napi_tsfn_nonblocking);

  NAPI_ASSERT(env, (ts_fn == NULL), "Existing thread-safe function");
  napi_value async_name;
  NAPI_CALL(env, napi_create_string_utf8(env, "N-API Thread-safe Function Test",
      NAPI_AUTO_LENGTH, &async_name));
  NAPI_CALL(env, napi_get_value_uint32(env, argv[3], &ts_info.max_queue_size));
  NAPI_CALL(env, napi_create_threadsafe_function(env,
                                                 argv[0],
                                                 NULL,
                                                 async_name,
                                                 ts_info.max_queue_size,
                                                 2,
                                                 uv_threads,
                                                 join_the_threads,
                                                 &ts_info,
                                                 cb,
                                                 &ts_fn));
  bool abort;
  NAPI_CALL(env, napi_get_value_bool(env, argv[1], &abort));
  ts_info.abort = abort ? napi_tsfn_abort : napi_tsfn_release;
  NAPI_CALL(env, napi_get_value_bool(env, argv[2], &(ts_info.start_secondary)));

  NAPI_ASSERT(env,
      (uv_thread_create(&uv_threads[0], data_source_thread, ts_fn) == 0),
      "Thread creation");

  return NULL;
}

static napi_value Unref(napi_env env, napi_callback_info info) {
  NAPI_ASSERT(env, ts_fn != NULL, "No existing thread-safe function");
  NAPI_CALL(env, napi_unref_threadsafe_function(env, ts_fn));
  return NULL;
}

static napi_value Release(napi_env env, napi_callback_info info) {
  NAPI_ASSERT(env, ts_fn != NULL, "No existing thread-safe function");
  NAPI_CALL(env, napi_release_threadsafe_function(ts_fn, napi_tsfn_release));
  return NULL;
}

// Startup
static napi_value StartThread(napi_env env, napi_callback_info info) {
  return StartThreadInternal(env, info, call_js,
    /** block_on_full */true, /** alt_ref_js_cb */false);
}

static napi_value StartThreadNonblocking(napi_env env,
                                         napi_callback_info info) {
  return StartThreadInternal(env, info, call_js,
    /** block_on_full */false, /** alt_ref_js_cb */false);
}

static napi_value StartThreadNoNative(napi_env env, napi_callback_info info) {
  return StartThreadInternal(env, info, NULL,
    /** block_on_full */true, /** alt_ref_js_cb */false);
}

static napi_value StartThreadNoJsFunc(napi_env env, napi_callback_info info) {
  return StartThreadInternal(env, info, call_ref,
    /** block_on_full */true, /** alt_ref_js_cb */true);
}

static void DeadlockTestDummyMarshaller(napi_env env,
                                        napi_value empty0,
                                        void* empty1,
                                        void* empty2) {}

static napi_value TestDeadlock(napi_env env, napi_callback_info info) {
  napi_threadsafe_function tsfn;
  napi_status status;
  napi_value async_name;
  napi_value return_value;

  // Create an object to store the returned information.
  NAPI_CALL(env, napi_create_object(env, &return_value));

  // Create a string to be used with the thread-safe function.
  NAPI_CALL(env, napi_create_string_utf8(env,
                                     "N-API Thread-safe Function Deadlock Test",
                                     NAPI_AUTO_LENGTH,
                                     &async_name));

  // Create the thread-safe function with a single queue slot and a single thread.
  NAPI_CALL(env, napi_create_threadsafe_function(env,
                                                 NULL,
                                                 NULL,
                                                 async_name,
                                                 1,
                                                 1,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 DeadlockTestDummyMarshaller,
                                                 &tsfn));

  // Call the threadsafe function. This should succeed and fill the queue.
  NAPI_CALL(env, napi_call_threadsafe_function(tsfn, NULL, napi_tsfn_blocking));

  // Call the threadsafe function. This should not block, but return
  // `napi_would_deadlock`. We save the resulting status in an object to be
  // returned.
  status = napi_call_threadsafe_function(tsfn, NULL, napi_tsfn_blocking);
  add_returned_status(env,
                      "deadlockTest",
                      return_value,
                      "Main thread would deadlock",
                      napi_would_deadlock,
                      status);

  // Clean up the thread-safe function before returning.
  NAPI_CALL(env, napi_release_threadsafe_function(tsfn, napi_tsfn_release));

  // Return the result.
  return return_value;
}

// Module init
static napi_value Init(napi_env env, napi_value exports) {
  size_t index;
  for (index = 0; index < ARRAY_LENGTH; index++) {
    ints[index] = index;
  }
  napi_value js_array_length, js_max_queue_size;
  napi_create_uint32(env, ARRAY_LENGTH, &js_array_length);
  napi_create_uint32(env, MAX_QUEUE_SIZE, &js_max_queue_size);

  napi_property_descriptor properties[] = {
    {
      "ARRAY_LENGTH",
      NULL,
      NULL,
      NULL,
      NULL,
      js_array_length,
      napi_enumerable,
      NULL
    },
    {
      "MAX_QUEUE_SIZE",
      NULL,
      NULL,
      NULL,
      NULL,
      js_max_queue_size,
      napi_enumerable,
      NULL
    },
    DECLARE_NAPI_PROPERTY("StartThread", StartThread),
    DECLARE_NAPI_PROPERTY("StartThreadNoNative", StartThreadNoNative),
    DECLARE_NAPI_PROPERTY("StartThreadNonblocking", StartThreadNonblocking),
    DECLARE_NAPI_PROPERTY("StartThreadNoJsFunc", StartThreadNoJsFunc),
    DECLARE_NAPI_PROPERTY("StopThread", StopThread),
    DECLARE_NAPI_PROPERTY("Unref", Unref),
    DECLARE_NAPI_PROPERTY("Release", Release),
    DECLARE_NAPI_PROPERTY("TestDeadlock", TestDeadlock),
  };

  NAPI_CALL(env, napi_define_properties(env, exports,
    sizeof(properties)/sizeof(properties[0]), properties));

  return exports;
}
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
