// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function get() {
  return 3;                       // Break
}                                 // Break

function set(x) {
  this.x = x;                     // Break
}                                 // Break

var o = {};
Object.defineProperty(o, "get", { get : get });
Object.defineProperty(o, "set", { set : set });

function f() {
  for (var i = 0; i < 10; i++) {  // Break
    o.get;                        // Break
    o.set = 1;                    // Break
  }
}                                 // Break

var break_count = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var source_line = exec_state.frame(0).sourceLineText();
    assertTrue(source_line.indexOf("// Break") > 0);
    exec_state.prepareStep(Debug.StepAction.StepIn);
    break_count++;
  } catch (e) {
    exception = e;
  }
}

var Debug = debug.Debug;
Debug.setListener(listener);

debugger;                         // Break
f();                              // Break

Debug.setListener(null);          // Break
assertEquals(86, break_count);
assertNull(exception);
