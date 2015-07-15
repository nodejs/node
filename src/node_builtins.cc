#include "node.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {

using v8::Array;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;

void DefineBuiltins(Environment* env,
                    Handle<Object> target,
                    node_module* builtins) {
  HandleScope scope(env->isolate());
  struct node_module* mp;
  int i = 0;
  Local<Array> bindings = Array::New(env->isolate(), 0);

  for (mp = builtins; mp != nullptr; mp = mp->nm_link) {
    Local<String> name = String::NewFromUtf8(env->isolate(), mp->nm_modname);
    bindings->Set(i++, name);
  }
  target->Set(String::NewFromUtf8(env->isolate(), "bindings"), bindings);
}

}  // namespace node
