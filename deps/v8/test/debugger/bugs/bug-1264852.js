// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
Debug = debug.Debug

let listenerComplete = false;
let exceptionThrown = false;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      assertEquals(42, exec_state.frame(0).evaluate("42").value());
      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
   exceptionThrown = true;
  };
};

// Add the debug event listener.
Debug.setListener(listener);

assertEquals(
  42,
  (function f() {
     eval("var f = 42");
     debugger;
     return f;
   })()
);

Debug.setListener(null);

assertFalse(exceptionThrown, "exception in listener");
// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion");
