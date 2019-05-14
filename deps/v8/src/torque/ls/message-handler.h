// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LS_MESSAGE_HANDLER_H_
#define V8_TORQUE_LS_MESSAGE_HANDLER_H_

#include "src/base/macros.h"
#include "src/torque/ls/json.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

// The message handler might send responses or follow up requests.
// To allow unit testing, the "sending" function is configurable.
using MessageWriter = void (*)(JsonValue& message);

V8_EXPORT_PRIVATE void HandleMessage(JsonValue& raw_message, MessageWriter);

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_LS_MESSAGE_HANDLER_H_
