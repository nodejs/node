// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

Debug = debug.Debug;
var exception = null;
var error_count = 0;

function f() {
  return 0;  // Break
}            // Break

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    if (exec_state.frame(0).sourceLineText().indexOf("Break") <0) {
      error_count++;
    }
    exec_state.prepareStep(Debug.StepAction.StepIn);
    f();  // We should not break in this call of f().
  } catch (e) {
    print(e + e.stack);
    exception = e;
  }
}

Debug.setListener(listener);

debugger;  // Break
f();       // Break

Debug.setListener(null);  // Break

assertNull(exception);
assertEquals(0, error_count);
