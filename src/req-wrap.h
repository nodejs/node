#ifndef SRC_REQ_WRAP_H_
#define SRC_REQ_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async-wrap.h"
#include "env.h"
#include "util.h"
#include "v8.h"

namespace node {

template <typename T>
class ReqWrap : public AsyncWrap {
 public:
  inline ReqWrap(Environment* env,
                 v8::Local<v8::Object> object,
                 AsyncWrap::ProviderType provider);
  inline ~ReqWrap() override;
  inline void Dispatched();  // Call this after the req has been dispatched.
  T* req() { return &req_; }

 private:
  friend class Environment;
  ListNode<ReqWrap> req_wrap_queue_;

 protected:
  T req_;  // Must be last.
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_REQ_WRAP_H_
