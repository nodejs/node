// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

Debug = debug.Debug

var exception = null;

function f() {
  let a = 0;
  function g() {
    let a = 1;
    {
      let a = 2;
      debugger;  // Breakpoint.
      if (a !== 3) {
        // We cannot change stack locals in optimized frames.
        assertEquals(2, a);
        assertOptimized(g);
      }
    }
    assertEquals(1, a);
  }
  g.call(1);
  if (a !== 4) {
    // We cannot change stack locals in optimized frames.
    assertEquals(0, a);
    assertOptimized(f);
  }
}


function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    exec_state.frame(0).evaluate("a = 3");
    exec_state.frame(1).evaluate("a = 4");
    assertThrows(() => exec_state.frame(0).evaluate("this = 2"));
  } catch (e) {
    exception = e;
    print("Caught something. " + e + " " + e.stack);
  };
};

Debug.setListener(listener);

f();

Debug.setListener(null);
assertNull(exception);
