// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --harmony-tailcalls

"use strict";

var Debug = debug.Debug
var exception = null;
var breaks = 0;

function f(x) {
  if (x > 0) {            // B3 B5 B7 B9
    return f(x - 1);      // B4 B6 B8
  }
}                         // B10

function g(x) {
  return f(x);            // B2
}

function h(x) {
  debugger;               // B0
  g(x);                   // B1
}                         // B11


function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    assertTrue(event_data.sourceLineText().indexOf(`B${breaks++}`) > 0);
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    exception = e;
  };
};

Debug.setListener(listener);

h(3);

Debug.setListener(null);  // B12
assertNull(exception);
assertEquals(13, breaks);
