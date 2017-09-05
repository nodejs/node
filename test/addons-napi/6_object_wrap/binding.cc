#include "myobject.h"

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  MyObject::Init(env, exports);
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
