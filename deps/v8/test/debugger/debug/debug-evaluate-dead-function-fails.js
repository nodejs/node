// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-always-turbofan

Debug = debug.Debug

var exception = null;
function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    // Evaluating the live function should succeed.
    assertEquals(exec_state.frame(0).evaluate("live()").value(), 1);
    // Evaluating the dead function should fail.
    assertThrows(()=>exec_state.frame(0).evaluate("dead()"), ReferenceError);
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

Debug.setListener(listener);

(function() {
  "use strict";
  function live() { return 1; }
  function dead() { return 2; }
  // Use 'foo' to make it non-dead.
  live;
  debugger;
})();

Debug.setListener(null);
assertNull(exception);
