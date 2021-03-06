#include "node_api.h"
#include "uv.h"
#include "../../js-native-api/common.h"

static void cleanup(void* arg) {
  printf("cleanup(%d)\n", *(int*)(arg));
}

static int secret = 42;
static int wrong_secret = 17;

static napi_value Init(napi_env env, napi_value exports) {
  napi_add_env_cleanup_hook(env, cleanup, &wrong_secret);
  napi_add_env_cleanup_hook(env, cleanup, &secret);
  napi_remove_env_cleanup_hook(env, cleanup, &wrong_secret);

  return NULL;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
