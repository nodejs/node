// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/source-positions.h"

namespace v8 {
namespace internal {
namespace torque {

DEFINE_CONTEXTUAL_VARIABLE(CurrentSourceFile)
DEFINE_CONTEXTUAL_VARIABLE(CurrentSourcePosition)
DEFINE_CONTEXTUAL_VARIABLE(SourceFileMap)

}  // namespace torque
}  // namespace internal
}  // namespace v8
