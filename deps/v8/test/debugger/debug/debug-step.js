// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

// Simple debug event handler which first time hit will perform 1000 steps and
// second time hit will evaluate and store the value of "i". If requires that
// the global property "state" is initially zero.

var bp1, bp2;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    if (step_count > 0) {
      exec_state.prepareStep(Debug.StepAction.StepIn);
      step_count--;
    } else {
      result = exec_state.frame().evaluate("i").value();
      // Clear the break point on line 2 if set.
      if (bp2) {
        Debug.clearBreakPoint(bp2);
      }
    }
  }
};

// Add the debug event listener.
Debug.setListener(listener);

// Test debug event for break point.
function f() {
  var i;                        // Line 1.
  for (i = 0; i < 1000; i++) {  // Line 2.
    x = 1;                      // Line 3.
  }
};

// Set a breakpoint on the for statement (line 1).
bp1 = Debug.setBreakPoint(f, 1);

// Check that performing 1000 steps will make i 333.
var step_count = 1000;
result = -1;
f();
assertEquals(333, result);
Debug.setListener(null);
