// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  debugger;
  return 1;
}

function g() {
  return f();
}  // Break

function h() {
  return g();
}

h();
h();

var Debug = debug.Debug;
var step_count = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    if (step_count == 0) {
      exec_state.prepareStep(Debug.StepAction.StepOut);
    } else {
      assertTrue(exec_state.frame().sourceLineText().includes('Break'));
    }
    step_count++;
  } catch (e) {
    exception = e;
    print(e);
  }
}

Debug.setListener(listener);
% OptimizeFunctionOnNextCall(h);
h();
Debug.setListener(null);
assertNull(exception);
assertEquals(2, step_count);
