// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug
var exception = null;

function breakListener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    exec_state.prepareStep(Debug.StepAction.StepInto);
    // Assert that the break happens at an intended location.
    assertTrue(exec_state.frame(0).sourceLineText().indexOf("// break") > 0);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(breakListener);

debugger;                          // break

function f(x) {
  return x;                        // break
}                                  // break

Debug.setBreakPoint(f, 0, 0);      // break

new Error("123").stack;            // break
Math.sin(0);                       // break

f("this should break");            // break

Debug.setListener(null);           // break

f("this should not break");

assertNull(exception);
