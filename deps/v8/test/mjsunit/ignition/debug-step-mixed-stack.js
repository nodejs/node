// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

Debug = debug.Debug

var exception = null;
var frame_depth = 11;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertEquals(frame_depth, exec_state.frameCount());
    assertTrue(/\/\/ Break$/.test(exec_state.frame(0).sourceLineText()));
    assertEquals(12 - frame_depth, exec_state.frame(0).evaluate("x").value());
    if (frame_depth > 2) exec_state.prepareStep(Debug.StepAction.StepOut);
    frame_depth--;
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

function ChooseCode(f, x) {
  switch (x % 2) {
    case 0:
      %BaselineFunctionOnNextCall(f);
      break;
    case 1:
      %InterpretFunctionOnNextCall(f);
      break;
  }
}

function factorial(x) {
  ChooseCode(factorial, x);
  if (x == 1) {
    debugger;         // Break
    return 1;
  }
  var factor = factorial(x - 1);
  return x * factor;  // Break
}

Debug.setListener(listener);

assertEquals(3628800, factorial(10));

Debug.setListener(null);
assertNull(exception);
assertEquals(1, frame_depth);
