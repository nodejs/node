// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --expose-debug-as debug

Debug = debug.Debug

var exception = null;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    var column = event_data.sourceColumn();
    assertTrue(event_data.sourceLineText().indexOf(
        `Break ${break_count++}. ${column}.`) > 0);
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    print(e + e.stack);
    exception = e;
  }
};

function f() {
  var a = 1;                     // Break 2. 10.
  // This return statement emits no bytecode instruction for the evaluation of
  // the to-be-returned expression. Therefore we cannot set a break location
  // before the statement and a second break location immediately before
  // returning to the caller.
  return a;
}                                // Break 3. 0.

Debug.setListener(listener);
debugger;                        // Break 0. 0.
f();                             // Break 1. 0.
Debug.setListener(null);         // Break 4. 0.

assertNull(exception);
assertEquals(5, break_count);
