#include <node_api.h>

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports);
EXTERN_C_END

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
