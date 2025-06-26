#include "node_locks.h"

#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "util-inl.h"
#include "v8.h"

namespace node::worker::locks {

using node::errors::TryCatchScope;
using v8::Array;
using v8::Context;
using v8::Exception;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::String;
using v8::Value;

static constexpr const char* kSharedMode = "shared";
static constexpr const char* kExclusiveMode = "exclusive";
static constexpr const char* kLockStolenError = "LOCK_STOLEN";

static MaybeLocal<Object> CreateLockInfoObject(Isolate* isolate,
                                               Local<Context> context,
                                               const std::u16string& name,
                                               Lock::Mode mode,
                                               const std::string& client_id);

Lock::Lock(Environment* env,
           const std::u16string& name,
           Mode mode,
           const std::string& client_id,
           Local<Promise::Resolver> waiting,
           Local<Promise::Resolver> released)
    : env_(env), name_(name), mode_(mode), client_id_(client_id) {
  waiting_promise_.Reset(env_->isolate(), waiting);
  released_promise_.Reset(env_->isolate(), released);
}

LockRequest::LockRequest(Environment* env,
                         Local<Promise::Resolver> waiting,
                         Local<Promise::Resolver> released,
                         Local<Function> callback,
                         const std::u16string& name,
                         Lock::Mode mode,
                         const std::string& client_id,
                         bool steal,
                         bool if_available)
    : env_(env),
      name_(name),
      mode_(mode),
      client_id_(client_id),
      steal_(steal),
      if_available_(if_available) {
  waiting_promise_.Reset(env_->isolate(), waiting);
  released_promise_.Reset(env_->isolate(), released);
  callback_.Reset(env_->isolate(), callback);
}

bool LockManager::IsGrantable(const LockRequest* request) const {
  // Steal requests bypass all normal granting rules
  if (request->steal()) return true;

  auto held_locks_iter = held_locks_.find(request->name());
  // No existing locks for this resource name
  if (held_locks_iter == held_locks_.end()) return true;

  // Exclusive requests cannot coexist with any existing locks
  if (request->mode() == Lock::Mode::Exclusive) return false;

  // For shared requests, check if any existing lock is exclusive
  for (const auto& existing_lock : held_locks_iter->second) {
    if (existing_lock->mode() == Lock::Mode::Exclusive) return false;
  }
  // All existing locks are shared, so this shared request can be granted
  return true;
}

// Called when the user callback promise fulfills
static void OnLockCallbackFulfilled(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  Environment* env = Environment::GetCurrent(info);

  // Extract the LockHolder from V8 External data
  auto* lock_holder =
      static_cast<LockHolder*>(info.Data().As<External>()->Value());
  std::shared_ptr<Lock> lock = lock_holder->lock();
  delete lock_holder;

  // Release the lock and continue processing the queue.
  LockManager::GetCurrent()->ReleaseLockAndProcessQueue(
      env, lock, info[0], false);
}

// Called when the user callback promise rejects
static void OnLockCallbackRejected(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  Environment* env = Environment::GetCurrent(info);

  auto* lock_holder =
      static_cast<LockHolder*>(info.Data().As<External>()->Value());
  std::shared_ptr<Lock> lock = lock_holder->lock();
  delete lock_holder;

  LockManager::GetCurrent()->ReleaseLockAndProcessQueue(
      env, lock, info[0], true);
}

// Called when the promise returned from the user's callback resolves
static void OnIfAvailableFulfill(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  auto* holder = static_cast<v8::Global<v8::Promise::Resolver>*>(
      info.Data().As<External>()->Value());
  USE(holder->Get(info.GetIsolate())
          ->Resolve(info.GetIsolate()->GetCurrentContext(), info[0]));

  delete holder;
}

// Called when the promise returned from the user's callback rejects
static void OnIfAvailableReject(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  auto* holder = static_cast<v8::Global<v8::Promise::Resolver>*>(
      info.Data().As<External>()->Value());
  USE(holder->Get(info.GetIsolate())
          ->Reject(info.GetIsolate()->GetCurrentContext(), info[0]));

  delete holder;
}

void LockManager::CleanupStolenLocks(Environment* env) {
  std::vector<std::u16string> resources_to_clean;

  // Iterate held locks and remove entries that were stolen from other envs.
  {
    Mutex::ScopedLock scoped_lock(mutex_);

    for (auto resource_iter = held_locks_.begin();
         resource_iter != held_locks_.end();
         ++resource_iter) {
      auto& resource_locks = resource_iter->second;
      bool has_stolen_from_other_env = false;

      // Check if this resource has stolen locks from other environments
      for (const auto& lock_ptr : resource_locks) {
        if (lock_ptr->is_stolen() && lock_ptr->env() != env) {
          has_stolen_from_other_env = true;
          break;
        }
      }

      if (has_stolen_from_other_env) {
        resources_to_clean.push_back(resource_iter->first);
      }
    }
  }

  // Clean up resources
  for (const auto& resource_name : resources_to_clean) {
    Mutex::ScopedLock scoped_lock(mutex_);

    auto resource_iter = held_locks_.find(resource_name);
    if (resource_iter != held_locks_.end()) {
      auto& resource_locks = resource_iter->second;

      // Remove stolen locks from other environments
      for (auto lock_iter = resource_locks.begin();
           lock_iter != resource_locks.end();) {
        if ((*lock_iter)->is_stolen() && (*lock_iter)->env() != env) {
          lock_iter = resource_locks.erase(lock_iter);
        } else {
          ++lock_iter;
        }
      }

      if (resource_locks.empty()) {
        held_locks_.erase(resource_iter);
      }
    }
  }
}

/**
 * Web Locks algorithm implementation
 * https://w3c.github.io/web-locks/#algorithms
 */
void LockManager::ProcessQueue(Environment* env) {
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();

  // Remove locks that were stolen from this Environment first
  CleanupStolenLocks(env);

  while (true) {
    std::unique_ptr<LockRequest> grantable_request;
    std::unique_ptr<LockRequest> if_available_request;
    std::unordered_set<Environment*> other_envs_to_wake;

    /**
     * First pass over pending_queue_
     * 1- Build first_seen_for_resource: the oldest pending request
     *   for every resource name we encounter
     * 2- Decide what to do with each entry:
     *     – If it belongs to another Environment, remember that env so we
     *       can wake it later
     *     – For our Environment, pick one of:
     *         * grantable_request  – can be granted now
     *         * if_available_request – user asked for ifAvailable and the
     *           resource is currently busy
     *         * otherwise we skip and keep scanning
     */

    {
      std::unordered_map<std::u16string, LockRequest*> first_seen_for_resource;

      Mutex::ScopedLock scoped_lock(mutex_);
      for (auto queue_iter = pending_queue_.begin();
           queue_iter != pending_queue_.end();
           ++queue_iter) {
        LockRequest* request = queue_iter->get();

        // Collect unique environments to wake up later
        if (request->env() != env) {
          other_envs_to_wake.insert(request->env());
        }

        // During a single pass, the first time we see a resource name is the
        // earliest pending request
        auto& first_for_resource = first_seen_for_resource[request->name()];
        if (first_for_resource == nullptr) {
          first_for_resource = request;  // Mark as first seen for this resource
        }

        bool has_earlier_request_for_same_resource =
            (first_for_resource != request);

        bool should_wait_for_earlier_requests = false;

        if (has_earlier_request_for_same_resource) {
          // Check if this request is compatible with the earliest pending
          // request first_for_resource
          if (request->mode() == Lock::Mode::Exclusive ||
              first_for_resource->mode() == Lock::Mode::Exclusive) {
            // Exclusive locks are incompatible with everything
            should_wait_for_earlier_requests = true;
          }
          // If both are shared, they're compatible and can proceed
        }

        // Only process requests from the current environment
        if (request->env() != env) {
          continue;
        }

        if (should_wait_for_earlier_requests || !IsGrantable(request)) {
          if (request->if_available()) {
            // ifAvailable request when resource not available: grant with null
            if_available_request = std::move(*queue_iter);
            pending_queue_.erase(queue_iter);
            break;
          }
          continue;
        }

        // Found a request that can be granted normally
        grantable_request = std::move(*queue_iter);
        pending_queue_.erase(queue_iter);
        break;
      }
    }

    // Wake each environment only once
    for (Environment* target_env : other_envs_to_wake) {
      WakeEnvironment(target_env);
    }

    /**
     * 1- We call the user callback immediately with `null` to signal
     *    that the lock was not granted - Check wrapCallback function in
     * locks.js 2- Depending on what the callback returns we settle the two
     *    internal promises
     * 3- No lock is added to held_locks_ in this path, so nothing to
     *    remove later
     */
    if (if_available_request) {
      Local<Value> null_arg = Null(isolate);
      Local<Value> callback_result;
      {
        TryCatchScope try_catch_scope(env);
        if (!if_available_request->callback()
                 ->Call(context, Undefined(isolate), 1, &null_arg)
                 .ToLocal(&callback_result)) {
          USE(if_available_request->waiting_promise()->Reject(
              context, try_catch_scope.Exception()));
          USE(if_available_request->released_promise()->Reject(
              context, try_catch_scope.Exception()));
          return;
        }
      }
      if (callback_result->IsPromise()) {
        Local<Promise> p = callback_result.As<Promise>();

        // Use a Global holder so the resolver survives until the promise
        // settles.
        auto* fulf_holder = new v8::Global<v8::Promise::Resolver>(
            isolate, if_available_request->released_promise());
        auto* rej_holder = new v8::Global<v8::Promise::Resolver>(
            isolate, if_available_request->released_promise());

        Local<Function> on_fulfilled;
        Local<Function> on_rejected;
        CHECK(Function::New(context,
                            OnIfAvailableFulfill,
                            External::New(isolate, fulf_holder))
                  .ToLocal(&on_fulfilled));
        CHECK(Function::New(context,
                            OnIfAvailableReject,
                            External::New(isolate, rej_holder))
                  .ToLocal(&on_rejected));

        if (p->Then(context, on_fulfilled, on_rejected).IsEmpty()) {
          // Attaching handlers failed; reject promises and return.
          Local<String> err_str_local =
              String::NewFromUtf8(isolate, "Failed to attach promise handlers")
                  .ToLocalChecked();
          Local<Value> err_val = Exception::Error(err_str_local);

          USE(if_available_request->waiting_promise()->Reject(context,
                                                              err_val));
          USE(if_available_request->released_promise()->Reject(context,
                                                               err_val));

          return;
        }

        // After handlers are attached, resolve waiting_promise with the
        // promise.
        USE(if_available_request->waiting_promise()->Resolve(context, p));

        return;
      }

      // Non-promise callback result: settle both promises right away.
      USE(if_available_request->waiting_promise()->Resolve(context,
                                                           callback_result));
      USE(if_available_request->released_promise()->Resolve(context,
                                                            callback_result));

      return;
    }

    if (!grantable_request) return;

    /**
     * 1- We grant the lock immediately even if other envs hold it
     * 2- All existing locks with the same name are marked stolen, their
     *    released_promise is rejected, and their owners are woken so they
     *    can observe the rejection
     * 3- We remove stolen locks that belong to this env right away; other
     *    envs will clean up in their next queue pass
     */
    if (grantable_request->steal()) {
      std::unordered_set<Environment*> envs_to_notify;

      {
        Mutex::ScopedLock scoped_lock(mutex_);
        auto held_locks_iter = held_locks_.find(grantable_request->name());
        if (held_locks_iter != held_locks_.end()) {
          // Mark existing locks as stolen and collect environments to notify
          for (auto& existing_lock : held_locks_iter->second) {
            existing_lock->mark_stolen();
            envs_to_notify.insert(existing_lock->env());

            // Immediately reject the stolen lock's released_promise
            Local<String> error_string;
            if (!String::NewFromUtf8(isolate, kLockStolenError)
                     .ToLocal(&error_string)) {
              return;
            }
            Local<Value> error = Exception::Error(error_string);

            USE(existing_lock->released_promise()->Reject(context, error));
          }

          // Remove stolen locks from current environment immediately
          for (auto lock_iter = held_locks_iter->second.begin();
               lock_iter != held_locks_iter->second.end();) {
            if ((*lock_iter)->env() == env) {
              lock_iter = held_locks_iter->second.erase(lock_iter);
            } else {
              ++lock_iter;
            }
          }

          if (held_locks_iter->second.empty()) {
            held_locks_.erase(held_locks_iter);
          }
        }
      }

      // Wake other environments
      for (Environment* target_env : envs_to_notify) {
        if (target_env != env) {
          WakeEnvironment(target_env);
        }
      }
    }

    // Create and store the new granted lock
    auto granted_lock =
        std::make_shared<Lock>(env,
                               grantable_request->name(),
                               grantable_request->mode(),
                               grantable_request->client_id(),
                               grantable_request->waiting_promise(),
                               grantable_request->released_promise());
    {
      Mutex::ScopedLock scoped_lock(mutex_);
      held_locks_[grantable_request->name()].push_back(granted_lock);
    }

    // Create and store the new granted lock
    Local<Object> lock_info_obj;
    if (!CreateLockInfoObject(isolate,
                              context,
                              grantable_request->name(),
                              grantable_request->mode(),
                              grantable_request->client_id())
             .ToLocal(&lock_info_obj)) {
      return;
    }

    // Call user callback
    Local<Value> callback_arg = lock_info_obj;
    Local<Value> callback_result;
    {
      TryCatchScope try_catch_scope(env);
      if (!grantable_request->callback()
               ->Call(context, Undefined(isolate), 1, &callback_arg)
               .ToLocal(&callback_result)) {
        USE(grantable_request->waiting_promise()->Reject(
            context, try_catch_scope.Exception()));
        USE(grantable_request->released_promise()->Reject(
            context, try_catch_scope.Exception()));

        return;
      }
    }

    // Allocate a LockHolder on the heap to safely manage the lock's lifetime
    // until the user's callback promise settles.
    auto* fulfill_holder = new LockHolder(granted_lock);
    auto* reject_holder = new LockHolder(granted_lock);
    Local<Function> on_fulfilled_callback;
    Local<Function> on_rejected_callback;

    // Create fulfilled callback first
    if (!Function::New(context,
                       OnLockCallbackFulfilled,
                       External::New(isolate, fulfill_holder))
             .ToLocal(&on_fulfilled_callback)) {
      delete fulfill_holder;
      delete reject_holder;
      return;
    }

    // Create rejected callback second
    if (!Function::New(context,
                       OnLockCallbackRejected,
                       External::New(isolate, reject_holder))
             .ToLocal(&on_rejected_callback)) {
      // fulfill_holder is now owned by the External, don't delete it
      delete reject_holder;
      return;
    }

    // Handle promise chain
    if (callback_result->IsPromise()) {
      Local<Promise> promise = callback_result.As<Promise>();
      if (promise->Then(context, on_fulfilled_callback, on_rejected_callback)
              .IsEmpty()) {
        // Then() failed, reject both promises and return
        Local<String> err_str_local =
            String::NewFromUtf8(isolate, "Failed to attach promise handlers")
                .ToLocalChecked();
        Local<Value> err_val = Exception::Error(err_str_local);

        USE(grantable_request->waiting_promise()->Reject(context, err_val));
        USE(grantable_request->released_promise()->Reject(context, err_val));

        return;
      }

      // Lock granted: waiting_promise resolves now with the promise returned
      // by the callback; on_fulfilled/on_rejected will release the lock when
      // that promise settles.
      USE(grantable_request->waiting_promise()->Resolve(context,
                                                        callback_result));
    } else {
      USE(grantable_request->waiting_promise()->Resolve(context,
                                                        callback_result));
      Local<Value> promise_args[] = {callback_result};
      if (on_fulfilled_callback
              ->Call(context, Undefined(isolate), 1, promise_args)
              .IsEmpty()) {
        // Callback threw an error, handle it like a rejected promise
        // The error is already propagated through the TryCatch in the
        // callback
        return;
      }
      delete reject_holder;
    }
  }
}

/**
 * name        : string   – resource identifier
 * clientId    : string   – client identifier
 * mode        : string   – lock mode
 * steal       : boolean  – whether to steal existing locks
 * ifAvailable : boolean  – only grant if immediately available
 * callback    : Function - JS callback
 */
void LockManager::Request(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env->context();

  CHECK_EQ(args.Length(), 6);
  CHECK(args[0]->IsString());    // name
  CHECK(args[1]->IsString());    // clientId
  CHECK(args[2]->IsString());    // mode
  CHECK(args[3]->IsBoolean());   // steal
  CHECK(args[4]->IsBoolean());   // ifAvailable
  CHECK(args[5]->IsFunction());  // callback

  Local<String> resource_name_str = args[0].As<String>();
  TwoByteValue resource_name_utf16(isolate, resource_name_str);
  std::u16string resource_name(
      reinterpret_cast<const char16_t*>(*resource_name_utf16),
      resource_name_utf16.length());
  String::Utf8Value client_id_utf8(isolate, args[1]);
  std::string client_id(*client_id_utf8);
  String::Utf8Value mode_utf8(isolate, args[2]);
  std::string mode_str(*mode_utf8);
  bool steal = args[3]->BooleanValue(isolate);
  bool if_available = args[4]->BooleanValue(isolate);
  Local<Function> callback = args[5].As<Function>();

  Lock::Mode lock_mode =
      mode_str == kSharedMode ? Lock::Mode::Shared : Lock::Mode::Exclusive;

  Local<Promise::Resolver> waiting_promise;
  Local<Promise::Resolver> released_promise;

  if (!Promise::Resolver::New(context).ToLocal(&waiting_promise) ||
      !Promise::Resolver::New(context).ToLocal(&released_promise)) {
    return;
  }

  // Mark both internal promises as handled to prevent unhandled rejection
  // warnings
  waiting_promise->GetPromise()->MarkAsHandled();
  released_promise->GetPromise()->MarkAsHandled();

  LockManager* manager = GetCurrent();
  {
    Mutex::ScopedLock scoped_lock(manager->mutex_);

    // Register cleanup hook for the environment only once
    if (manager->registered_envs_.insert(env).second) {
      env->AddCleanupHook(LockManager::OnEnvironmentCleanup, env);
    }

    auto lock_request = std::make_unique<LockRequest>(env,
                                                      waiting_promise,
                                                      released_promise,
                                                      callback,
                                                      resource_name,
                                                      lock_mode,
                                                      client_id,
                                                      steal,
                                                      if_available);
    // Steal requests get priority by going to front of queue
    if (steal) {
      manager->pending_queue_.emplace_front(std::move(lock_request));
    } else {
      manager->pending_queue_.push_back(std::move(lock_request));
    }
  }

  manager->ProcessQueue(env);

  args.GetReturnValue().Set(released_promise->GetPromise());
}

void LockManager::Query(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env->context();

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) {
    return;
  }

