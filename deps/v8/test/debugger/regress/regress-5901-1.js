// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  throw new Error();
}

function g() {
  try {
    f();
  } catch (e) {
    return 1;  // Break
  }
}

function h() {
  return g();
}

%PrepareFunctionForOptimization(h);
h();
h();

var Debug = debug.Debug;
var step_count = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Exception) {
    step_count++;
    exec_state.prepareStep(Debug.StepAction.StepNext);
  } else if (event == Debug.DebugEvent.Break) {
    step_count++;
    try {
      assertTrue(exec_state.frame().sourceLineText().includes('Break'));
    } catch (e) {
      exception = e;
      print(e);
    }
  }
}

Debug.setListener(listener);
Debug.setBreakOnException();
%OptimizeFunctionOnNextCall(h);
h();
Debug.setListener(null);
assertNull(exception);
