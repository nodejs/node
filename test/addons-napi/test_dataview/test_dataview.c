#include <node_api.h>
#include <string.h>
#include "../common.h"

napi_value CreateDataView(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args [1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 1, "Wrong number of arguments");

  napi_valuetype valuetype;
  napi_value input_dataview = args[0];

  NAPI_CALL(env, napi_typeof(env, input_dataview, &valuetype));
  NAPI_ASSERT(env, valuetype == napi_object,
              "Wrong type of arguments. Expects a DataView as the first "
              "argument.");

  bool is_dataview;
  NAPI_CALL(env, napi_is_dataview(env, input_dataview, &is_dataview));
  NAPI_ASSERT(env, is_dataview,
              "Wrong type of arguments. Expects a DataView as the first "
              "argument.");
  size_t byte_offset = 0;
  size_t length = 0;
  napi_value buffer;
  NAPI_CALL(env,
            napi_get_dataview_info(env, input_dataview, &length, NULL,
                                   &buffer, &byte_offset));

  napi_value output_dataview;
  NAPI_CALL(env,
            napi_create_dataview(env, length, buffer,
                                 byte_offset, &output_dataview));

  return output_dataview;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("CreateDataView", CreateDataView)
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
