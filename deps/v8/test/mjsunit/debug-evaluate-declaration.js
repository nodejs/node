// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

// Test that debug-evaluate only resolves variables that are used by
// the function inside which we debug-evaluate. This is to avoid
// incorrect variable resolution when a context-allocated variable is
// shadowed by a stack-allocated variable.

"use strict";

var Debug = debug.Debug

var exception = null;
function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    exec_state.frame(0).evaluate("var x = 2");
    exec_state.frame(0).evaluate("'use strict'; let y = 3");
    exec_state.frame(0).evaluate("var z = 4");
    exec_state.frame(0).evaluate("function bar() { return 5; }");
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

Debug.setListener(listener);

var z = 1;

(function() {
  debugger;
})();

assertEquals(2, x);                     // declaration
assertThrows(() => y, ReferenceError);  // let-declaration does not stick
assertEquals(4, z);                     // re-declaration
assertEquals(5, bar());                 // function declaration

Debug.setListener(null);
assertNull(exception);
