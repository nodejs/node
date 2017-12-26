#include <node_api.h>
#include <string.h>
#include "../common.h"

napi_value CreateDataView(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args [3];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 3, "Wrong number of arguments");

  napi_valuetype valuetype0;
  napi_value arraybuffer = args[0];

  NAPI_CALL(env, napi_typeof(env, arraybuffer, &valuetype0));
  NAPI_ASSERT(env, valuetype0 == napi_object,
              "Wrong type of arguments. Expects a ArrayBuffer as the first "
              "argument.");

  bool is_arraybuffer;
  NAPI_CALL(env, napi_is_arraybuffer(env, arraybuffer, &is_arraybuffer));
  NAPI_ASSERT(env, is_arraybuffer,
              "Wrong type of arguments. Expects a ArrayBuffer as the first "
              "argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NAPI_ASSERT(env, valuetype1 == napi_number,
      "Wrong type of arguments. Expects a number as second argument.");

  size_t byte_offset = 0;
  NAPI_CALL(env, napi_get_value_uint32(env, args[1], (uint32_t*)(&byte_offset)));

  napi_valuetype valuetype2;
  NAPI_CALL(env, napi_typeof(env, args[2], &valuetype2));

  NAPI_ASSERT(env, valuetype2 == napi_number,
      "Wrong type of arguments. Expects a number as third argument.");

  size_t length = 0;
  NAPI_CALL(env, napi_get_value_uint32(env, args[2], (uint32_t*)(&length)));

  napi_value output_dataview;
  NAPI_CALL(env,
            napi_create_dataview(env, length, arraybuffer,
                                 byte_offset, &output_dataview));

  return output_dataview;
}

napi_value CreateDataViewFromJSDataView(napi_env env, napi_callback_info info) {
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

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("CreateDataView", CreateDataView),
    DECLARE_NAPI_PROPERTY("CreateDataViewFromJSDataView",
        CreateDataViewFromJSDataView)
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
