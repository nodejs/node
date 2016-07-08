#ifndef SRC_NODE_STAT_WATCHER_H_
#define SRC_NODE_STAT_WATCHER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "async-wrap.h"
#include "env.h"
#include "uv.h"
#include "v8.h"

namespace node {

class StatWatcher : public AsyncWrap {
 public:
  ~StatWatcher() override;

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

 protected:
  StatWatcher(Environment* env, v8::Local<v8::Object> wrap);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args);

  size_t self_size() const override { return sizeof(*this); }

 private:
  static void Callback(uv_fs_poll_t* handle,
                       int status,
                       const uv_stat_t* prev,
                       const uv_stat_t* curr);
  void Stop();

  uv_fs_poll_t* watcher_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_STAT_WATCHER_H_
