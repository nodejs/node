#include "node.h"
#include "node_natives.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

#include <string.h>
#if !defined(_MSC_VER)
#include <strings.h>
#endif

namespace node {

using v8::ArrayBuffer;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Uint8Array;

ScriptCompiler::Source* MainSource(Environment* env, Local<String> filename) {
  Local<String> source_str =
      OneByteString(env->isolate(), node_native, sizeof(node_native) - 1);

  ScriptOrigin origin(filename);
  ScriptCompiler::CachedData* cached_data = nullptr;

  // NOTE: It is illegal to have empty arrays in C, so we use { 0 } for this
  // purposes.
  if (sizeof(node_native_cache) <= 1) {
    cached_data = new ScriptCompiler::CachedData(node_native_cache,
                                                 sizeof(node_native_cache));
  }

  return new ScriptCompiler::Source(source_str, origin, cached_data);
}

void DefineJavaScript(Environment* env, Local<Object> target) {
  HandleScope scope(env->isolate());

  Local<String> cache_postfix =
      String::NewFromUtf8(env->isolate(), "__cached_data");

  for (int i = 0; natives[i].name; i++) {
    if (natives[i].source != node_native) {
      Local<String> name = String::NewFromUtf8(env->isolate(), natives[i].name);
      Local<String> source = String::NewFromUtf8(env->isolate(),
                                                 natives[i].source,
                                                 String::kNormalString,
                                                 natives[i].source_len);
      target->Set(name, source);

      if (natives[i].cache_len <= 1)
        continue;

      Local<ArrayBuffer> cache_ab = ArrayBuffer::New(env->isolate(),
          const_cast<unsigned char*>(natives[i].cache),
          natives[i].cache_len);
      Local<Uint8Array> cache =
          Uint8Array::New(cache_ab, 0, natives[i].cache_len);

      target->Set(String::Concat(name, cache_postfix), cache);
    }
  }
}

}  // namespace node
