// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug

var exception = null;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    assertTrue(
        event_data.sourceLineText().indexOf(`Break ${break_count++}.`) > 0);
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    exception = e;
  }
};

function replaceCallback(a) {
  return "x";                                   // Break 2.
}                                               // Break 3.

var re = /x/g;
// Optimize the inner helper function for string replace.
for (var i = 0; i < 10000; i++) "x".replace(re, replaceCallback);

Debug.setListener(listener);
debugger;                                       // Break 0.
var result = "x".replace(re, replaceCallback);  // Break 1.
Debug.setListener(null);                        // Break 4.

assertNull(exception);
