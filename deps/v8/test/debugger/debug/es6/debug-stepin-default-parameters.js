// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug

var exception = null;
var log = [];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    var entry = "";
    for (var i = 0; i < exec_state.frameCount(); i++) {
      entry += exec_state.frame(i).sourceLineText().substr(-1);
      entry += exec_state.frame(i).sourceColumn();
    }
    log.push(entry);
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    exception = e;
  }
};

function default_arg(x) {
  return "default";                 // d
}                                   // e

function f(arg0 = default_arg()) {  // f
  return arg0;                      // g
}                                   // h


Debug.setListener(listener);
debugger;                           // a
var result = f();                   // b
Debug.setListener(null);            // c

assertNull(exception);
assertEquals("default", result);

assertEquals(["a0","b13","f18b13","d2f18b13","e0f18b13","g2b13","h0b13","c0"],
             log);
