// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef NODE_STAT_WATCHER_H_
#define NODE_STAT_WATCHER_H_

#include <node.h>
#include <node_events.h>
#include <ev.h>

namespace node {

class StatWatcher : EventEmitter {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  StatWatcher() : EventEmitter() {
    persistent_ = false;
    path_ = NULL;
    ev_init(&watcher_, StatWatcher::Callback);
    watcher_.data = this;
  }

  ~StatWatcher() {
    Stop();
    assert(path_ == NULL);
  }

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);

 private:
  static void Callback(EV_P_ ev_stat *watcher, int revents);

  void Stop();

  ev_stat watcher_;
  bool persistent_;
  char *path_;
};

}  // namespace node
#endif  // NODE_STAT_WATCHER_H_
