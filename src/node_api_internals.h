#ifndef SRC_NODE_API_INTERNALS_H_
#define SRC_NODE_API_INTERNALS_H_

#include "v8.h"
#define NAPI_EXPERIMENTAL
#include "env-inl.h"
#include "js_native_api_v8.h"
#include "node_api.h"
#include "util-inl.h"

struct node_napi_env__ : public napi_env__ {
  static napi_env New(v8::Local<v8::Context> context,
                      const std::string& module_filename,
                      int32_t module_api_version);

  node_napi_env__(v8::Local<v8::Context> context,
                  const std::string& module_filename,
                  int32_t module_api_version);

  bool can_call_into_js() const override;
  void CallFinalizer(napi_finalize cb, void* data, void* hint) override;
  template <bool enforceUncaughtExceptionPolicy>
  void CallFinalizer(napi_finalize cb, void* data, void* hint);

  void EnqueueFinalizer(v8impl::RefTracker* finalizer) override;
  void DrainFinalizerQueue();

  void trigger_fatal_exception(v8::Local<v8::Value> local_err);
  template <bool enforceUncaughtExceptionPolicy, typename T>
  void CallbackIntoModule(T&& call);

  void DeleteMe() override;

  inline node::Environment* node_env() const {
    return node::Environment::GetCurrent(context());
  }
  inline const char* GetFilename() const { return filename.c_str(); }

  std::string filename;
  bool destructing = false;
  bool finalization_scheduled = false;
};

using node_napi_env = node_napi_env__*;

#endif  // SRC_NODE_API_INTERNALS_H_
