#include "node_l10n.h"

#include "node_wrap.h"
#include <assert.h>

namespace node {

  using v8::String;
  using v8::Boolean;
  using v8::HandleScope;
  using v8::FunctionCallbackInfo;
  using v8::Context;
  using v8::Object;
  using v8::Value;
  using v8::Isolate;
  using v8::Local;

namespace l10n {

  void Initialize(Local<Object> target,
                  Local<Value> unused,
                  Local<Context> context) {
    Environment* env = Environment::GetCurrent(context);
    env->SetMethod(target, "fetch", Fetch);
    #if defined(NODE_HAVE_I18N_SUPPORT)
      target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "icu"),
                  Boolean::New(env->isolate(), 1));
      target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "locale"),
                  String::NewFromUtf8(env->isolate(), L10N_LOCALE()));
    #endif
  }

  void Fetch(const FunctionCallbackInfo<Value>& args) {
    assert(args.Length() >= 2);
    HandleScope handle_scope(args.GetIsolate());
    String::Utf8Value key(args[0]);
    String::Value fallback(args[1]);
    args.GetReturnValue().Set(
      String::NewFromTwoByte(
        args.GetIsolate(),
        L10N(*key,*fallback)));
  }

} // namespace l10n
} // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(l10n, node::l10n::Initialize);
