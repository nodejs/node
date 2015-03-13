#include "js_stream.h"

#include "async-wrap.h"
#include "env.h"
#include "env-inl.h"
#include "stream_base.h"
#include "v8.h"

namespace node {

using v8::Context;
using v8::Handle;
using v8::Object;
using v8::Value;

void JSStream::Initialize(Handle<Object> target,
                          Handle<Value> unused,
                          Handle<Context> context) {
}

}  // namespace node
