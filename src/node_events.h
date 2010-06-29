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

  bool Emit(v8::Handle<v8::String> event,
            int argc,
            v8::Handle<v8::Value> argv[]);

 protected:
  EventEmitter() : ObjectWrap () { }
};

}  // namespace node
#endif  // SRC_EVENTS_H_
