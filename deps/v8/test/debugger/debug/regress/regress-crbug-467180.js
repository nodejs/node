// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function f() {
  for (var i = 10; i < 14; i++) {  // 1
    i *= 1;                        // 2
  }
}                                  // 3

var state = "conditional";
var log = [];
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var label = +exec_state.frame(0).sourceLineText().substr(-1);
    log.push(label);
    if (label == 2) log.push(exec_state.frame(0).evaluate("i").value());
    exec_state.prepareStep(Debug.StepAction.StepOver);
  } catch (e) {
    exception = e;
    print("Caught something. " + e + " " + e.stack);
  };
};


var Debug = debug.Debug;
Debug.setListener(listener);

Debug.setBreakPoint(f, 2, 0, "i == 12");

f();

Debug.setListener(null);  // 4

assertEquals([2,12,1,1,2,13,1,1,3,4], log);
assertNull(exception);
