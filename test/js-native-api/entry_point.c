#include <node_api.h>

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports);
EXTERN_C_END

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
