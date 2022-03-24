#include <node_api.h>
#include <memory>
#include <string>
#include "../../js-native-api/common.h"

namespace {

inline std::string testString = "123";

struct ValueHolder {
  int value{42};
};

class MyClass {
 public:
  // Ensure that the static variable is initialized by a dynamic static
  // initializer.
  static std::unique_ptr<ValueHolder> valueHolder;
};

std::unique_ptr<ValueHolder> MyClass::valueHolder{new ValueHolder()};

}  // namespace

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_value cppIntValue, cppStringValue;
  NODE_API_CALL(
      env, napi_create_int32(env, MyClass::valueHolder->value, &cppIntValue));
  NODE_API_CALL(
      env,
      napi_create_string_utf8(
          env, testString.c_str(), NAPI_AUTO_LENGTH, &cppStringValue));

  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_PROPERTY_VALUE("cppIntValue", cppIntValue),
      DECLARE_NODE_API_PROPERTY_VALUE("cppStringValue", cppStringValue)};

  NODE_API_CALL(env,
                napi_define_properties(
                    env, exports, std::size(descriptors), descriptors));

  return exports;
}
EXTERN_C_END

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
