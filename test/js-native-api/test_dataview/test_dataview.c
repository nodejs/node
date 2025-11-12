#include <js_native_api.h>
#include <string.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value CreateDataView(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args [3];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 3, "Wrong number of arguments");

  napi_valuetype valuetype0;
  napi_value arraybuffer = args[0];

  NODE_API_CALL(env, napi_typeof(env, arraybuffer, &valuetype0));
  NODE_API_ASSERT(env, valuetype0 == napi_object,
              "Wrong type of arguments. Expects a ArrayBuffer as the first "
              "argument.");

  bool is_arraybuffer;
  NODE_API_CALL(env, napi_is_arraybuffer(env, arraybuffer, &is_arraybuffer));
  NODE_API_ASSERT(env, is_arraybuffer,
              "Wrong type of arguments. Expects a ArrayBuffer as the first "
              "argument.");

  napi_valuetype valuetype1;
  NODE_API_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == napi_number,
      "Wrong type of arguments. Expects a number as second argument.");

  size_t byte_offset = 0;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[1], (uint32_t*)(&byte_offset)));

  napi_valuetype valuetype2;
  NODE_API_CALL(env, napi_typeof(env, args[2], &valuetype2));

  NODE_API_ASSERT(env, valuetype2 == napi_number,
      "Wrong type of arguments. Expects a number as third argument.");

  size_t length = 0;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[2], (uint32_t*)(&length)));

  napi_value output_dataview;
  NODE_API_CALL(env,
            napi_create_dataview(env, length, arraybuffer,
                                 byte_offset, &output_dataview));

  return output_dataview;
}

static napi_value CreateDataViewFromJSDataView(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args [1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  napi_valuetype valuetype;
  napi_value input_dataview = args[0];

  NODE_API_CALL(env, napi_typeof(env, input_dataview, &valuetype));
  NODE_API_ASSERT(env, valuetype == napi_object,
              "Wrong type of arguments. Expects a DataView as the first "
              "argument.");

  bool is_dataview;
  NODE_API_CALL(env, napi_is_dataview(env, input_dataview, &is_dataview));
  NODE_API_ASSERT(env, is_dataview,
              "Wrong type of arguments. Expects a DataView as the first "
              "argument.");
  size_t byte_offset = 0;
  size_t length = 0;
  napi_value buffer;
  NODE_API_CALL(env,
            napi_get_dataview_info(env, input_dataview, &length, NULL,
                                   &buffer, &byte_offset));

  napi_value output_dataview;
  NODE_API_CALL(env,
            napi_create_dataview(env, length, buffer,
                                 byte_offset, &output_dataview));


  return output_dataview;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("CreateDataView", CreateDataView),
    DECLARE_NODE_API_PROPERTY("CreateDataViewFromJSDataView",
        CreateDataViewFromJSDataView)
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
