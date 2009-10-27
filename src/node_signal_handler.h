// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef SRC_SIGNAL_HANDLER_H_
#define SRC_SIGNAL_HANDLER_H_

#include <node.h>
#include <node_events.h>

#include <v8.h>
#include <ev.h>

namespace node {

class SignalHandler : EventEmitter {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  SignalHandler() : EventEmitter() { }
  ~SignalHandler();

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);

 private:
  static void OnSignal(EV_P_ ev_signal *watcher, int revents);
  ev_signal watcher_;
};

}  // namespace node
#endif  // SRC_SIGNAL_HANDLER_H_

