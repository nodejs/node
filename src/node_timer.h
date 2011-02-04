#ifndef SRC_NODE_TIMER_H_
#define SRC_NODE_TIMER_H_

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <ev.h>

namespace node {

class Timer : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  Timer() : ObjectWrap() {
    // dummy timeout values
    ev_timer_init(&watcher_, OnTimeout, 0., 1.);
    watcher_.data = this;
  }

  ~Timer();

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);
  static v8::Handle<v8::Value> Again(const v8::Arguments& args);
  static v8::Handle<v8::Value> RepeatGetter(v8::Local<v8::String> property,
                                            const v8::AccessorInfo& info);
  static void RepeatSetter(v8::Local<v8::String> property,
                           v8::Local<v8::Value> value,
                           const v8::AccessorInfo& info);

 private:
  static void OnTimeout(EV_P_ ev_timer *watcher, int revents);
  void Stop();
  ev_timer watcher_;
};

}  // namespace node
#endif  // SRC_NODE_TIMER_H_
