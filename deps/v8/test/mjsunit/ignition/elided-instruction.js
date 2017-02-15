// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

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
  return a;                      // Break 3. 2.
}                                // Break 4. 0.

Debug.setListener(listener);
debugger;                        // Break 0. 0.
f();                             // Break 1. 0.
Debug.setListener(null);         // Break 5. 0.

assertNull(exception);
assertEquals(6, break_count);
