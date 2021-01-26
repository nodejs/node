#include <stdlib.h>
#include <string.h>
#include <node_api.h>
#include "../../js-native-api/common.h"

static const char theText[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

static int deleterCallCount = 0;
static void deleteTheText(node_api_env env, void* data, void* finalize_hint) {
  NODE_API_ASSERT_RETURN_VOID(env, data != NULL && strcmp(data, theText) == 0,
      "invalid data");
  (void)finalize_hint;
  free(data);
  deleterCallCount++;
}

static void noopDeleter(node_api_env env, void* data, void* finalize_hint) {
  NODE_API_ASSERT_RETURN_VOID(env, data != NULL && strcmp(data, theText) == 0,
      "invalid data");
  (void)finalize_hint;
  deleterCallCount++;
}

static node_api_value
newBuffer(node_api_env env, node_api_callback_info info) {
  node_api_value theBuffer;
  char* theCopy;
  const unsigned int kBufferSize = sizeof(theText);

  NODE_API_CALL(env,
            node_api_create_buffer(
                env,
                sizeof(theText),
                (void**)(&theCopy),
                &theBuffer));
  NODE_API_ASSERT(env, theCopy, "Failed to copy static text for newBuffer");
  memcpy(theCopy, theText, kBufferSize);

  return theBuffer;
}

static node_api_value
newExternalBuffer(node_api_env env, node_api_callback_info info) {
  node_api_value theBuffer;
  char* theCopy = strdup(theText);
  NODE_API_ASSERT(env, theCopy,
      "Failed to copy static text for newExternalBuffer");
  NODE_API_CALL(env,
                node_api_create_external_buffer(
                    env,
                    sizeof(theText),
                    theCopy,
                    deleteTheText,
                    NULL,  // finalize_hint
                    &theBuffer));

  return theBuffer;
}

static node_api_value
getDeleterCallCount(node_api_env env, node_api_callback_info info) {
  node_api_value callCount;
  NODE_API_CALL(env, node_api_create_int32(env, deleterCallCount, &callCount));
  return callCount;
}

static node_api_value
copyBuffer(node_api_env env, node_api_callback_info info) {
  node_api_value theBuffer;
  NODE_API_CALL(env, node_api_create_buffer_copy(
      env, sizeof(theText), theText, NULL, &theBuffer));
  return theBuffer;
}

static node_api_value
bufferHasInstance(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");
  node_api_value theBuffer = args[0];
  bool hasInstance;
  node_api_valuetype theType;
  NODE_API_CALL(env, node_api_typeof(env, theBuffer, &theType));
  NODE_API_ASSERT(env, theType == node_api_object,
      "bufferHasInstance: instance is not an object");
  NODE_API_CALL(env, node_api_is_buffer(env, theBuffer, &hasInstance));
  NODE_API_ASSERT(env, hasInstance,
      "bufferHasInstance: instance is not a buffer");
  node_api_value returnValue;
  NODE_API_CALL(env, node_api_get_boolean(env, hasInstance, &returnValue));
  return returnValue;
}

static node_api_value
bufferInfo(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");
  node_api_value theBuffer = args[0];
  char *bufferData;
  node_api_value returnValue;
  size_t bufferLength;
  NODE_API_CALL(env,
                node_api_get_buffer_info(
                    env,
                    theBuffer,
                    (void**)(&bufferData),
                    &bufferLength));
  NODE_API_CALL(env, node_api_get_boolean(env,
      !strcmp(bufferData, theText) && bufferLength == sizeof(theText),
      &returnValue));
  return returnValue;
}

static node_api_value
staticBuffer(node_api_env env, node_api_callback_info info) {
  node_api_value theBuffer;
  NODE_API_CALL(
      env,
      node_api_create_external_buffer(env,
                                      sizeof(theText),
                                      (void*)theText,
                                      noopDeleter,
                                      NULL,  // finalize_hint
                                      &theBuffer));
  return theBuffer;
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_value theValue;

  NODE_API_CALL(env,
      node_api_create_string_utf8(env, theText, sizeof(theText), &theValue));
  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "theText", theValue));

  node_api_property_descriptor methods[] = {
    DECLARE_NODE_API_PROPERTY("newBuffer", newBuffer),
    DECLARE_NODE_API_PROPERTY("newExternalBuffer", newExternalBuffer),
    DECLARE_NODE_API_PROPERTY("getDeleterCallCount", getDeleterCallCount),
    DECLARE_NODE_API_PROPERTY("copyBuffer", copyBuffer),
    DECLARE_NODE_API_PROPERTY("bufferHasInstance", bufferHasInstance),
    DECLARE_NODE_API_PROPERTY("bufferInfo", bufferInfo),
    DECLARE_NODE_API_PROPERTY("staticBuffer", staticBuffer),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(methods) / sizeof(methods[0]), methods));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
