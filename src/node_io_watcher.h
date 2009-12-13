// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef NODE_IO_H_
#define NODE_IO_H_

#include <node.h>
#include <ev.h>

namespace node {

class IOWatcher : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  IOWatcher(int fd, int events) : ObjectWrap() {
    ev_io_init(&watcher_, IOWatcher::Callback, fd, events);
    watcher_.data = this;
  }

  ~IOWatcher() {
    Stop();
  }

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);

 private:
  static void Callback(EV_P_ ev_io *watcher, int revents);

  void Stop();

  ev_io watcher_;
};

}  // namespace node
#endif  // NODE_IO_H_

