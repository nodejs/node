#ifndef SRC_NODE_API_INTERNALS_H_
#define SRC_NODE_API_INTERNALS_H_

#include "v8.h"
#define NAPI_EXPERIMENTAL
#include "env-inl.h"
#include "js_native_api_v8.h"
#include "node_api.h"
#include "util-inl.h"

struct node_napi_env__ : public napi_env__ {
  node_napi_env__(v8::Local<v8::Context> context,
                  const std::string& module_filename);

  bool can_call_into_js() const override;
  v8::Maybe<bool> mark_arraybuffer_as_untransferable(
      v8::Local<v8::ArrayBuffer> ab) const override;
  void CallFinalizer(napi_finalize cb, void* data, void* hint) override;

  inline node::Environment* node_env() const {
    return node::Environment::GetCurrent(context());
  }
  inline const char* GetFilename() const { return filename.c_str(); }

  std::string filename;
};

using node_napi_env = node_napi_env__*;

#endif  // SRC_NODE_API_INTERNALS_H_
