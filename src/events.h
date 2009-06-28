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

 protected:
  EventEmitter (v8::Handle<v8::Object> handle)
    : ObjectWrap(handle) { }
};

class Promise : public EventEmitter {
 public:
  static void Initialize (v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  virtual size_t size (void) { return sizeof(Promise); };
 
  bool EmitSuccess (int argc, v8::Handle<v8::Value> argv[]);
  bool EmitError (int argc, v8::Handle<v8::Value> argv[]);

  static Promise* Create (void);

  v8::Handle<v8::Object> Handle(void) { return handle_; }

 protected:

  Promise (v8::Handle<v8::Object> handle) : EventEmitter(handle) { }
};

} // namespace node
#endif
