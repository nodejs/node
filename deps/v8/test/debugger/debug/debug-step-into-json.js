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

function toJsonCallback() {
  return "x";                    // Break 2.
}                                // Break 3.
var o = {};
o.toJSON = toJsonCallback;

Debug.setListener(listener);
debugger;                        // Break 0.
var result = JSON.stringify(o);  // Break 1.
Debug.setListener(null);         // Break 4.

assertEquals('"x"', result);
assertNull(exception);
