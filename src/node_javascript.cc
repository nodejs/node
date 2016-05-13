#include "node.h"
#include "node_natives.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {

using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;

// id##_data is defined in node_natives.h.
#define V(id)                                                                 \
  static struct : public String::ExternalOneByteStringResource {              \
    const char* data() const override {                                       \
      return reinterpret_cast<const char*>(id##_data);                        \
    }                                                                         \
    size_t length() const override { return sizeof(id##_data); }              \
    void Dispose() override { /* Default calls `delete this`. */ }            \
  } id##_external_data;
NODE_NATIVES_MAP(V)
#undef V

Local<String> MainSource(Environment* env) {
  auto maybe_string =
      String::NewExternalOneByte(
          env->isolate(),
          &internal_bootstrap_node_external_data);
  return maybe_string.ToLocalChecked();
}

void DefineJavaScript(Environment* env, Local<Object> target) {
  auto context = env->context();
#define V(id)                                                                 \
  do {                                                                        \
    auto key =                                                                \
        String::NewFromOneByte(                                               \
            env->isolate(), id##_name, NewStringType::kNormal,                \
            sizeof(id##_name)).ToLocalChecked();                              \
    auto value =                                                              \
        String::NewExternalOneByte(                                           \
            env->isolate(), &id##_external_data).ToLocalChecked();            \
    CHECK(target->Set(context, key, value).FromJust());                       \
  } while (0);
  NODE_NATIVES_MAP(V)
#undef V
}

}  // namespace node
