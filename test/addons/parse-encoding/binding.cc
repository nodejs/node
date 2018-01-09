#include <node.h>
#include <v8.h>

namespace {

#define ENCODING_MAP(V) \
  V(ASCII)              \
  V(BASE64)             \
  V(BUFFER)             \
  V(HEX)                \
  V(LATIN1)             \
  V(UCS2)               \
  V(UTF8)               \

static_assert(node::BINARY == node::LATIN1, "BINARY == LATIN1");

using v8::FunctionCallbackInfo;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Local;
using v8::Value;

void ParseEncoding(const FunctionCallbackInfo<Value>& args) {
  const node::encoding encoding =
      node::ParseEncoding(args.GetIsolate(), args[0],
                          static_cast<node::encoding>(-1));
  const char* encoding_name = "UNKNOWN";
#define V(name) if (encoding == node::name) encoding_name = #name;
  ENCODING_MAP(V)
#undef V
  auto encoding_string =
      String::NewFromUtf8(args.GetIsolate(), encoding_name,
                              NewStringType::kNormal)
      .ToLocalChecked();
  args.GetReturnValue().Set(encoding_string);
}

void Initialize(Local<Object> exports) {
  NODE_SET_METHOD(exports, "parseEncoding", ParseEncoding);
}

}  // anonymous namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
