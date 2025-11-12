// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug
var exception = null;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      var frame = exec_state.frame(0);
      try {
        assertTrue(frame.evaluate("f").value().startsWith("function f()"));
      } catch {
        assertTrue(frame.sourceLineText().endsWith("throws"));
      }
    }
  } catch(e) {
    exception = e;
    print(e, e.stack);
  }
};

Debug.setListener(listener);

(function f() {
  f;
  debugger;  // works
})();

(function f() {
  () => f;
  debugger;  // works
})();

(function f() {
  debugger;  // throws
})();

assertNull(exception);

Debug.setListener(null);
