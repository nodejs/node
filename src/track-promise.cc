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

typedef void (*FreeCallback)(Local<Object> object, Local<Function> fn);


TrackPromise* TrackPromise::New(Isolate* isolate,
                                Local<Object> object) {
  return new TrackPromise(isolate, object);
}


Persistent<Object>* TrackPromise::persistent() {
  return &persistent_;
}


TrackPromise::TrackPromise(Isolate* isolate,
                           Local<Object> object)
    : persistent_(isolate, object) {
  persistent_.SetWeak(this, WeakCallback, WeakCallbackType::kFinalizer);
  persistent_.MarkIndependent();
}


TrackPromise::~TrackPromise() {
  persistent_.Reset();
}


void TrackPromise::WeakCallback(
    const WeakCallbackInfo<TrackPromise>& data) {
  data.GetParameter()->WeakCallback(data.GetIsolate());
}


void TrackPromise::WeakCallback(Isolate* isolate) {
  Environment* env = Environment::GetCurrent(isolate);

  Local<Value> promise = persistent_.Get(isolate);
  Local<Value> err = node::GetPromiseReason(env, promise);
  Local<Message> message = Exception::CreateMessage(isolate, err);

  node::InternalFatalException(isolate, err, message, true);
  delete this;
}

}  // namespace node
