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

  /* Attach() marks the object as being attached to an event loop.
   * Attached objects will not be garbage collected, even if
   * all references are lost.
   */
  void Attach();
  /* Detach() marks an object as detached from the event loop.  This is its
   * default state.  When an object with a "weak" reference changes from
   * attached to detached state it will be freed. Be careful not to access
   * the object after making this call as it might be gone!
   * (A "weak reference" is v8 terminology for an object who only has a
   * persistant handle.)
   */
  void Detach();

private:
  static void MakeWeak (v8::Persistent<v8::Value> _, void *data);
  int attach_count_;
  bool weak_;
};

} // namespace node
#endif // node_h
