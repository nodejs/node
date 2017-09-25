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
  // Call this after the req has been dispatched, if that did not already
  // happen by using Dispatch().
  inline void Dispatched();
  T* req() { return &req_; }
  inline void Cancel();

  template <typename LibuvFunction, typename... Args>
  inline int Dispatch(LibuvFunction fn, Args... args);

 private:
  friend class Environment;
  template<typename ReqT, typename U>
  friend struct MakeLibuvRequestCallback;

  ListNode<ReqWrap> req_wrap_queue_;

  typedef void (*callback_t)();
  callback_t original_callback_ = nullptr;

 protected:
  // req_wrap_queue_ needs to be at a fixed offset from the start of the class
  // because it is used by ContainerOf to calculate the address of the embedding
  // ReqWrap. ContainerOf compiles down to simple, fixed pointer arithmetic.
  // sizeof(req_) depends on the type of T, so req_wrap_queue_ would
  // no longer be at a fixed offset if it came after req_.
  T req_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_REQ_WRAP_H_
