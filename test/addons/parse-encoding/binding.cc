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

void ParseEncoding(const v8::FunctionCallbackInfo<v8::Value>& args) {
  const node::encoding encoding =
      node::ParseEncoding(args.GetIsolate(), args[0],
                          static_cast<node::encoding>(-1));
  const char* encoding_name = "UNKNOWN";
#define V(name) if (encoding == node::name) encoding_name = #name;
  ENCODING_MAP(V)
#undef V
  auto encoding_string =
      v8::String::NewFromUtf8(args.GetIsolate(), encoding_name,
                              v8::NewStringType::kNormal)
      .ToLocalChecked();
  args.GetReturnValue().Set(encoding_string);
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "parseEncoding", ParseEncoding);
}

}  // anonymous namespace

NODE_MODULE(binding, Initialize);
