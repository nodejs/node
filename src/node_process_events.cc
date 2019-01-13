#include <stdarg.h>

#include "env-inl.h"
#include "node_process.h"
#include "util.h"

namespace node {
using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Nothing;
using v8::Object;
using v8::String;
using v8::Value;

MaybeLocal<Value> ProcessEmit(Environment* env,
                              const char* event,
                              Local<Value> message) {
  // Send message to enable debug in cluster workers
  Local<Object> process = env->process_object();
  Isolate* isolate = env->isolate();
  Local<Value> argv[] = {OneByteString(isolate, event), message};

  return MakeCallback(isolate, process, "emit", arraysize(argv), argv, {0, 0});
}

Maybe<bool> ProcessEmitWarningGeneric(Environment* env,
                                      const char* warning,
                                      const char* type,
                                      const char* code) {
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> process = env->process_object();
  Local<Value> emit_warning;
  if (!process->Get(env->context(), env->emit_warning_string())
           .ToLocal(&emit_warning)) {
    return Nothing<bool>();
  }

  if (!emit_warning->IsFunction()) return Just(false);

  int argc = 0;
  Local<Value> args[3];  // warning, type, code

  // The caller has to be able to handle a failure anyway, so we might as well
  // do proper error checking for string creation.
  if (!String::NewFromUtf8(env->isolate(), warning, NewStringType::kNormal)
           .ToLocal(&args[argc++])) {
    return Nothing<bool>();
  }
  if (type != nullptr) {
    if (!String::NewFromOneByte(env->isolate(),
                                reinterpret_cast<const uint8_t*>(type),
                                NewStringType::kNormal)
             .ToLocal(&args[argc++])) {
      return Nothing<bool>();
    }
    if (code != nullptr &&
        !String::NewFromOneByte(env->isolate(),
                                reinterpret_cast<const uint8_t*>(code),
                                NewStringType::kNormal)
             .ToLocal(&args[argc++])) {
      return Nothing<bool>();
    }
  }

  // MakeCallback() unneeded because emitWarning is internal code, it calls
  // process.emit('warning', ...), but does so on the nextTick.
  if (emit_warning.As<Function>()
          ->Call(env->context(), process, argc, args)
          .IsEmpty()) {
    return Nothing<bool>();
  }
  return Just(true);
}

// Call process.emitWarning(str), fmt is a snprintf() format string
Maybe<bool> ProcessEmitWarning(Environment* env, const char* fmt, ...) {
  char warning[1024];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(warning, sizeof(warning), fmt, ap);
  va_end(ap);

  return ProcessEmitWarningGeneric(env, warning);
}

Maybe<bool> ProcessEmitDeprecationWarning(Environment* env,
                                          const char* warning,
                                          const char* deprecation_code) {
  return ProcessEmitWarningGeneric(
      env, warning, "DeprecationWarning", deprecation_code);
}

}  // namespace node
