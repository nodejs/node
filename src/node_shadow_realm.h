#ifndef SRC_NODE_SHADOW_REALM_H_
#define SRC_NODE_SHADOW_REALM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

namespace node {
namespace shadow_realm {

v8::MaybeLocal<v8::Context> HostCreateShadowRealmContextCallback(
    v8::Local<v8::Context> initiator_context);

}  // namespace shadow_realm
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SHADOW_REALM_H_
