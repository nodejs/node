#ifndef object_wrap_h
#define object_wrap_h

#include <v8.h>
#include <assert.h>

namespace node {

#define NODE_UNWRAP(type, value) static_cast<type*>(node::ObjectWrap::Unwrap(value))

class ObjectWrap {
public:
  ObjectWrap ( ) {
    weak_ = false;
    attached_ = false;
  }

  virtual ~ObjectWrap ( ) {
    if (!handle_.IsEmpty()) {
      handle_->SetInternalField(0, v8::Undefined());
      handle_.Dispose();
      handle_.Clear(); 
    }
  }

 protected:
  static inline void* Unwrap (v8::Handle<v8::Object> handle)
  {
    assert(!handle.IsEmpty());
    assert(handle->InternalFieldCount() == 1);
    return v8::Handle<v8::External>::Cast(
        handle->GetInternalField(0))->Value();
  }

  inline void Wrap(v8::Handle<v8::Object> handle)
  {
    assert(handle_.IsEmpty());
    assert(handle->InternalFieldCount() > 0);
    handle_ = v8::Persistent<v8::Object>::New(handle);
    handle_->SetInternalField(0, v8::External::New(this));
    handle_.MakeWeak(this, MakeWeak);
  }

  /* Attach() marks the object as being attached to an event loop.
   * Attached objects will not be garbage collected, even if
   * all references are lost.
   */
  void Attach() {
    assert(!handle_.IsEmpty());
    attached_ = true;
  }
  
  /* Detach() marks an object as detached from the event loop.  This is its
   * default state.  When an object with a "weak" reference changes from
   * attached to detached state it will be freed. Be careful not to access
   * the object after making this call as it might be gone!
   * (A "weak reference" is v8 terminology for an object that only has a
   * persistant handle.)
   */
  void Detach() {
    assert(!handle_.IsEmpty());
    attached_ = false;
    if (weak_) delete this;
  }

  v8::Persistent<v8::Object> handle_;

 private:
  static void MakeWeak (v8::Persistent<v8::Value> value, void *data) {
    ObjectWrap *obj = static_cast<ObjectWrap*>(data);
    assert(value == obj->handle_);
    obj->weak_ = true;
    if (!obj->attached_) delete obj;
  }
  bool attached_;
  bool weak_;
};

} // namespace node
#endif // object_wrap_h
