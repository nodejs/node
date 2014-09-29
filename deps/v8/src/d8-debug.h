// Copyright 2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_DEBUG_H_
#define V8_D8_DEBUG_H_


#include "src/d8.h"
#include "src/debug.h"


namespace v8 {

void HandleDebugEvent(const Debug::EventDetails& event_details);

}  // namespace v8


#endif  // V8_D8_DEBUG_H_
