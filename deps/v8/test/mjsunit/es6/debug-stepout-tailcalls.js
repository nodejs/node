// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --harmony-tailcalls

"use strict";

var Debug = debug.Debug
var exception = null;
var breaks = 0;

function f(x) {
  if (x > 0) {
    return f(x - 1);      // Tail call
  }
  debugger;               // Break 0
}

function g(x) {
  return f(x);            // Tail call
}

function h(x) {
  g(x);                   // Not tail call
}                         // Break 1


function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertTrue(event_data.sourceLineText().indexOf(`Break ${breaks++}`) > 0);
    exec_state.prepareStep(Debug.StepAction.StepOut);
  } catch (e) {
    exception = e;
  };
};

Debug.setListener(listener);

h(3);

Debug.setListener(null);  // Break 2
assertNull(exception);
assertEquals(3, breaks);
