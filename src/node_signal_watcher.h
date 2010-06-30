// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef NODE_SIGNAL_WATCHER_H_
#define NODE_SIGNAL_WATCHER_H_

#include <node.h>
#include <node_events.h>

#include <v8.h>
#include <ev.h>

namespace node {

class SignalWatcher : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  SignalWatcher(int sig) : ObjectWrap() {
    ev_signal_init(&watcher_, SignalWatcher::Callback, sig);
    watcher_.data = this;
  }

  ~SignalWatcher() {
    ev_signal_stop(EV_DEFAULT_UC_ &watcher_);
  }

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);

 private:
  static void Callback(EV_P_ ev_signal *watcher, int revents);

  void Start();
  void Stop();

  ev_signal watcher_;
};

}  // namespace node
#endif  // NODE_SIGNAL_WATCHER_H_

