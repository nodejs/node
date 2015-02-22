#ifndef SRC_JS_STREAM_H_
#define SRC_JS_STREAM_H_

#include "async-wrap.h"
#include "env.h"
#include "stream_base.h"
#include "v8.h"

namespace node {

class JSStream : public StreamBase {
 public:
  static void Initialize(v8::Handle<v8::Object> target,
                         v8::Handle<v8::Value> unused,
                         v8::Handle<v8::Context> context);
};

}  // namespace node

#endif  // SRC_JS_STREAM_H_
