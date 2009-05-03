#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>

namespace node {

#define NODE_SYMBOL(name) v8::String::NewSymbol(name)
#define NODE_METHOD(name) v8::Handle<v8::Value> name (const v8::Arguments& args)
#define NODE_SET_METHOD(obj, name, callback) \
  obj->Set(NODE_SYMBOL(name), v8::FunctionTemplate::New(callback)->GetFunction())
#define NODE_UNWRAP(type, value) static_cast<type*>(node::ObjectWrap::Unwrap(value))

enum encoding {UTF8, RAW};
void fatal_exception (v8::TryCatch &try_catch); 
void eio_warmup (void); // call this before creating a new eio event.

class ObjectWrap {
public:
  ObjectWrap (v8::Handle<v8::Object> handle);
  virtual ~ObjectWrap ( );

protected:
  static void* Unwrap (v8::Handle<v8::Object> handle);
  v8::Persistent<v8::Object> handle_;

private:
  static void MakeWeak (v8::Persistent<v8::Value> _, void *data);
};

} // namespace node
#endif // node_h
