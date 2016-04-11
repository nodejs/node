// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --debug-eval-readonly-locals

Debug = debug.Debug

var debug_step = 0;
var failure = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    if (debug_step == 0) {
      assertEquals(1, exec_state.frame(0).evaluate('a').value());
      assertEquals(3, exec_state.frame(0).evaluate('b').value());
      exec_state.frame(0).evaluate("a = 4").value();  // no effect.
      debug_step++;
    } else {
      assertEquals(1, exec_state.frame(0).evaluate('a').value());
      assertEquals(3, exec_state.frame(0).evaluate('b').value());
      exec_state.frame(0).evaluate("set_a_to_5()");
      exec_state.frame(0).evaluate("b = 5").value();  // no effect.
    }
  } catch (e) {
    failure = e;
  }
}

Debug.setListener(listener);

function* generator(a, b) {
  function set_a_to_5() { a = 5 }
  var b = 3;  // Shadows a parameter.
  debugger;
  yield a;
  yield b;
  debugger;
  yield a;
  return b;
}

var foo = generator(1, 2);

assertEquals(1, foo.next().value);
assertEquals(3, foo.next().value);
assertEquals(5, foo.next().value);
assertEquals(3, foo.next().value);
assertNull(failure);

Debug.setListener(null);
