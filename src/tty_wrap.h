#ifndef SRC_TTY_WRAP_H_
#define SRC_TTY_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "handle_wrap.h"
#include "stream_wrap.h"

namespace node {

class TTYWrap : public StreamWrap {
 public:
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);

  uv_tty_t* UVHandle();

  size_t self_size() const override { return sizeof(*this); }

 private:
  TTYWrap(Environment* env,
          v8::Local<v8::Object> object,
          int fd,
          bool readable);

  static void GuessHandleType(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsTTY(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetWindowSize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetRawMode(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  uv_tty_t handle_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TTY_WRAP_H_
