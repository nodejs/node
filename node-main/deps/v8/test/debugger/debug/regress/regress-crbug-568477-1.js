// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises


var Debug = debug.Debug;
var expected = ["debugger;", "var x = y;", "debugger;", "var x = y;"];
var log = [];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = exec_state.frame(0).sourceLineText().trimLeft();
    log.push(line);
    if (line == "debugger;") exec_state.prepareStep(Debug.StepAction.StepOver);
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

Promise.resolve().then(f).catch(call_f_with_deeper_stack);

// Schedule microtask to check against expectation at the end.
function testDone(iteration) {
  function checkResult() {
    try {
      assertTrue(iteration < 10);
      if (expected.length == log.length) {
        assertEquals(expected, log);
      } else {
        testDone(iteration + 1);
      }
    } catch (e) {
      %AbortJS(e + "\n" + e.stack);
    }
  }

  %EnqueueMicrotask(checkResult);
}

testDone(0);