  // Always set the return value first so Javascript gets a promise
  args.GetReturnValue().Set(resolver->GetPromise());

  Local<Object> result = Object::New(isolate);
  Local<Array> held_list = Array::New(isolate);
  Local<Array> pending_list = Array::New(isolate);
  LockManager* manager = GetCurrent();

  {
    Mutex::ScopedLock scoped_lock(manager->mutex_);

    uint32_t index = 0;
    Local<Object> lock_info;
    for (const auto& resource_entry : manager->held_locks_) {
      for (const auto& held_lock : resource_entry.second) {
        if (held_lock->env() == env) {
          if (!CreateLockInfoObject(isolate,
                                    context,
                                    held_lock->name(),
                                    held_lock->mode(),
                                    held_lock->client_id())
                   .ToLocal(&lock_info)) {
            THROW_ERR_OPERATION_FAILED(env,
                                       "Failed to create lock info object");
            return;
          }
          if (held_list->Set(context, index++, lock_info).IsNothing()) {
            THROW_ERR_OPERATION_FAILED(env, "Failed to build held locks array");
            return;
          }
        }
      }
    }

    index = 0;
    for (const auto& pending_request : manager->pending_queue_) {
      if (pending_request->env() == env) {
        if (!CreateLockInfoObject(isolate,
                                  context,
                                  pending_request->name(),
                                  pending_request->mode(),
                                  pending_request->client_id())
                 .ToLocal(&lock_info)) {
          THROW_ERR_OPERATION_FAILED(env, "Failed to create lock info object");
          return;
        }
        if (pending_list->Set(context, index++, lock_info).IsNothing()) {
          THROW_ERR_OPERATION_FAILED(env,
                                     "Failed to build pending locks array");
          return;
        }
      }
    }
  }

