// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/test-interface.h"

#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"

namespace v8_inspector {

void SetMaxAsyncTaskStacksForTest(V8Inspector* inspector, int limit) {
  static_cast<V8InspectorImpl*>(inspector)
      ->debugger()
      ->setMaxAsyncTaskStacksForTest(limit);
}

void DumpAsyncTaskStacksStateForTest(V8Inspector* inspector) {
  static_cast<V8InspectorImpl*>(inspector)
      ->debugger()
      ->dumpAsyncTaskStacksStateForTest();
}

}  // namespace v8_inspector
