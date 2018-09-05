#ifndef SRC_NODE_LOCKS_H_
#define SRC_NODE_LOCKS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <deque>
#include <memory>  // std::unique_ptr
#include <string>
#include "node.h"
#include "node_persistent.h"
#include "node_internals.h"
#include "env-inl.h"
#include "v8.h"

namespace node {
namespace worker {

class Lock {
 public:
  enum Mode {
    kShared,
    kExclusive,
  };

  Lock(Environment* env,
       Lock::Mode mode,
       const std::string& name,
       v8::Local<v8::Promise::Resolver> released_promise,
       v8::Local<v8::Promise::Resolver> waiting_promise)
      : env_(env), mode_(mode), name_(name) {
    released_promise_.Reset(env->isolate(), released_promise);
    waiting_promise_.Reset(env->isolate(), waiting_promise);
  }

  ~Lock() {
    waiting_promise_.Reset();
    released_promise_.Reset();
  }

  inline const std::string& name() const { return name_; }
  inline Mode mode() const { return mode_; }
  inline v8::Local<v8::Promise::Resolver> released_promise() {
    return released_promise_.Get(env_->isolate());
  }

 private:
  Environment* env_;
  Mode mode_;
  std::string name_;
  Persistent<v8::Promise::Resolver> waiting_promise_;
  Persistent<v8::Promise::Resolver> released_promise_;
};

class LockRequest {
 public:
  LockRequest(Environment* env,
              v8::Local<v8::Promise::Resolver> promise,
              v8::Local<v8::Function> callback,
              const std::string& name,
              Lock::Mode mode)
      : env_(env), name_(name), mode_(mode) {
    promise_.Reset(env->isolate(), promise);
    callback_.Reset(env->isolate(), callback);
  }

  ~LockRequest() {
    callback_.Reset();
    promise_.Reset();
  }

  inline const std::string& name() const { return name_; }
  inline Lock::Mode mode() const { return mode_; }
  inline v8::Local<v8::Promise::Resolver> promise() const {
    return promise_.Get(env_->isolate());
  }
  inline v8::Local<v8::Function> callback() const {
    return callback_.Get(env_->isolate());
  }

 private:
  Environment* env_;
  std::string name_;
  Lock::Mode mode_;
  Persistent<v8::Function> callback_;
  Persistent<v8::Promise::Resolver> promise_;
};

class LockManager {
 public:
  void Snapshot(Environment* env, v8::Local<v8::Promise::Resolver> promise);
  void ProcessQueue(Environment* env);
  bool IsGrantable(const LockRequest* request);

  static void ReleaseLock(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Snapshot(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Request(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Query(const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline LockManager* GetCurrent() { return &current_; }

 private:
  static LockManager current_;

  std::deque<std::unique_ptr<LockRequest>> pending_requests_;
  std::deque<std::unique_ptr<Lock>> held_locks_;

  mutable Mutex work_mutex_;

  friend class LockRequest;
};

}  // namespace worker
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_LOCKS_H_