  if (result->Set(context, env->held_string(), held_list).IsNothing() ||
      result->Set(context, env->pending_string(), pending_list).IsNothing()) {
    THROW_ERR_OPERATION_FAILED(env, "Failed to build query result object");
    return;
  }

  USE(resolver->Resolve(context, result));
}

// Runs after the user callback (or its returned promise) settles.
void LockManager::ReleaseLockAndProcessQueue(Environment* env,
                                             std::shared_ptr<Lock> lock,
                                             Local<Value> callback_result,
                                             bool was_rejected) {
  {
    Mutex::ScopedLock scoped_lock(mutex_);
    ReleaseLock(lock.get());
  }

  Local<Context> context = env->context();

  // For stolen locks, the released_promise was already rejected when marked as
  // stolen.
  if (!lock->is_stolen()) {
    if (was_rejected) {
      // Propagate rejection from the user callback
      USE(lock->released_promise()->Reject(context, callback_result));
    } else {
      // Propagate fulfilment
      USE(lock->released_promise()->Resolve(context, callback_result));
    }
  }

  ProcessQueue(env);
}

// Remove a lock from held_locks_ when it's no longer needed
void LockManager::ReleaseLock(Lock* lock) {
  const std::u16string& resource_name = lock->name();
  auto resource_iter = held_locks_.find(resource_name);
  if (resource_iter == held_locks_.end()) return;

  auto& resource_locks = resource_iter->second;
  for (auto lock_iter = resource_locks.begin();
       lock_iter != resource_locks.end();
       ++lock_iter) {
    if (lock_iter->get() == lock) {
      resource_locks.erase(lock_iter);
      if (resource_locks.empty()) held_locks_.erase(resource_iter);
      break;
    }
  }
}

