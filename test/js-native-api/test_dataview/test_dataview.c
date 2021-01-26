#include <js_native_api.h>
#include <string.h>
#include "../common.h"

static node_api_value
CreateDataView(node_api_env env, node_api_callback_info info) {
  size_t argc = 3;
  node_api_value args [3];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 3, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  node_api_value arraybuffer = args[0];

  NODE_API_CALL(env, node_api_typeof(env, arraybuffer, &valuetype0));
  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects a ArrayBuffer as the first argument.");

  bool is_arraybuffer;
  NODE_API_CALL(env,
      node_api_is_arraybuffer(env, arraybuffer, &is_arraybuffer));
  NODE_API_ASSERT(env, is_arraybuffer,
      "Wrong type of arguments. Expects a ArrayBuffer as the first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == node_api_number,
      "Wrong type of arguments. Expects a number as second argument.");

  size_t byte_offset = 0;
  NODE_API_CALL(env,
      node_api_get_value_uint32(env, args[1], (uint32_t*)(&byte_offset)));

  node_api_valuetype valuetype2;
  NODE_API_CALL(env, node_api_typeof(env, args[2], &valuetype2));

  NODE_API_ASSERT(env, valuetype2 == node_api_number,
      "Wrong type of arguments. Expects a number as third argument.");

  size_t length = 0;
  NODE_API_CALL(env,
      node_api_get_value_uint32(env, args[2], (uint32_t*)(&length)));

  node_api_value output_dataview;
  NODE_API_CALL(env,
      node_api_create_dataview(env, length, arraybuffer, byte_offset,
          &output_dataview));

  return output_dataview;
}

static node_api_value
CreateDataViewFromJSDataView(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args [1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  node_api_value input_dataview = args[0];

  NODE_API_CALL(env, node_api_typeof(env, input_dataview, &valuetype));
  NODE_API_ASSERT(env, valuetype == node_api_object,
      "Wrong type of arguments. Expects a DataView as the first argument.");

  bool is_dataview;
  NODE_API_CALL(env, node_api_is_dataview(env, input_dataview, &is_dataview));
  NODE_API_ASSERT(env, is_dataview,
      "Wrong type of arguments. Expects a DataView as the first argument.");
  size_t byte_offset = 0;
  size_t length = 0;
  node_api_value buffer;
  NODE_API_CALL(env,
      node_api_get_dataview_info(env, input_dataview, &length, NULL, &buffer,
          &byte_offset));

  node_api_value output_dataview;
  NODE_API_CALL(env,
      node_api_create_dataview(env, length, buffer, byte_offset,
          &output_dataview));


  return output_dataview;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("CreateDataView", CreateDataView),
    DECLARE_NODE_API_PROPERTY("CreateDataViewFromJSDataView",
        CreateDataViewFromJSDataView)
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
