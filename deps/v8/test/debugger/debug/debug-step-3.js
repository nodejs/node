// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Debug = new DebugWrapper();
Debug.enable();

// This test tests that full code compiled without debug break slots
// is recompiled with debug break slots when debugging is started.

var bp;
var done = false;
var step_count = 0;
var set_bp = false

// Debug event listener which steps until the global variable done is true.
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    if (!done) Debug.stepOver();
    step_count++;
  }
};

// Set the global variables state to prepare the stepping test.
function prepare_step_test() {
  done = false;
  step_count = 0;
}

// Test function to step through.
function f() {
  var a = 0;
  if (set_bp) { bp = Debug.setBreakPoint(f, 3); }
  var i = 1;
  var j = 2;
  done = true;
};

prepare_step_test();
f();

// Add the debug event listener.
Debug.setListener(listener);

// Make f set a breakpoint with an activation on the stack.
prepare_step_test();
set_bp = true;
f();
assertEquals(4, step_count);
Debug.clearBreakPoint(bp);

// Set a breakpoint on the first var statement (line 1).
set_bp = false;
bp = Debug.setBreakPoint(f, 3);

// Step through the function ensuring that the var statements are hit as well.
prepare_step_test();
f();
assertEquals(4, step_count);

// Clear the breakpoint and check that no stepping happens.
Debug.clearBreakPoint(bp);
prepare_step_test();
f();
assertEquals(0, step_count);

// Get rid of the debug event listener.
Debug.setListener(null);
