#ifndef HANDLE_WRAP_H_
#define HANDLE_WRAP_H_

namespace node {

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
