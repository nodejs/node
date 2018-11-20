// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Debug = new DebugWrapper();
Debug.enable();

// Tests how debugger can step over not necessarily in the top frame.

// Simple 3 functions, that protocol their execution state in global
// variable state.
var state;

function f() {
  var a = 1978;
  for (state[2] = 0; state[2] < 3; state[2]++) {
    void String(a);
  }
}
function g() {
  for (state[1] = 0; state[1] < 3; state[1]++) {
    f();
  }
}
function h() {
  state = [-1, -1, -1];
  for (state[0] = 0; state[0] < 3; state[0]++) {
    g();
  }
}

function TestCase(expected_final_state) {
  var listener_exception = null;
  var state_snapshot;
  var listener_state;
  var bp;

  function listener(event, exec_state, event_data, data) {
    const location = exec_state.frames[0].location
    print("Here (" + event + "/" + listener_state + "): " +
          location.lineNumber + ":" + location.columnNumber);
    try {
      if (event == Debug.DebugEvent.Break) {
        if (listener_state == 0) {
          Debug.clearBreakPoint(bp);
          Debug.stepOver();
          listener_state = 1;
        } else if (listener_state == 1) {
          state_snapshot = String(state);
          print("State: " + state_snapshot);
          Debug.setListener(null);
          listener_state = 2;
        }
      }
    } catch (e) {
      listener_exception = e;
    }
  }


  // Add the debug event listener.
  listener_state = 0;
  Debug.setListener(listener);
  bp = Debug.setBreakPoint(f, 1);

  h();
  Debug.setListener(null);
  if (listener_exception !== null) {
    print("Exception caught: " + listener_exception);
    assertUnreachable();
  }

  assertEquals(expected_final_state, state_snapshot);
}


// Warm-up -- make sure all is compiled and ready for breakpoint.
h();

TestCase("0,0,-1");
