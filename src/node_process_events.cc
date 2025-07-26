#include <set>

#include "env-inl.h"
#include "node_process-inl.h"
#include "util.h"

namespace node {
using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Nothing;
using v8::Object;
using v8::String;
using v8::Value;

Maybe<void> ProcessEmitWarningSync(Environment* env, std::string_view message) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<String> message_string;
  if (!String::NewFromUtf8(isolate,
                           message.data(),
                           NewStringType::kNormal,
                           static_cast<int>(message.size()))
           .ToLocal(&message_string)) {
    return Nothing<void>();
  }

  Local<Value> argv[] = {message_string};
  Local<Function> emit_function = env->process_emit_warning_sync();
  // If this fails, this is called too early - before the bootstrap is even
  // finished.
  CHECK(!emit_function.IsEmpty());
  if (emit_function.As<Function>()
          ->Call(context, v8::Undefined(isolate), arraysize(argv), argv)
          .IsEmpty()) {
    return Nothing<void>();
  }
  return JustVoid();
}

MaybeLocal<Value> ProcessEmit(Environment* env,
                              std::string_view event,
                              Local<Value> message) {
  Isolate* isolate = env->isolate();

  Local<Value> event_string;
  if (!ToV8Value(env->context(), event).ToLocal(&event_string)) {
    return MaybeLocal<Value>();
  }

  Local<Object> process = env->process_object();
  Local<Value> argv[] = {event_string, message};
  return MakeCallback(
      isolate, process, env->emit_string(), arraysize(argv), argv, {0, 0});
}

Maybe<bool> ProcessEmitWarningGeneric(Environment* env,
                                      std::string_view warning,
                                      std::string_view type,
                                      std::string_view code) {
  if (!env->can_call_into_js()) {
    return Just(false);
  }

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
  if (!ToV8Value(env->context(), warning).ToLocal(&args[argc++])) {
    return Nothing<bool>();
  }

  if (!type.empty()) {
    if (!ToV8Value(env->context(), type).ToLocal(&args[argc++])) {
      return Nothing<bool>();
    }
    if (!code.empty() &&
        !ToV8Value(env->context(), code).ToLocal(&args[argc++])) {
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

std::set<std::string> experimental_warnings;

Maybe<bool> ProcessEmitExperimentalWarning(Environment* env,
                                           const std::string& warning) {
  if (experimental_warnings.contains(warning)) return Nothing<bool>();

  experimental_warnings.insert(warning);
  std::string message(warning);
  message.append(" is an experimental feature and might change at any time");
  return ProcessEmitWarningGeneric(env, message.c_str(), "ExperimentalWarning");
}

Maybe<bool> ProcessEmitDeprecationWarning(Environment* env,
                                          const std::string& warning,
                                          std::string_view deprecation_code) {
  return ProcessEmitWarningGeneric(
      env, warning, "DeprecationWarning", deprecation_code);
}

}  // namespace node
