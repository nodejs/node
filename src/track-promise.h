#ifndef SRC_TRACK_PROMISE_H_
#define SRC_TRACK_PROMISE_H_

#include "v8.h"

namespace node {

class Environment;

class TrackPromise {
 public:
  TrackPromise(v8::Isolate* isolate, v8::Local<v8::Object> object);
  virtual ~TrackPromise();

  static TrackPromise* New(v8::Isolate* isolate,
                                  v8::Local<v8::Object> object);

  inline v8::Persistent<v8::Object>* persistent();

  static inline void WeakCallback(
      const v8::WeakCallbackInfo<TrackPromise>& data);

 private:
  inline void WeakCallback(v8::Isolate* isolate);

  v8::Persistent<v8::Object> persistent_;
};

}  // namespace node

#endif  // SRC_TRACK_PROMISE_H_
