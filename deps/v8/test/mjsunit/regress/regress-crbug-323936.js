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
      exec_state.frame(0).evaluate("write_0('foo')");
      exec_state.frame(0).evaluate("write_1('modified')");
    } else {
      assertEquals("argument", exec_state.frame(0).evaluate("e").value());
      exec_state.frame(0).evaluate("write_2('bar')");
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
    // In ES2015 hoisting semantics, 'x' binds to the argument
    // and 'e' binds to the exception.
    function write_0(v) { e = v }
    function write_1(v) { x = v }
    debugger;
    assertEquals("foo", e);  // overwritten by the debugger
  }
  assertEquals("argument", e);  // debugger did not overwrite
  function write_2(v) { e = v }
  debugger;
  assertEquals("bar", e);
  assertEquals("modified", x);
}

f("argument")
assertNull(exception);
assertEquals(2, step);
