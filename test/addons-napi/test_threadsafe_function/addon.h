#ifndef TEST_ADDONS_NAPI_TEST_THREADSAFE_FUNCTION_ADDON_H_
#define TEST_ADDONS_NAPI_TEST_THREADSAFE_FUNCTION_ADDON_H_

#include <node_api.h>

typedef void (*AddonInit)(napi_env env,
                          napi_value exports,
                          size_t prop_count,
                          napi_property_descriptor* props);

void InitTestBasic(napi_env env, napi_value exports, AddonInit init);
void InitTestEmpty(napi_env env, napi_value exports, AddonInit init);

#endif  // TEST_ADDONS_NAPI_TEST_THREADSAFE_FUNCTION_ADDON_H_
