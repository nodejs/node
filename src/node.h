#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>
#include <oi_socket.h>

namespace node {

#define NODE_UNWRAP(type, value) static_cast<type*>(node::ObjectWrap::Unwrap(value))

#define NODE_DEFINE_CONSTANT(target, constant)                            \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Integer::New(constant))

#define NODE_SET_METHOD(obj, name, callback)                              \
  obj->Set(v8::String::NewSymbol(name),                                   \
           v8::FunctionTemplate::New(callback)->GetFunction())

#define NODE_SET_PROTOTYPE_METHOD(templ, name, callback)                  \
do {                                                                      \
  Local<Signature> __callback##_SIG = Signature::New(templ);              \
  Local<FunctionTemplate> __callback##_TEM =                              \
    FunctionTemplate::New(callback, Handle<Value>() , __callback##_SIG ); \
  templ->PrototypeTemplate()->Set(v8::String::NewSymbol(name),            \
                                  __callback##_TEM);                      \
} while(0)

enum encoding {ASCII, UTF8, RAW};
enum encoding ParseEncoding (v8::Handle<v8::Value> encoding_v);
void FatalException (v8::TryCatch &try_catch); 
oi_buf * buf_new (size_t size);

class ObjectWrap {
public:
  ObjectWrap (v8::Handle<v8::Object> handle);
  virtual ~ObjectWrap ( );

  virtual size_t size (void) = 0;

  /* This must be called after each new ObjectWrap creation! */
  static void InformV8ofAllocation (node::ObjectWrap *obj);

protected:
  static void* Unwrap (v8::Handle<v8::Object> handle);

  /* Attach() marks the object as being attached to an event loop.
   * Attached objects will not be garbage collected, even if
   * all references are lost.
   */
  void Attach();
  
  /* Detach() marks an object as detached from the event loop.  This is its
   * default state.  When an object with a "weak" reference changes from
   * attached to detached state it will be freed. Be careful not to access
   * the object after making this call as it might be gone!
   * (A "weak reference" is v8 terminology for an object that only has a
   * persistant handle.)
   */
  void Detach();
  v8::Persistent<v8::Object> handle_;

private:
  static void MakeWeak (v8::Persistent<v8::Value> _, void *data);
  int attach_count_;
  bool weak_;
};

} // namespace node
#endif // node_h
