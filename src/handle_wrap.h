#ifndef HANDLE_WRAP_H_
#define HANDLE_WRAP_H_

namespace node {

// Rules:
//
// - Do not throw from handle methods. Set errno.
//
// - MakeCallback may only be made directly off the event loop.
//   That is there can be no JavaScript stack frames underneith it.
//   (Is there anyway to assert that?)
//
// - No use of v8::WeakReferenceCallback. The close callback signifies that
//   we're done with a handle - external resources can be freed.
//
// - Reusable?
//
// - The uv_close_cb is used to free the c++ object. The close callback
//   is not made into javascript land.
//
// - uv_ref, uv_unref counts are managed at this layer to avoid needless
//   js/c++ boundary crossing. At the javascript layer that should all be
//   taken care of.

class HandleWrap {
  public:
    static void Initialize(v8::Handle<v8::Object> target);
    static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  protected:
    HandleWrap(v8::Handle<v8::Object> object, uv_handle_t* handle);
    virtual ~HandleWrap();

    virtual void StateChange() {}

    v8::Persistent<v8::Object> object_;

  private:
    static void OnClose(uv_handle_t* handle);
    // Using double underscore due to handle_ member in tcp_wrap. Probably
    // tcp_wrap should rename it's member to 'handle'.
    uv_handle_t* handle__;
};


}  // namespace node


#endif  // HANDLE_WRAP_H_
