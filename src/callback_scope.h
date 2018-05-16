#ifndef SRC_CALLBACK_SCOPE_H_
#define SRC_CALLBACK_SCOPE_H_

#ifdef _WIN32
# ifndef BUILDING_NODE_EXTENSION
#   define NODE_EXTERN __declspec(dllexport)
# else
#   define NODE_EXTERN __declspec(dllimport)
# endif
#else
# define NODE_EXTERN /* nothing */
#endif

#include "v8.h"

namespace node {

typedef double async_id;
struct async_context {
  ::node::async_id async_id;
  ::node::async_id trigger_async_id;
};

class InternalCallbackScope;

/* This class works like `MakeCallback()` in that it sets up a specific
 * asyncContext as the current one and informs the async_hooks and domains
 * modules that this context is currently active.
 *
 * `MakeCallback()` is a wrapper around this class as well as
 * `Function::Call()`. Either one of these mechanisms needs to be used for
 * top-level calls into JavaScript (i.e. without any existing JS stack).
 *
 * This object should be stack-allocated to ensure that it is contained in a
 * valid HandleScope.
 */
class NODE_EXTERN CallbackScope {
 public:
  CallbackScope(v8::Isolate* isolate,
                v8::Local<v8::Object> resource,
                async_context asyncContext);
  ~CallbackScope();

 private:
  InternalCallbackScope* private_;
  v8::TryCatch try_catch_;

  void operator=(const CallbackScope&) = delete;
  void operator=(CallbackScope&&) = delete;
  CallbackScope(const CallbackScope&) = delete;
  CallbackScope(CallbackScope&&) = delete;
};

}  // namespace node

#endif  // SRC_CALLBACK_SCOPE_H_
