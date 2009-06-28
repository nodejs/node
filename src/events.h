#ifndef node_events_h
#define node_events_h

#include "node.h"
#include <v8.h>

namespace node {

class EventEmitter : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  virtual size_t size (void) { return sizeof(EventEmitter); };

  bool Emit (const char *type, int argc, v8::Handle<v8::Value> argv[]);

  EventEmitter (v8::Handle<v8::Object> handle)
    : ObjectWrap(handle) { }
};

class Promise : public EventEmitter {
 public:
  static void Initialize (v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  virtual size_t size (void) { return sizeof(Promise); };
 
  bool EmitSuccess (int argc, v8::Handle<v8::Value> argv[])
  {
    return Emit("Success", argc, argv);
  }

  bool EmitError (int argc, v8::Handle<v8::Value> argv[])
  {
    return Emit("Error", argc, argv);
  }

  static Promise* Create (void);

 protected:

  Promise (v8::Handle<v8::Object> handle) : EventEmitter(handle) { }
};

} // namespace node
#endif
