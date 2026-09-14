#include <node_api.h>

// This add-on declares a Node-API version that no build supports, so loading it
// must fail with the error `node_napi_env__::New()` throws -- not a crash.
NAPI_MODULE_INIT() {
  return exports;
}
