#ifndef node_events_h
#define node_events_h

#include "node.h"
#include <v8.h>

namespace node {

class EventEmitter : public ObjectWrap {
 public:
  static void Initialize (v8::Local<v8::FunctionTemplate> ctemplate);
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

  static Promise* Create (bool ref = false);

  bool EmitSuccess (int argc, v8::Handle<v8::Value> argv[]);
  bool EmitError (int argc, v8::Handle<v8::Value> argv[]);
  void Block ();


  v8::Handle<v8::Object> Handle ()
  {
    return handle_;
  }

 protected:
  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> Block (const v8::Arguments& args);
  static v8::Handle<v8::Value> EmitSuccess (const v8::Arguments& args);
  static v8::Handle<v8::Value> EmitError (const v8::Arguments& args);

  virtual void Detach (void);

  bool blocking_;
  bool ref_;

  Promise () : EventEmitter()
  {
    blocking_ = false;
    ref_ = false;
  }
};

} // namespace node
#endif
