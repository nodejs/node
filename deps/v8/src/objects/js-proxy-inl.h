// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PROXY_INL_H_
#define V8_OBJECTS_JS_PROXY_INL_H_

#include "src/objects/js-proxy.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<UnionOf<JSReceiver, Null>> JSProxy::target() const {
  return target_.load();
}
void JSProxy::set_target(Tagged<UnionOf<JSReceiver, Null>> value,
                         WriteBarrierMode mode) {
  target_.store(this, value, mode);
}

Tagged<UnionOf<JSReceiver, Null>> JSProxy::handler() const {
  return handler_.load();
}
void JSProxy::set_handler(Tagged<UnionOf<JSReceiver, Null>> value,
                          WriteBarrierMode mode) {
  handler_.store(this, value, mode);
}

int JSProxy::flags() const { return flags_; }
void JSProxy::set_flags(int value) { flags_ = value; }

bool JSProxy::is_revocable() const { return IsRevocableBit::decode(flags()); }

bool JSProxy::IsRevoked() const { return !IsJSReceiver(handler()); }

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PROXY_INL_H_
