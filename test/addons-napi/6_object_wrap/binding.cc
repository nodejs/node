#include "myobject.h"
#include "../common.h"

napi_value Init(napi_env env, napi_value exports) {
  MyObject::Init(env, exports);
  return exports;
}

NAPI_MODULE(addon, Init)
