// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

var exception = null;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    assertTrue(
        event_data.sourceLineText().indexOf(`Break ${break_count++}`) > 0);
    exec_state.prepareStep(Debug.StepAction.StepInto);
  } catch (e) {
    exception = e;
  }
};

function f() {
  let a = 1;  // Break 2
  {
    using b = { // Break 3
      value: 2,
      [Symbol.dispose]() {
        console.log(42);  // Break 7
      }  // Break 8
    };
    using c = { // Break 4
      value: 3,
      [Symbol.dispose]() {
        console.log(43);  // Break 5
      }  // Break 6
    };
  }
  let d = 4;  // Break 9
};  // Break 10

Debug.setListener(listener);
debugger;                 // Break 0
f();                      // Break 1
Debug.setListener(null);  // Break 11
assertNull(exception);
