// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;
var expected = ["debugger;",
                "var x = y;",
                "debugger;",
                "var x = y;"];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = exec_state.frame(0).sourceLineText().trimLeft();
    assertEquals(expected.shift(), line);
    if (line == "debugger;") exec_state.prepareStep(Debug.StepAction.StepNext);
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setListener(listener);

function f() {
  var a = 1;
  debugger;
  var x = y;
  print(x);
}

function call_f_with_deeper_stack() {
  (() => () => () => f())()()();
}

new Promise(f).catch(call_f_with_deeper_stack);
var a = 1;
