#include <string.h>
#include <string>
#include <node_api.h>

#define JS_ASSERT(env, assertion, message)                             \
  if (!(assertion)) {                                                  \
    napi_throw_error(                                                  \
        (env),                                                         \
        (std::string("assertion (" #assertion ") failed: ") + message) \
            .c_str());                                                 \
    return;                                                            \
  }

#define NAPI_CALL(env, theCall)                                           \
  if (theCall != napi_ok) {                                               \
    const char *errorMessage = napi_get_last_error_info()->error_message; \
    errorMessage = errorMessage ? errorMessage : "empty error message";   \
    napi_throw_error((env), errorMessage);                                \
    return;                                                               \
  }

static const char theText[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

static int deleterCallCount = 0;
static void deleteTheText(void *data, void* /*finalize_hint*/) {
  delete reinterpret_cast<char *>(data);
  deleterCallCount++;
}

static void noopDeleter(void *data, void* /*finalize_hint*/) {
    deleterCallCount++;
}

void newBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  char *theCopy;
  const unsigned int kBufferSize = sizeof(theText);

  NAPI_CALL(env,
            napi_create_buffer(
                env,
                sizeof(theText),
                reinterpret_cast<void**>(&theCopy),
                &theBuffer));
  JS_ASSERT(env, theCopy, "Failed to copy static text for newBuffer");
  snprintf(theCopy, kBufferSize, "%s", theText);
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

void newExternalBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  char *theCopy = strdup(theText);
  JS_ASSERT(env, theCopy, "Failed to copy static text for newExternalBuffer");
  NAPI_CALL(env,
            napi_create_external_buffer(
                env,
                sizeof(theText),
                theCopy,
                deleteTheText,
                nullptr,  // finalize_hint
                &theBuffer));
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

void getDeleterCallCount(napi_env env, napi_callback_info info) {
  napi_value callCount;
  NAPI_CALL(env, napi_create_number(env, deleterCallCount, &callCount));
  NAPI_CALL(env, napi_set_return_value(env, info, callCount));
}

void copyBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  NAPI_CALL(env,
            napi_create_buffer_copy(env, theText, sizeof(theText), &theBuffer));
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

void bufferHasInstance(napi_env env, napi_callback_info info) {
  int argc;
  NAPI_CALL(env, napi_get_cb_args_length(env, info, &argc));
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer;
  NAPI_CALL(env, napi_get_cb_args(env, info, &theBuffer, 1));
  bool hasInstance;
  napi_valuetype theType;
  NAPI_CALL(env, napi_get_type_of_value(env, theBuffer, &theType));
  JS_ASSERT(env,
            theType == napi_object,
            "bufferHasInstance: instance is not an object");
  NAPI_CALL(env, napi_is_buffer(env, theBuffer, &hasInstance));
  JS_ASSERT(env, hasInstance, "bufferHasInstance: instance is not a buffer");
  napi_value returnValue;
  NAPI_CALL(env, napi_create_boolean(env, hasInstance, &returnValue));
  NAPI_CALL(env, napi_set_return_value(env, info, returnValue));
}

void bufferInfo(napi_env env, napi_callback_info info) {
  int argc;
  NAPI_CALL(env, napi_get_cb_args_length(env, info, &argc));
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer, returnValue;
  NAPI_CALL(env, napi_get_cb_args(env, info, &theBuffer, 1));
  char *bufferData;
  size_t bufferLength;
  NAPI_CALL(env,
            napi_get_buffer_info(
                env,
                theBuffer,
                reinterpret_cast<void**>(&bufferData),
                &bufferLength));
  NAPI_CALL(env, napi_create_boolean(env,
    !strcmp(bufferData, theText) && bufferLength == sizeof(theText),
    &returnValue));
  NAPI_CALL(env, napi_set_return_value(env, info, returnValue));
}

void staticBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  NAPI_CALL(
      env,
      napi_create_external_buffer(env,
                                  sizeof(theText),
                                  const_cast<char *>(theText),
                                  noopDeleter,
                                  nullptr,  // finalize_hint
                                  &theBuffer));
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_value theValue;

  NAPI_CALL(env, napi_create_string_utf8(env,
            theText, sizeof(theText), &theValue));
  NAPI_CALL(env, napi_set_named_property(env, exports, "theText", theValue));

  napi_property_descriptor methods[] = {
      DECLARE_NAPI_METHOD("newBuffer", newBuffer),
      DECLARE_NAPI_METHOD("newExternalBuffer", newExternalBuffer),
      DECLARE_NAPI_METHOD("getDeleterCallCount", getDeleterCallCount),
      DECLARE_NAPI_METHOD("copyBuffer", copyBuffer),
      DECLARE_NAPI_METHOD("bufferHasInstance", bufferHasInstance),
      DECLARE_NAPI_METHOD("bufferInfo", bufferInfo),
      DECLARE_NAPI_METHOD("staticBuffer", staticBuffer),
  };
  NAPI_CALL(env,
            napi_define_properties(
                env, exports, sizeof(methods) / sizeof(methods[0]), methods));
}

NAPI_MODULE(addon, Init)
