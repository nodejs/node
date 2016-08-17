// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

// Check that the we are still in function context when we break on return.

var Debug = debug.Debug;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    // Access scope details to check the context is correct.
    var scope_count = exec_state.frame().scopeCount();
    // Do steps until we reach the global scope again.
    exec_state.prepareStep(Debug.StepAction.StepIn);
  }
}

Debug.setListener(listener);

function f() {
  debugger;

  L: with ({x:12}) {
    break L;
  }

  return;
}
f();
