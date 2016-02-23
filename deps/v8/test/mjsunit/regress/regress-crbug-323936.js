// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

Debug = debug.Debug;

var step = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    if (step == 0) {
      assertEquals("error", exec_state.frame(0).evaluate("e").value());
      exec_state.frame(0).evaluate("e = 'foo'");
      exec_state.frame(0).evaluate("x = 'modified'");
    } else {
      assertEquals("argument", exec_state.frame(0).evaluate("e").value());
      exec_state.frame(0).evaluate("e = 'bar'");
    }
    step++;
  } catch (e) {
    print(e + e.stack);
    exception = e;
  }
}

Debug.setListener(listener);

function f(e, x) {
  try {
    throw "error";
  } catch(e) {
    debugger;
    assertEquals("foo", e);
  }
  debugger;
  assertEquals("bar", e);
  assertEquals("modified", x);
}

f("argument")
assertNull(exception);
assertEquals(2, step);
