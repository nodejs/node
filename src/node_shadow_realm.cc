#include "node_shadow_realm.h"

namespace node {
namespace shadow_realm {
using v8::Context;
using v8::Local;
using v8::MaybeLocal;

// static
MaybeLocal<Context> HostCreateShadowRealmContextCallback(
    Local<Context> initiator_context) {
  return Context::New(initiator_context->GetIsolate());
}

}  // namespace shadow_realm
}  // namespace node
