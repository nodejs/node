// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef NODE_IDLE_H_
#define NODE_IDLE_H_

#include <node_object_wrap.h>
#include <ev.h>

namespace node {

class IdleWatcher : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  IdleWatcher() : ObjectWrap() {
    ev_idle_init(&watcher_, IdleWatcher::Callback);
    watcher_.data = this;
  }

  ~IdleWatcher() {
    ev_idle_stop(EV_DEFAULT_UC_ &watcher_);
  }

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetPriority(const v8::Arguments& args);

 private:
  static void Callback(EV_P_ ev_idle *watcher, int revents);

  void Start();
  void Stop();

  ev_idle watcher_;
};

}  // namespace node
#endif  // NODE_IDLE_H_