// Wakeup of target Environment's event loop
void LockManager::WakeEnvironment(Environment* target_env) {
  if (target_env == nullptr || target_env->is_stopping()) return;

  // Schedule ProcessQueue in the target Environment on its event loop.
  target_env->SetImmediateThreadsafe([](Environment* env_to_wake) {
    if (env_to_wake != nullptr && !env_to_wake->is_stopping()) {
      LockManager::GetCurrent()->ProcessQueue(env_to_wake);
    }
  });
}

// Remove all held locks and pending requests that belong to an Environment
// that is being destroyed
void LockManager::CleanupEnvironment(Environment* env_to_cleanup) {
  Mutex::ScopedLock scoped_lock(mutex_);

  // Remove every held lock that belongs to this Environment.
  for (auto resource_iter = held_locks_.begin();
       resource_iter != held_locks_.end();) {
    auto& resource_locks = resource_iter->second;
    for (auto lock_iter = resource_locks.begin();
         lock_iter != resource_locks.end();) {
      if ((*lock_iter)->env() == env_to_cleanup) {
        lock_iter = resource_locks.erase(lock_iter);
      } else {
        ++lock_iter;
      }
    }
    if (resource_locks.empty()) {
      resource_iter = held_locks_.erase(resource_iter);
    } else {
      ++resource_iter;
    }
  }

  // Remove every pending request submitted by this Environment.
  for (auto request_iter = pending_queue_.begin();
       request_iter != pending_queue_.end();) {
    if ((*request_iter)->env() == env_to_cleanup) {
      request_iter = pending_queue_.erase(request_iter);
    } else {
      ++request_iter;
    }
  }

  // Finally, remove it from registered_envs_
  registered_envs_.erase(env_to_cleanup);
}

