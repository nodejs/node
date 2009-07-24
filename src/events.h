#ifndef node_events_h
#define node_events_h

#include "node.h"
#include <v8.h>

namespace node {

class EventEmitter : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  bool Emit (const char *event, int argc, v8::Handle<v8::Value> argv[]);

 protected:
  static v8::Handle<v8::Value> Emit (const v8::Arguments& args);

  EventEmitter () : ObjectWrap () { }
};

class Promise : public EventEmitter {
 public:
  static void Initialize (v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
 
  bool EmitSuccess (int argc, v8::Handle<v8::Value> argv[]);
  bool EmitError (int argc, v8::Handle<v8::Value> argv[]);

  static Promise* Create (void);

  v8::Handle<v8::Object> Handle(void) { return handle_; }

 protected:

  Promise () : EventEmitter() { }
};

} // namespace node
#endif
