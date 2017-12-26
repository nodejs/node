#ifndef SRC_REQ_WRAP_H_
#define SRC_REQ_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
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
  // req_wrap_queue_ needs to be at a fixed offset from the start of the class
  // because it is used by ContainerOf to calculate the address of the embedding
  // ReqWrap. ContainerOf compiles down to simple, fixed pointer arithmetic. It
  // is also used by src/node_postmortem_metadata.cc to calculate offsets and
  // generate debug symbols for ReqWrap, which assumes that the position of
  // members in memory are predictable. sizeof(req_) depends on the type of T,
  // so req_wrap_queue_ would no longer be at a fixed offset if it came after
  // req_. For more information please refer to
  // `doc/guides/node-postmortem-support.md`
  T req_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_REQ_WRAP_H_
