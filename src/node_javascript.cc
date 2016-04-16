#include "node.h"
#include "node_natives.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {

using v8::HandleScope;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;

Local<String> MainSource(Environment* env) {
  return String::NewFromUtf8(
      env->isolate(),
      reinterpret_cast<const char*>(internal_bootstrap_node_native),
      NewStringType::kNormal,
      sizeof(internal_bootstrap_node_native)).ToLocalChecked();
}

void DefineJavaScript(Environment* env, Local<Object> target) {
  HandleScope scope(env->isolate());

  for (auto native : natives) {
    if (native.source != internal_bootstrap_node_native) {
      Local<String> name = String::NewFromUtf8(env->isolate(), native.name);
      Local<String> source =
          String::NewFromUtf8(
              env->isolate(), reinterpret_cast<const char*>(native.source),
              NewStringType::kNormal, native.source_len).ToLocalChecked();
      target->Set(name, source);
    }
  }
}

}  // namespace node
