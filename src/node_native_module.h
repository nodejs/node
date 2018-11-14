#ifndef SRC_NODE_NATIVE_MODULE_H_
#define SRC_NODE_NATIVE_MODULE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"

namespace node {
namespace native_module {

// The native (C++) side of the native module compilation.

class NativeModule {
 public:
  // For legacy process.binding('natives') which is mutable
  static void GetNatives(Environment* env, v8::Local<v8::Object> exports);
  // Loads the static JavaScript source code and the cache into Environment
  static void LoadBindings(Environment* env);
  // Compile code cache for a specific native module
  static void CompileCodeCache(const v8::FunctionCallbackInfo<v8::Value>& args);
  // Compile a specific native module as a function
  static void CompileFunction(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static v8::Local<v8::Value> CompileAsModule(Environment* env,
                                              v8::Local<v8::String> id,
                                              bool produce_code_cache);
  // TODO(joyeecheung): make this public and reuse it to compile bootstrappers
  static v8::Local<v8::Value> Compile(Environment* env,
                                      v8::Local<v8::String> id,
                                      v8::Local<v8::String> parameters[],
                                      size_t parameters_count,
                                      bool produce_code_cache);
};

}  // namespace native_module
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_NATIVE_MODULE_H_
