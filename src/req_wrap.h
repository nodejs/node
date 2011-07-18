#ifndef REQ_WRAP_H_
#define REQ_WRAP_H_

namespace node {

template <typename T>
class ReqWrap {
 public:
  ReqWrap() {
    v8::HandleScope scope;
    object_ = v8::Persistent<v8::Object>::New(v8::Object::New());
  }

  ~ReqWrap() {
    // Assert that someone has called Dispatched()
    assert(req_.data == this);
    assert(!object_.IsEmpty());
    object_.Dispose();
    object_.Clear();
  }

  // Call this after the req has been dispatched.
  void Dispatched() {
    req_.data = this;
  }

  v8::Persistent<v8::Object> object_;
  T req_;
};


}  // namespace node


#endif  // REQ_WRAP_H_
