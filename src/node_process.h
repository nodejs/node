#ifndef SRC_NODE_PROCESS_H_
#define SRC_NODE_PROCESS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

namespace node {

v8::MaybeLocal<v8::Object> CreateEnvVarProxy(v8::Local<v8::Context> context,
                                             v8::Isolate* isolate,
                                             v8::Local<v8::Object> data);

// Most of the time, it's best to use `console.error` to write
// to the process.stderr stream.  However, in some cases, such as
// when debugging the stream.Writable class or the process.nextTick
// function, it is useful to bypass JavaScript entirely.
void RawDebug(const v8::FunctionCallbackInfo<v8::Value>& args);

v8::MaybeLocal<v8::Value> ProcessEmit(Environment* env,
                                      const char* event,
                                      v8::Local<v8::Value> message);

v8::Maybe<bool> ProcessEmitWarningGeneric(Environment* env,
                                          const char* warning,
                                          const char* type = nullptr,
                                          const char* code = nullptr);

v8::Maybe<bool> ProcessEmitWarning(Environment* env, const char* fmt, ...);
v8::Maybe<bool> ProcessEmitExperimentalWarning(Environment* env,
                                              const char* warning);
v8::Maybe<bool> ProcessEmitDeprecationWarning(Environment* env,
                                              const char* warning,
                                              const char* deprecation_code);

v8::MaybeLocal<v8::Object> CreateProcessObject(Environment* env);
void PatchProcessObject(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace node
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_PROCESS_H_
