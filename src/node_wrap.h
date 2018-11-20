#ifndef SRC_NODE_WRAP_H_
#define SRC_NODE_WRAP_H_

#include "env.h"
#include "env-inl.h"
#include "js_stream.h"
#include "pipe_wrap.h"
#include "tcp_wrap.h"
#include "tty_wrap.h"
#include "udp_wrap.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

namespace node {

#define WITH_GENERIC_UV_STREAM(env, obj, BODY, ELSE)                          \
    do {                                                                      \
      if (env->tcp_constructor_template().IsEmpty() == false &&               \
          env->tcp_constructor_template()->HasInstance(obj)) {                \
        TCPWrap* const wrap = Unwrap<TCPWrap>(obj);                           \
        BODY                                                                  \
      } else if (env->tty_constructor_template().IsEmpty() == false &&        \
                 env->tty_constructor_template()->HasInstance(obj)) {         \
        TTYWrap* const wrap = Unwrap<TTYWrap>(obj);                           \
        BODY                                                                  \
      } else if (env->pipe_constructor_template().IsEmpty() == false &&       \
                 env->pipe_constructor_template()->HasInstance(obj)) {        \
        PipeWrap* const wrap = Unwrap<PipeWrap>(obj);                         \
        BODY                                                                  \
      } else {                                                                \
        ELSE                                                                  \
      }                                                                       \
    } while (0)

#define WITH_GENERIC_STREAM(env, obj, BODY)                                   \
    do {                                                                      \
      WITH_GENERIC_UV_STREAM(env, obj, BODY, {                                \
        if (env->tls_wrap_constructor_template().IsEmpty() == false &&        \
            env->tls_wrap_constructor_template()->HasInstance(obj)) {         \
          TLSWrap* const wrap = Unwrap<TLSWrap>(obj);                         \
          BODY                                                                \
        } else if (env->jsstream_constructor_template().IsEmpty() == false && \
                   env->jsstream_constructor_template()->HasInstance(obj)) {  \
          JSStream* const wrap = Unwrap<JSStream>(obj);                       \
          BODY                                                                \
        }                                                                     \
      });                                                                     \
    } while (0)

inline uv_stream_t* HandleToStream(Environment* env,
                                   v8::Local<v8::Object> obj) {
  v8::HandleScope scope(env->isolate());

  WITH_GENERIC_UV_STREAM(env, obj, {
    return reinterpret_cast<uv_stream_t*>(wrap->UVHandle());
  }, {});

  return nullptr;
}

}  // namespace node

#endif  // SRC_NODE_WRAP_H_
