#include "node.h"
#include "node_natives.h"
#include "node_internals.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {

using v8::Local;
using v8::MaybeLocal;
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

static MaybeLocal<String> MaybeLoadSourceFromDisk(Environment* env,
                                                  const uint8_t* name,
                                                  size_t name_length) {
  if (internal_modules_source_dir == nullptr) {
    return MaybeLocal<String>();
  }

  std::string name_str(reinterpret_cast<const char*>(name), name_length);

  std::string path = std::string(internal_modules_source_dir) +
      '/' + name_str + ".js";
  return InternalModuleReadFile(env, path.c_str());
}

Local<String> MainSource(Environment* env) {
  if (js_entry_point != nullptr) {
    return InternalModuleReadFile(env, js_entry_point).ToLocalChecked();
  }

  auto maybe_disk_src =
      MaybeLoadSourceFromDisk(env,
                              internal_bootstrap_node_name,
                              sizeof(internal_bootstrap_node_name));
  if (!maybe_disk_src.IsEmpty()) {
    return maybe_disk_src.ToLocalChecked();
  }

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
    Local<String> value;                                                      \
    auto maybe_disk_src =                                                     \
        MaybeLoadSourceFromDisk(env, id##_name, sizeof(id##_name));           \
    if (!maybe_disk_src.IsEmpty()) {                                          \
      value = maybe_disk_src.ToLocalChecked();                                \
    } else {                                                                  \
        value = String::NewExternalOneByte(                                   \
            env->isolate(), &id##_external_data).ToLocalChecked();            \
    }                                                                         \
    CHECK(target->Set(context, key, value).FromJust());                       \
  } while (0);
  NODE_NATIVES_MAP(V)
#undef V
}

}  // namespace node
