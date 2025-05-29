#ifndef SRC_NODE_LOCKS_H_
#define SRC_NODE_LOCKS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "env.h"
#include "node_mutex.h"
#include "v8.h"

namespace node {
namespace worker {
namespace locks {

class Lock final {
 public:
  enum Mode { kShared, kExclusive };

  Lock(Environment* env,
       const std::u16string& name,
       Mode mode,
       const std::string& client_id,
       v8::Local<v8::Promise::Resolver> waiting,
       v8::Local<v8::Promise::Resolver> released);
  ~Lock();

  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

  const std::u16string& name() const { return name_; }
  Mode mode() const { return mode_; }
  const std::string& client_id() const { return client_id_; }
  Environment* env() const { return env_; }

  bool is_stolen() const { return stolen_; }
  void mark_stolen() { stolen_ = true; }

  v8::Local<v8::Promise::Resolver> waiting_promise() {
    return waiting_promise_.Get(env_->isolate());
  }
  v8::Local<v8::Promise::Resolver> released_promise() {
    return released_promise_.Get(env_->isolate());
  }

 private:
  Environment* env_;
  std::u16string name_;
  Mode mode_;
  std::string client_id_;
  bool stolen_ = false;
  v8::Global<v8::Promise::Resolver> waiting_promise_;
  v8::Global<v8::Promise::Resolver> released_promise_;
};

class LockRequest final {
 public:
  LockRequest(Environment* env,
              v8::Local<v8::Promise::Resolver> waiting,
              v8::Local<v8::Promise::Resolver> released,
              v8::Local<v8::Function> callback,
              const std::u16string& name,
              Lock::Mode mode,
              const std::string& client_id,
              bool steal,
              bool if_available);
  ~LockRequest();

  LockRequest(const LockRequest&) = delete;
  LockRequest& operator=(const LockRequest&) = delete;

  const std::u16string& name() const { return name_; }
  Lock::Mode mode() const { return mode_; }
  const std::string& client_id() const { return client_id_; }
  bool steal() const { return steal_; }
  bool if_available() const { return if_available_; }
  Environment* env() const { return env_; }

  v8::Local<v8::Promise::Resolver> waiting_promise() {
    return waiting_promise_.Get(env_->isolate());
  }
  v8::Local<v8::Promise::Resolver> released_promise() {
    return released_promise_.Get(env_->isolate());
  }
  v8::Local<v8::Function> callback() { return callback_.Get(env_->isolate()); }

 private:
  Environment* env_;
  std::u16string name_;
  Lock::Mode mode_;
  std::string client_id_;
  bool steal_;
  bool if_available_;
  v8::Global<v8::Promise::Resolver> waiting_promise_;
  v8::Global<v8::Promise::Resolver> released_promise_;
  v8::Global<v8::Function> callback_;
};

class LockManager final {
 public:
  static void Request(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Query(const v8::FunctionCallbackInfo<v8::Value>& args);

  void ProcessQueue(Environment* env);
  void CleanupEnvironment(Environment* env);

  static void OnEnvironmentCleanup(void* arg);
  static LockManager* GetCurrent() { return &current_; }
  void ReleaseLockAndProcessQueue(Environment* env,
                                  std::shared_ptr<Lock> lock,
                                  v8::Local<v8::Value> result);

 private:
  LockManager() = default;
  ~LockManager() = default;

  LockManager(const LockManager&) = delete;
  LockManager& operator=(const LockManager&) = delete;

  bool IsGrantable(const LockRequest* req) const;
  void CleanupStolenLocks(Environment* env);
  void ReleaseLock(Lock* lock);
  void WakeEnvironment(Environment* env);

  static LockManager current_;

  mutable Mutex mutex_;
  std::unordered_map<std::u16string, std::deque<std::shared_ptr<Lock>>>
      held_locks_;
  std::deque<std::unique_ptr<LockRequest>> pending_queue_;
  std::unordered_set<Environment*> registered_envs_;
};

}  // namespace locks
}  // namespace worker
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_NODE_LOCKS_H_
