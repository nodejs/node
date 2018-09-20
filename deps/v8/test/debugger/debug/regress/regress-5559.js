// Copyright 2016 the V8 project authors. All rights reserved.
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
    exec_state.prepareStep(Debug.StepAction.StepOut);
  } catch (e) {
    exception = e;
  }
};

function thrower() {
  try {
    debugger;             // Break 0.
    throw 'error';
  } catch (err) {
  }
}


Debug.setListener(listener);
thrower();
Debug.setListener(null);  // Break 1.

assertNull(exception);
assertEquals(2, break_count);
