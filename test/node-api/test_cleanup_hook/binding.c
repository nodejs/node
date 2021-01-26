#include "node_api.h"
#include "uv.h"
#include "../../js-native-api/common.h"

static void cleanup(void* arg) {
  printf("cleanup(%d)\n", *(int*)(arg));
}

static int secret = 42;
static int wrong_secret = 17;

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_add_env_cleanup_hook(env, cleanup, &wrong_secret);
  node_api_add_env_cleanup_hook(env, cleanup, &secret);
  node_api_remove_env_cleanup_hook(env, cleanup, &wrong_secret);

  return NULL;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
