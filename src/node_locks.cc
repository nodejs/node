#include "node_locks.h"
#include <algorithm>
#include <memory>
#include <utility>
#include "env-inl.h"
#include "node.h"
#include "node_internals.h"
#include "v8.h"

namespace node {
namespace worker {

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::Promise;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

LockManager LockManager::current_;

void LockManager::ReleaseLock(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  LockManager* manager = LockManager::GetCurrent();

  Lock* lock = reinterpret_cast<Lock*>(args.Data().As<External>()->Value());

  Mutex::ScopedLock(manager->work_mutex_);

  for (auto const& maybe : manager->held_locks_) {
    if (maybe.get() == lock) {
      manager->held_locks_.erase(
          std::remove(
              manager->held_locks_.begin(), manager->held_locks_.end(), maybe),
          manager->held_locks_.end());
      break;
    }
  }

  manager->ProcessQueue(env);
}

void LockManager::ProcessQueue(Environment* env) {
  // For each request of queue:
  for (auto const& request : pending_requests_) {
    // If request is grantable, then run these steps:
    if (IsGrantable(request.get())) {
      // Let waiting be a new Promise.
      Local<Promise::Resolver> waiting =
        Promise::Resolver::New(env->context()).ToLocalChecked();

      Lock* lock = new Lock(
          env, request->mode(), request->name(), request->promise(), waiting);

      // Let r be the result of invoking callback with a
      // new Lock object associated with lock as the only argument.

      Local<Value> args[] = {
          String::NewFromUtf8(env->isolate(), lock->name().c_str()),
          Number::New(env->isolate(), lock->mode()),
          waiting->GetPromise(),
          Function::New(env->context(),
                        LockManager::ReleaseLock,
                        External::New(env->isolate(), lock))
              .ToLocalChecked(),
      };

      TryCatch try_catch(env->isolate());
      Local<Value> r;
      if (request->callback()
              ->Call(env->context(), Undefined(env->isolate()), 4, args)
              .ToLocal(&r)) {
        waiting->Resolve(env->context(), r).ToChecked();
      } else {
        waiting->Reject(env->context(), try_catch.Exception()).ToChecked();
      }

      // Remove request from queue.
      pending_requests_.erase(
          std::remove(
              pending_requests_.begin(), pending_requests_.end(), request),
          pending_requests_.end());

      // Append lock to origin’s held lock set.
      held_locks_.emplace_back(lock);
    }
  }
}

bool LockManager::IsGrantable(const LockRequest* request) {
  // If mode is "exclusive", then return true if all of the
  // following conditions are true, and false otherwise:
  if (request->mode() == Lock::Mode::kExclusive) {
    // No lock in held has a name that equals name
    for (auto const& lock : held_locks_) {
      if (lock->name() == request->name()) {
        return false;
      }
    }

    // No lock request in queue earlier than request has a name that equals
    // name.
    for (auto const& req : pending_requests_) {
      if (request == req.get()) {
        break;
      }
      if (req->name() == request->name()) {
        return false;
      }
    }
    return true;
  } else {
    // Otherwise, mode is "shared"; return true if all of the
    // following conditions are true, and false otherwise:

    // No lock in held has mode "exclusive" and has a name that equals name.
    for (auto const& lock : held_locks_) {
      if (lock->name() == request->name() &&
          lock->mode() == Lock::Mode::kExclusive) {
        return false;
      }
    }

    // No lock request in queue earlier than request has a
    // mode "exclusive" and name that equals name
    for (auto const& req : pending_requests_) {
      if (request == req.get()) {
        break;
      }
      if (req->name() == request->name() &&
          req->mode() == Lock::Mode::kExclusive) {
        return false;
      }
    }
    return true;
  }
}

// https://wicg.github.io/web-locks/#algorithm-request-lock
// promise, callback, name, mode, ifAvailable, steal
void LockManager::Request(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  LockManager* manager = LockManager::GetCurrent();

  CHECK_EQ(args.Length(), 6);

  CHECK(args[0]->IsPromise());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsString());
  CHECK(args[3]->IsNumber());
  CHECK(args[4]->IsBoolean());
  CHECK(args[5]->IsBoolean());

  Local<Promise::Resolver> promise = args[0].As<Promise::Resolver>();
  Local<Function> callback = args[1].As<Function>();

  String::Utf8Value name_utf8(env->isolate(), args[2]);
  std::string name(*name_utf8);

