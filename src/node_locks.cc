#include "node_locks.h"

#include "base_object-inl.h"
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
using v8::DictionaryTemplate;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::PropertyAttribute;
using v8::Value;

// Reject two promises and return `false` on failure.
static bool RejectBoth(Local<Context> ctx,
                       Local<Promise::Resolver> first,
                       Local<Promise::Resolver> second,
                       Local<Value> reason) {
  return first->Reject(ctx, reason).IsJust() &&
         second->Reject(ctx, reason).IsJust();
}

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

void Lock::MemoryInfo(node::MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("name", name_.size());
  tracker->TrackField("client_id", client_id_);
  tracker->TrackField("waiting_promise", waiting_promise_);
  tracker->TrackField("released_promise", released_promise_);
}

LockRequest::LockRequest(Environment* env,
                         Local<Promise::Resolver> waiting,
                         Local<Promise::Resolver> released,
                         Local<Function> callback,
                         const std::u16string& name,
                         Lock::Mode mode,
                         std::string client_id,
                         bool steal,
                         bool if_available)
    : env_(env),
      name_(name),
      mode_(mode),
      client_id_(std::move(client_id)),
      steal_(steal),
      if_available_(if_available) {
  waiting_promise_.Reset(env_->isolate(), waiting);
  released_promise_.Reset(env_->isolate(), released);
  callback_.Reset(env_->isolate(), callback);
}

Local<DictionaryTemplate> GetLockInfoTemplate(Environment* env) {
  auto tmpl = env->lock_info_template();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view names[] = {
        "name",
        "mode",
        "clientId",
    };
    tmpl = DictionaryTemplate::New(env->isolate(), names);
    env->set_lock_info_template(tmpl);
  }
  return tmpl;
}

