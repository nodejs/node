#ifndef SRC_TRACK_PROMISE_H_
#define SRC_TRACK_PROMISE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

#include <unordered_set>
#include <memory>

namespace node {

class Environment;

struct v8ObjectHash {
  size_t operator() (v8::Persistent<v8::Object>* handle) const;
  
  Environment* env;
};

struct v8ObjectEquals {
  bool operator() (v8::Persistent<v8::Object>* lhs,
                   v8::Persistent<v8::Object>* rhs) const;

  Environment* env;
};

class PromiseTracker {
 public:
  explicit inline PromiseTracker(Environment* env)
    : env_(env), set_(0, v8ObjectHash({env}), v8ObjectEquals({env})) { }

  void TrackPromise(v8::Local<v8::Object> promise);
  void UntrackPromise(v8::Local<v8::Object> promise);
  bool HasPromise(v8::Local<v8::Object> promise);

  template<typename Iterator>
  inline void ForEach(Iterator fn);

  inline size_t Size() const { return set_.size(); }

  static void WeakCallback(
      const v8::WeakCallbackInfo<v8::Persistent<v8::Object>>& data);
 private:
  inline void WeakCallback(v8::Persistent<v8::Object>* promise);

  Environment* env_;
  std::unordered_set<v8::Persistent<v8::Object>*,
                     v8ObjectHash,
                     v8ObjectEquals> set_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TRACK_PROMISE_H_