  Lock::Mode mode = static_cast<Lock::Mode>(args[3].As<Number>()->Int32Value());

  bool if_available = args[4]->IsTrue();
  bool steal = args[5]->IsTrue();

  Mutex::ScopedLock(manager->work_mutex_);

  LockRequest* request = new LockRequest(env, promise, callback, name, mode);

  if (steal) {
    for (auto const& lock : manager->held_locks_) {
      if (lock->name() == name) {
        // Reject lock’s released promise with an "AbortError" DOMException.
        Local<Function> domexception_ctor = env->domexception_function();
        CHECK(!domexception_ctor.IsEmpty());
        Local<Value> argv[] = {
            FIXED_ONE_BYTE_STRING(env->isolate(),
                                  "Lock request could not be fulfilled"),
            FIXED_ONE_BYTE_STRING(env->isolate(), "AbortError"),
        };
        Local<Value> exception =
            domexception_ctor->NewInstance(env->context(), 2, argv)
                .ToLocalChecked();
        lock->released_promise()->Reject(env->context(), exception).ToChecked();

        // Remove lock from held lock set
        manager->held_locks_.erase(
            std::remove(
                manager->held_locks_.begin(), manager->held_locks_.end(), lock),
            manager->held_locks_.end());
      }
    }
    // Prepend request in origin’s lock request queue.
    manager->pending_requests_.emplace_front(request);
  } else {
    // Otherwise, run these steps:
    // If ifAvailable is true and request is not
    // grantable, then run these steps:
    if (if_available && !manager->IsGrantable(request)) {
      // Let r be the result of invoking callback with null as the only
      // argument. Note that r could be a regular completion, an abrupt
      // completion, or an unresolved Promise.
      TryCatch try_catch(env->isolate());
      Local<Value> r;
      Local<Value> null = Null(env->isolate());
      if (callback->Call(env->context(), Undefined(env->isolate()), 1, &null)
              .ToLocal(&r)) {
        // Resolve promise with r and abort these steps.
        promise->Resolve(env->context(), r).ToChecked();
        return;
      } else {
        promise->Reject(env->context(), try_catch.Exception()).ToChecked();
      }
    }
    // Enqueue request in origin's lock request queue;
    manager->pending_requests_.emplace_back(request);
  }

  manager->ProcessQueue(env);
}

// https://wicg.github.io/web-locks/#snapshot-the-lock-state
void LockManager::Snapshot(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  LockManager* manager = LockManager::GetCurrent();

  Mutex::ScopedLock(manager->work_mutex_);

  Local<Object> obj = Object::New(isolate);

  {
    Local<Array> pending =
        Array::New(isolate, manager->pending_requests_.size());

    for (uint32_t i = 0; i < manager->pending_requests_.size(); i += 1) {
      LockRequest* request = manager->pending_requests_[i].get();

      Local<Object> obj = Object::New(isolate);
      obj->Set(context,
               env->name_string(),
               String::NewFromUtf8(isolate, request->name().c_str()))
          .ToChecked();
      obj->Set(context,
               env->mode_string(),
               request->mode() == Lock::Mode::kShared ? env->shared_string()
                                                      : env->exclusive_string())
          .ToChecked();

      pending->Set(context, i, obj).ToChecked();
    }

    obj->Set(context, env->pending_string(), pending).ToChecked();
  }

  {
    Local<Array> held = Array::New(isolate, manager->held_locks_.size());

    for (uint32_t i = 0; i < manager->held_locks_.size(); i += 1) {
      Lock* lock = manager->held_locks_[i].get();

      Local<Object> obj = Object::New(isolate);
      obj->Set(context,
               env->name_string(),
               String::NewFromUtf8(isolate, lock->name().c_str()))
          .ToChecked();
      obj->Set(context,
               env->mode_string(),
               lock->mode() == Lock::Mode::kShared ? env->shared_string()
                                                   : env->exclusive_string())
          .ToChecked();

      held->Set(context, i, obj).ToChecked();
    }

    obj->Set(context, env->held_string(), held).ToChecked();
  }

  args.GetReturnValue().Set(obj);
}

void InitializeLocks(Local<Object> target,
                     Local<Value> unused,
                     Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "snapshot", LockManager::Snapshot);
  env->SetMethod(target, "request", LockManager::Request);
}

}  // namespace worker
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(locks, node::worker::InitializeLocks)