// Cleanup hook wrapper
void LockManager::OnEnvironmentCleanup(void* arg) {
  Environment* env = static_cast<Environment*>(arg);
  LockManager::GetCurrent()->CleanupEnvironment(env);
}

static MaybeLocal<Object> CreateLockInfoObject(Isolate* isolate,
                                               Local<Context> context,
                                               const std::u16string& name,
                                               Lock::Mode mode,
                                               const std::string& client_id) {
  Local<Object> obj = Object::New(isolate);
  Environment* env = Environment::GetCurrent(context);

  // TODO(ilyasshabi): Add ToV8Value that directly accepts std::u16string
  // so we can avoid the manual String::NewFromTwoByte()
  Local<String> name_string;
  if (!String::NewFromTwoByte(isolate,
                              reinterpret_cast<const uint16_t*>(name.data()),
                              v8::NewStringType::kNormal,
                              static_cast<int>(name.length()))
           .ToLocal(&name_string)) {
    return MaybeLocal<Object>();
  }
  if (obj->Set(context, env->name_string(), name_string).IsNothing()) {
    return MaybeLocal<Object>();
  }

  Local<String> mode_string;
  if (!String::NewFromUtf8(
           isolate,
           mode == Lock::Mode::Exclusive ? kExclusiveMode : kSharedMode)
           .ToLocal(&mode_string)) {
    return MaybeLocal<Object>();
  }
  if (obj->Set(context, env->mode_string(), mode_string).IsNothing()) {
    return MaybeLocal<Object>();
  }

  Local<String> client_id_string;
  if (!String::NewFromUtf8(isolate, client_id.c_str())
           .ToLocal(&client_id_string)) {
    return MaybeLocal<Object>();
  }
  if (obj->Set(context, env->clientId_string(), client_id_string).IsNothing()) {
    return MaybeLocal<Object>();
  }

  return obj;
}

