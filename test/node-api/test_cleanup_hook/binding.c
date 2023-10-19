#include <assert.h>
#include <stdlib.h>
#include "../../js-native-api/common.h"
#include "node_api.h"
#include "uv.h"

static int cleanup_hook_count = 0;
static void cleanup(void* arg) {
  cleanup_hook_count++;
  printf("cleanup(%d)\n", *(int*)(arg));
}

static int secret = 42;
static int wrong_secret = 17;

static void ObjectFinalizer(napi_env env, void* data, void* hint) {
  // cleanup is called once.
  assert(cleanup_hook_count == 1);

  napi_ref* ref = data;
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, *ref));
  free(ref);
}

static void CreateObjectWrap(napi_env env) {
  napi_value js_obj;
  napi_ref* ref = malloc(sizeof(*ref));
  NODE_API_CALL_RETURN_VOID(env, napi_create_object(env, &js_obj));
  NODE_API_CALL_RETURN_VOID(
      env, napi_wrap(env, js_obj, ref, ObjectFinalizer, NULL, ref));
  // create a strong reference so that the finalizer is called at shutdown.
  NODE_API_CALL_RETURN_VOID(env, napi_reference_ref(env, *ref, NULL));
}

static napi_value Init(napi_env env, napi_value exports) {
  // Create object wrap before cleanup hooks.
  CreateObjectWrap(env);

  napi_add_env_cleanup_hook(env, cleanup, &wrong_secret);
  napi_add_env_cleanup_hook(env, cleanup, &secret);
  napi_remove_env_cleanup_hook(env, cleanup, &wrong_secret);

  // Create object wrap after cleanup hooks.
  CreateObjectWrap(env);

  return NULL;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
