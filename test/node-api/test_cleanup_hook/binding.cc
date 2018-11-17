#include "node_api.h"
#include "uv.h"
#include "../../js-native-api/common.h"

namespace {

void cleanup(void* arg) {
  printf("cleanup(%d)\n", *static_cast<int*>(arg));
}

int secret = 42;
int wrong_secret = 17;

napi_value Init(napi_env env, napi_value exports) {
  napi_add_env_cleanup_hook(env, cleanup, &wrong_secret);
  napi_add_env_cleanup_hook(env, cleanup, &secret);
  napi_remove_env_cleanup_hook(env, cleanup, &wrong_secret);

  return nullptr;
}

}  // anonymous namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
