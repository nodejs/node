#include "base-object.h"
#include "base-object-inl.h"
#include "env.h"
#include "env-inl.h"

namespace node {

using v8::Local;
using v8::Object;

BaseObject::BaseObject(Environment* env, Local<Object> handle)
    : handle_(env->isolate(), handle),
      isolate_(env->isolate()),
      env_(env) {
  CHECK_EQ(false, handle.IsEmpty());
}

}  // namespace node
