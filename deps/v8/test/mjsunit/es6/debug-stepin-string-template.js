// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

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

function u(x) {
  return x.toUpperCase();                         // d
}                                                 // e

var n = 3;

var o = {
  toString: function() {
    return "D";                                   // f
  }                                               // g
}



Debug.setListener(listener);
debugger;                                         // a
var s = `1 ${u("a")} 2 ${u("b")} 3 ${n} 4 ${o}`;  // b
Debug.setListener(null);                          // c

assertNull(exception);

assertEquals([
  "a0",
  "b44",
  "b13",
  "d2b13",
  "e0b13",
  "b25",
  "d2b25",
  "e0b25",
  "f4b44",
  "g2b44",
  "c0"
], log);
