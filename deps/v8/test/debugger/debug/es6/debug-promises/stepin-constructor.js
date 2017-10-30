// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;
var exception = null;
var breaks = [];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    breaks.push(exec_state.frame(0).sourceLineText().trimLeft());
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

function resolver(resolve, reject) {
  print(1);
  print(2);
  print(3);
  resolve();
}

debugger;
var p = new Promise(resolver);

Debug.setListener(null);

var expected_breaks = [
  "debugger;",
  "var p = new Promise(resolver);",
  "print(1);",
  "print(2);",
  "print(3);",
  "resolve();",
  "}",
  "Debug.setListener(null);"
];

assertEquals(expected_breaks, breaks);
assertNull(exception);