LockManager LockManager::current_;

void CreatePerIsolateProperties(IsolateData* isolate_data,
                                Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  SetMethod(isolate, target, "request", LockManager::Request);
  SetMethod(isolate, target, "query", LockManager::Query);

  Local<String> shared_mode;
  if (String::NewFromUtf8(isolate, kSharedMode).ToLocal(&shared_mode)) {
    target->Set(FIXED_ONE_BYTE_STRING(isolate, "LOCK_MODE_SHARED"),
                shared_mode);
  }
  Local<String> exclusive_mode;
  if (String::NewFromUtf8(isolate, kExclusiveMode).ToLocal(&exclusive_mode)) {
    target->Set(FIXED_ONE_BYTE_STRING(isolate, "LOCK_MODE_EXCLUSIVE"),
                exclusive_mode);
  }
  Local<String> stolen_error;
  if (String::NewFromUtf8(isolate, kLockStolenError).ToLocal(&stolen_error)) {
    target->Set(FIXED_ONE_BYTE_STRING(isolate, "LOCK_STOLEN_ERROR"),
                stolen_error);
  }
}

void CreatePerContextProperties(Local<Object> target,
                                Local<Value> unused,
                                Local<Context> context,
                                void* priv) {}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(LockManager::Request);
  registry->Register(LockManager::Query);
  registry->Register(OnLockCallbackFulfilled);
  registry->Register(OnLockCallbackRejected);
  registry->Register(OnIfAvailableFulfill);
  registry->Register(OnIfAvailableReject);
}

}  // namespace node::worker::locks

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    locks, node::worker::locks::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(locks,
                              node::worker::locks::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(locks,
                                node::worker::locks::RegisterExternalReferences)
