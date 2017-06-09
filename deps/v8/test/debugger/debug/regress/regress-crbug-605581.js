// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertThrows(() => exec_state.frame(0).evaluate("bar.baz"), ReferenceError);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

(function() {
  debugger;    // bar is still in TDZ at this point.
  let bar = 1;
  (x => bar);  // force bar to be context-allocated.
})();

Debug.setListener(null);
assertNull(exception);