// The request here can be either a Lock or a LockRequest.
static MaybeLocal<Object> CreateLockInfoObject(Environment* env,
                                               const auto& request) {
  auto tmpl = GetLockInfoTemplate(env);
  MaybeLocal<Value> values[] = {
      ToV8Value(env->context(), request.name()),
      request.mode() == Lock::Mode::Exclusive ? env->exclusive_string()
                                              : env->shared_string(),
      ToV8Value(env->context(), request.client_id()),
  };

  return NewDictionaryInstance(env->context(), tmpl, values);
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
static void OnLockCallbackFulfilled(const FunctionCallbackInfo<Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  Environment* env = Environment::GetCurrent(info);

  BaseObjectPtr<LockHolder> lock_holder{
      BaseObject::FromJSObject<LockHolder>(info.Data())};
  std::shared_ptr<Lock> lock = lock_holder->lock();

  // Release the lock and continue processing the queue.
  LockManager::GetCurrent()->ReleaseLockAndProcessQueue(
      env, lock, info[0], false);
}

// Called when the user callback promise rejects
static void OnLockCallbackRejected(const FunctionCallbackInfo<Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  Environment* env = Environment::GetCurrent(info);

  BaseObjectPtr<LockHolder> lock_holder{
      BaseObject::FromJSObject<LockHolder>(info.Data())};
  std::shared_ptr<Lock> lock = lock_holder->lock();

  LockManager::GetCurrent()->ReleaseLockAndProcessQueue(
      env, lock, info[0], true);
}

// Called when the promise returned from the user's callback resolves
static void OnIfAvailableFulfill(const FunctionCallbackInfo<Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  USE(info.Data().As<Promise::Resolver>()->Resolve(
      info.GetIsolate()->GetCurrentContext(), info[0]));
}

// Called when the promise returned from the user's callback rejects
static void OnIfAvailableReject(const FunctionCallbackInfo<Value>& info) {
  USE(info.Data().As<Promise::Resolver>()->Reject(
      info.GetIsolate()->GetCurrentContext(), info[0]));
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
          // We don't really need to check the return value here since
          // we're returning early in either case.
          USE(RejectBoth(context,
                         if_available_request->waiting_promise(),
                         if_available_request->released_promise(),
                         try_catch_scope.Exception()));
          return;
        }
      }
      if (callback_result->IsPromise()) {
        Local<Promise> p = callback_result.As<Promise>();

        Local<Function> on_fulfilled;
        Local<Function> on_rejected;
        CHECK(Function::New(context,
                            OnIfAvailableFulfill,
                            if_available_request->released_promise())
                  .ToLocal(&on_fulfilled));
        CHECK(Function::New(context,
                            OnIfAvailableReject,
                            if_available_request->released_promise())
                  .ToLocal(&on_rejected));

        {
          TryCatchScope try_catch_scope(env);
          if (p->Then(context, on_fulfilled, on_rejected).IsEmpty()) {
            if (!try_catch_scope.CanContinue()) return;

            Local<Value> err_val;
            if (try_catch_scope.HasCaught() &&
                !try_catch_scope.Exception().IsEmpty()) {
              err_val = try_catch_scope.Exception();
            } else {
              err_val = Exception::Error(FIXED_ONE_BYTE_STRING(
                  isolate, "Failed to attach promise handlers"));
            }

            USE(RejectBoth(context,
                           if_available_request->waiting_promise(),
                           if_available_request->released_promise(),
                           err_val));
            return;
          }
        }

        // After handlers are attached, resolve waiting_promise with the
        // promise.
        USE(if_available_request->waiting_promise()
                ->Resolve(context, p)
                .IsNothing());
        return;
      }

      // Non-promise callback result: settle both promises right away.
      if (if_available_request->waiting_promise()
              ->Resolve(context, callback_result)
              .IsNothing()) {
        return;
      }
      USE(if_available_request->released_promise()
              ->Resolve(context, callback_result)
              .IsNothing());
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

            Local<Value> error =
                Exception::Error(FIXED_ONE_BYTE_STRING(isolate, "LOCK_STOLEN"));

            if (existing_lock->released_promise()
                    ->Reject(context, error)
                    .IsNothing())
              return;
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
    Local<Object> lock_info;
    if (!CreateLockInfoObject(env, *grantable_request).ToLocal(&lock_info)) {
      return;
    }

    // Call user callback
    Local<Value> callback_arg = lock_info;
    Local<Value> callback_result;
    {
      TryCatchScope try_catch_scope(env);
      if (!grantable_request->callback()
               ->Call(context, Undefined(isolate), 1, &callback_arg)
               .ToLocal(&callback_result)) {
        // We don't really need to check the return value here since
        // we're returning early in either case.
        USE(RejectBoth(context,
                       grantable_request->waiting_promise(),
                       grantable_request->released_promise(),
                       try_catch_scope.Exception()));
        return;
      }
    }

    // Create LockHolder BaseObjects to safely manage the lock's lifetime
    // until the user's callback promise settles.
    auto lock_resolve_holder = LockHolder::Create(env, granted_lock);
    auto lock_reject_holder = LockHolder::Create(env, granted_lock);
    Local<Function> on_fulfilled_callback;
    Local<Function> on_rejected_callback;

    // Create fulfilled callback first
    if (!Function::New(
             context, OnLockCallbackFulfilled, lock_resolve_holder->object())
             .ToLocal(&on_fulfilled_callback)) {
      return;
    }

    // Create rejected callback second
    if (!Function::New(
             context, OnLockCallbackRejected, lock_reject_holder->object())
             .ToLocal(&on_rejected_callback)) {
      return;
    }

    // Handle promise chain
    if (callback_result->IsPromise()) {
      Local<Promise> promise = callback_result.As<Promise>();
      {
        TryCatchScope try_catch_scope(env);
        if (promise->Then(context, on_fulfilled_callback, on_rejected_callback)
                .IsEmpty()) {
          if (!try_catch_scope.CanContinue()) return;

          Local<Value> err_val;
          if (try_catch_scope.HasCaught() &&
              !try_catch_scope.Exception().IsEmpty()) {
            err_val = try_catch_scope.Exception();
          } else {
            err_val = Exception::Error(FIXED_ONE_BYTE_STRING(
                isolate, "Failed to attach promise handlers"));
          }

          USE(RejectBoth(context,
                         grantable_request->waiting_promise(),
                         grantable_request->released_promise(),
                         err_val));
          return;
        }
      }

      // Lock granted: waiting_promise resolves now with the promise returned
      // by the callback; on_fulfilled/on_rejected will release the lock when
      // that promise settles.
      if (grantable_request->waiting_promise()
              ->Resolve(context, callback_result)
              .IsNothing()) {
        return;
      }
    } else {
      if (grantable_request->waiting_promise()
              ->Resolve(context, callback_result)
              .IsNothing()) {
        return;
      }
      Local<Value> promise_args[] = {callback_result};
      if (on_fulfilled_callback
              ->Call(context, Undefined(isolate), 1, promise_args)
              .IsEmpty()) {
        // Callback threw an error, handle it like a rejected promise
        // The error is already propagated through the TryCatch in the
        // callback
        return;
      }
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

  TwoByteValue resource_name(isolate, args[0]);
  Utf8Value client_id(isolate, args[1]);
  Utf8Value mode(isolate, args[2]);
  bool steal = args[3]->BooleanValue(isolate);
  bool if_available = args[4]->BooleanValue(isolate);
  Local<Function> callback = args[5].As<Function>();

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

    auto lock_request = std::make_unique<LockRequest>(
        env,
        waiting_promise,
        released_promise,
        callback,
        resource_name.ToU16String(),
        mode.ToStringView() == "shared" ? Lock::Mode::Shared
                                        : Lock::Mode::Exclusive,
        client_id.ToString(),
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

  LocalVector<Value> held_list(isolate);
  LocalVector<Value> pending_list(isolate);
  LockManager* manager = GetCurrent();

  {
    Mutex::ScopedLock scoped_lock(manager->mutex_);

    Local<Object> lock_info;
    for (const auto& resource_entry : manager->held_locks_) {
      for (const auto& held_lock : resource_entry.second) {
        if (held_lock->env() == env) {
          if (!CreateLockInfoObject(env, *held_lock).ToLocal(&lock_info)) {
            // There should already be a pending exception scheduled.
            return;
          }
          held_list.push_back(lock_info);
        }
      }
    }

    for (const auto& pending_request : manager->pending_queue_) {
      if (pending_request->env() == env) {
        if (!CreateLockInfoObject(env, *pending_request).ToLocal(&lock_info)) {
          // There should already be a pending exception scheduled.
          return;
        }
        pending_list.push_back(lock_info);
      }
    }
  }

  auto tmpl = env->lock_query_template();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view names[] = {
        "held",
        "pending",
    };
    tmpl = DictionaryTemplate::New(isolate, names);
    env->set_lock_query_template(tmpl);
  }

  MaybeLocal<Value> values[] = {
      Array::New(isolate, held_list.data(), held_list.size()),
      Array::New(isolate, pending_list.data(), pending_list.size()),
  };

  Local<Object> result;
  if (NewDictionaryInstance(env->context(), tmpl, values).ToLocal(&result)) {
    // There's no reason to check IsNothing here since we're just returning.
    USE(resolver->Resolve(context, result));
  }
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
      if (lock->released_promise()
              ->Reject(context, callback_result)
              .IsNothing())
        return;
    } else {
      // Propagate fulfilment
      if (lock->released_promise()
              ->Resolve(context, callback_result)
              .IsNothing())
        return;
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

LockManager LockManager::current_;

void CreatePerIsolateProperties(IsolateData* isolate_data,
                                Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  SetMethod(isolate, target, "request", LockManager::Request);
  SetMethod(isolate, target, "query", LockManager::Query);

  PropertyAttribute read_only = static_cast<PropertyAttribute>(
      PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete);
  target->Set(FIXED_ONE_BYTE_STRING(isolate, "LOCK_MODE_SHARED"),
              FIXED_ONE_BYTE_STRING(isolate, "shared"),
              read_only);
  target->Set(FIXED_ONE_BYTE_STRING(isolate, "LOCK_MODE_EXCLUSIVE"),
              FIXED_ONE_BYTE_STRING(isolate, "exclusive"),
              read_only);
  target->Set(FIXED_ONE_BYTE_STRING(isolate, "LOCK_STOLEN_ERROR"),
              FIXED_ONE_BYTE_STRING(isolate, "LOCK_STOLEN"),
              read_only);
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

void LockHolder::MemoryInfo(node::MemoryTracker* tracker) const {
  tracker->TrackField("lock", lock_);
}

BaseObjectPtr<LockHolder> LockHolder::Create(Environment* env,
                                             std::shared_ptr<Lock> lock) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<LockHolder>(env, obj, std::move(lock));
}

Local<FunctionTemplate> LockHolder::GetConstructorTemplate(Environment* env) {
  IsolateData* isolate_data = env->isolate_data();
  Local<FunctionTemplate> tmpl =
      isolate_data->lock_holder_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = isolate_data->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "LockHolder"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        LockHolder::kInternalFieldCount);
    isolate_data->set_lock_holder_constructor_template(tmpl);
  }
  return tmpl;
}

}  // namespace node::worker::locks

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    locks, node::worker::locks::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(locks,
                              node::worker::locks::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(locks,
                                node::worker::locks::RegisterExternalReferences)
