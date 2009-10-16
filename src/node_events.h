// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef SRC_EVENTS_H_
#define SRC_EVENTS_H_

#include <node_object_wrap.h>
#include <v8.h>

namespace node {

class EventEmitter : public ObjectWrap {
 public:
  static void Initialize(v8::Local<v8::FunctionTemplate> ctemplate);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  bool Emit(const char *event, int argc, v8::Handle<v8::Value> argv[]);

 protected:
  static v8::Handle<v8::Value> Emit(const v8::Arguments& args);

  EventEmitter() : ObjectWrap () { }
};

class Promise : public EventEmitter {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static Promise* Create(void);

  bool EmitSuccess(int argc, v8::Handle<v8::Value> argv[]);
  bool EmitError(int argc, v8::Handle<v8::Value> argv[]);
  void Block();

  v8::Handle<v8::Object> Handle() {
    return handle_;
  }

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Block(const v8::Arguments& args);
  static v8::Handle<v8::Value> EmitSuccess(const v8::Arguments& args);
  static v8::Handle<v8::Value> EmitError(const v8::Arguments& args);

  virtual void Detach(void);

  bool has_fired_;
  bool blocking_;
  Promise *prev_; /* for the prev in the Poor Man's coroutine stack */

  void Destack();

  Promise() : EventEmitter() {
    has_fired_ = false;
    blocking_ = false;
    prev_ = NULL;
  }
};

}  // namespace node
#endif  // SRC_EVENTS_H_
