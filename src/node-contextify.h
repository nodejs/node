#ifndef SRC_NODE_CONTEXTIFY_H_
#define SRC_NODE_CONTEXTIFY_H_

#include "base-object.h"
#include "env.h"
#include "v8.h"

namespace node {

class ContextifyScript : public BaseObject {
  public:
    ContextifyScript(Environment* env, v8::Local<v8::Object> object);
    ~ContextifyScript() override;

    void Dispose();
    static void Init(Environment* env, v8::Local<v8::Object> target);
    // args: code, [options]
    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static bool InstanceOf(Environment* env, const v8::Local<v8::Value>& value);
    // args: [options]
    static void RunInThisContext(
        const v8::FunctionCallbackInfo<v8::Value>& args);
    // args: sandbox, [options]
    static void RunInContext(const v8::FunctionCallbackInfo<v8::Value>& args);
    static int64_t GetTimeoutArg(
        const v8::FunctionCallbackInfo<v8::Value>& args, const int i);
    static bool GetDisplayErrorsArg(
        const v8::FunctionCallbackInfo<v8::Value>& args, const int i);
    static v8::Local<v8::String> GetFilenameArg(
        const v8::FunctionCallbackInfo<v8::Value>& args, const int i);
    static bool EvalMachine(Environment* env,
                            const int64_t timeout,
                            const bool display_errors,
                            const v8::FunctionCallbackInfo<v8::Value>& args,
                            v8::TryCatch& try_catch);
  private:
    v8::Persistent<v8::UnboundScript> script_;
};

}  // namespace node

#endif  // SRC_NODE_CONTEXTIFY_H_
