#ifndef SRC_NODE_API_BACKPORT_H_
#define SRC_NODE_API_BACKPORT_H_

// This file contains stubs for symbols and other features (such as async hooks)
// which have appeared in later versions of Node.js, and which are required for
// N-API.

#include "node_internals.h"

namespace node {

typedef int async_id;

typedef struct async_context {
  node::async_id async_id;
  node::async_id trigger_async_id;
} async_context;

class CallbackScope {
 public:
  CallbackScope(v8::Isolate *isolate,
                v8::Local<v8::Object> object,
                node::async_context context);
  ~CallbackScope();
 private:
  v8::Isolate* isolate = nullptr;
  node::Environment* env = nullptr;
  v8::TryCatch _try_catch;
  bool ran_init_callback = false;
  v8::Local<v8::Object> object;
  Environment::AsyncCallbackScope callback_scope;
};

NODE_EXTERN async_context EmitAsyncInit(v8::Isolate* isolate,
                                        v8::Local<v8::Object> resource,
                                        v8::Local<v8::String> name,
                                        async_id trigger_async_id = -1);

NODE_EXTERN void EmitAsyncDestroy(v8::Isolate* isolate,
                                  async_context asyncContext);

v8::MaybeLocal<v8::Value> MakeCallback(v8::Isolate* isolate,
                                       v8::Local<v8::Object> recv,
                                       v8::Local<v8::Function> callback,
                                       int argc,
                                       v8::Local<v8::Value>* argv,
                                       async_context asyncContext);

class AsyncResource {
 public:
  AsyncResource(v8::Isolate* _isolate,
                v8::Local<v8::Object> _object,
                char* name);
  ~AsyncResource();
  v8::Isolate* isolate;
  v8::Persistent<v8::Object> object;
};
}  // end of namespace node

class CallbackScope {
 public:
  explicit CallbackScope(node::AsyncResource* work);
 private:
  node::CallbackScope scope;
};

#endif  // SRC_NODE_API_BACKPORT_H_
