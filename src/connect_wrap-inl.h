#ifndef SRC_CONNECT_WRAP_INL_H_
#define SRC_CONNECT_WRAP_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "connect_wrap.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

inline void NewConnectWrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.IsConstructCall());
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECT_WRAP_INL_H_
