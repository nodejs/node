// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LS_MESSAGE_HANDLER_H_
#define V8_TORQUE_LS_MESSAGE_HANDLER_H_

#include "src/base/macros.h"
#include "src/torque/ls/json.h"
#include "src/torque/source-positions.h"
#include "src/torque/torque-compiler.h"

namespace v8 {
namespace internal {
namespace torque {

// A list of source Ids for which the LS provided diagnostic information
// after the last compile. The LS is responsible for syncing diagnostic
// information with the client. Before updated information can be sent,
// old diagnostic messages have to be reset.
DECLARE_CONTEXTUAL_VARIABLE(DiagnosticsFiles, std::vector<SourceId>);

namespace ls {

// The message handler might send responses or follow up requests.
// To allow unit testing, the "sending" function is configurable.
using MessageWriter = std::function<void(JsonValue)>;

V8_EXPORT_PRIVATE void HandleMessage(JsonValue raw_message, MessageWriter);

// Called when a compilation run finishes. Exposed for testability.
V8_EXPORT_PRIVATE void CompilationFinished(TorqueCompilerResult result,
                                           MessageWriter);

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_LS_MESSAGE_HANDLER_H_
