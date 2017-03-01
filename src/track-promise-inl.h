#ifndef SRC_TRACK_PROMISE_INL_H_
#define SRC_TRACK_PROMISE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "track-promise.h"
#include "env.h"

namespace node {

template<typename Iterator>
void PromiseTracker::ForEach(Iterator fn) {
  for (auto it = set_.begin(); it != set_.end(); ++it) {
    v8::Local<v8::Object> object = (*it)->Get(env_->isolate());
    fn(env_, object);
  }
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TRACK_PROMISE_INL_H_
