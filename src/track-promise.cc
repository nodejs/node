#include "track-promise.h"
#include "env.h"
#include "env-inl.h"
#include "node_internals.h"

namespace node {

using v8::Exception;
using v8::Function;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::Object;
using v8::Persistent;
using v8::Value;
using v8::WeakCallbackInfo;
using v8::WeakCallbackType;

size_t v8ObjectHash::operator() (Persistent<Object>* handle) const {
  Local<Object> object = handle->Get(env->isolate());
  return static_cast<size_t>(object->GetIdentityHash());
}

bool v8ObjectEquals::operator() (Persistent<Object>* lhs,
                                 Persistent<Object>* rhs) const {
  Local<Object> lhs_ = lhs->Get(env->isolate());
  Local<Object> rhs_ = rhs->Get(env->isolate());
  return lhs_->StrictEquals(rhs_);
}

void PromiseTracker::TrackPromise(Local<Object> promise) {
  Persistent<Object>* p = new Persistent<Object>(env_->isolate(), promise);
  set_.insert(p);
  p->SetWeak(p, WeakCallback, WeakCallbackType::kFinalizer);
  p->MarkIndependent();
}

void PromiseTracker::UntrackPromise(Local<Object> promise) {
  Persistent<Object> p(env_->isolate(), promise);
  auto it = set_.find(&p);
  CHECK_NE(it, set_.end());
  (*it)->Reset();
  set_.erase(it);
  delete *it;
  p.Reset();
}

bool PromiseTracker::HasPromise(Local<Object> promise) {
  Persistent<Object> p(env_->isolate(), promise);
  bool found = set_.find(&p) != set_.end();
  p.Reset();
  return found;
}

void PromiseTracker::WeakCallback(
    const v8::WeakCallbackInfo<v8::Persistent<v8::Object>>& data) {
  Environment* env = Environment::GetCurrent(data.GetIsolate());
  env->promise_tracker_.WeakCallback(data.GetParameter());
}

void PromiseTracker::WeakCallback(v8::Persistent<v8::Object>* persistent_) {
  Local<Object> promise = persistent_->Get(env_->isolate());
  CHECK(HasPromise(promise));
  Local<Value> err = node::GetPromiseReason(env_, promise);
  Local<Message> message = Exception::CreateMessage(env_->isolate(), err);

  node::InternalFatalException(env_->isolate(), err, message, true);
  UntrackPromise(promise);
}

void PromiseTracker::ForEach(Iterator fn) {
  for (auto it = set_.begin(); it != set_.end(); ++it) {
    Local<Object> object = (*it)->Get(env_->isolate());
    fn(env_, object);
  }
}

}  // namespace node
