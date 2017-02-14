// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = new DebugWrapper();
Debug.enable();

// Simple debug event handler which performs 100 steps and then retrieves
// the resulting value of "i" in f().

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    if (step_count > 0) {
      Debug.stepInto();
      step_count--;
    } else {
      const frameid = exec_state.frames[0].callFrameId;
      result = Debug.evaluate(frameid, "i").value;
    }
  }
};

// Add the debug event listener.
Debug.setListener(listener);

// Test debug event for break point.
function f() {
  var i;                       // Line 1.
  for (i = 0; i < 100; i++) {  // Line 2.
    x = 1;                     // Line 3.
  }
};

// Set a breakpoint on the for statement (line 1).
Debug.setBreakPoint(f, 1);

// Check that performing 100 steps will make i 33.
let step_count = 100;
let result = -1;

f();

assertEquals(33, result);
