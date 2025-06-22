// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PROXY_INL_H_
#define V8_OBJECTS_JS_PROXY_INL_H_

#include "src/objects/js-proxy.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/instance-type-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"  // Needed for write barriers

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-proxy-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSProxy)

bool JSProxy::IsRevoked() const { return !IsJSReceiver(handler()); }

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PROXY_INL_H_
