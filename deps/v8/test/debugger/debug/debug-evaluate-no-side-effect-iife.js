// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --turbo

// Test that declaring local variables in IIFEs works with
// side-effect free debug-evaluate.

Debug = debug.Debug

var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    function success(expectation, source) {
      assertEquals(expectation,
                   exec_state.frame(0).evaluate(source, true).value());
    }
    function fail(source) {
      assertThrows(() => exec_state.frame(0).evaluate(source, true),
                   EvalError);
    }
    // Declaring 'a' sets a property to the global object.
    fail("var a = 3");
    exec_state.frame(0).evaluate("var a = 2", false);
    assertEquals(2, a);
    // Wrapping into an IIFE would be fine, since 'a' is local.
    success(100,
            `(function(x) {
                var a = 0;
                for (var i = 0; i < x; i++) {
                  a += x;
                }
                return a;
              })(10);`);
    success(100,
            `(x => {
                let a = 0;
                for (var i = 0; i < x; i++) {
                  a += x;
                }
                return a;
              })(10);`);
    // Not using 'var' to declare would make the access go to global object.
    fail(   `(function(x) {
                a = 0;
                for (var i = 0; i < x; i++) {
                  a += x;
                }
                return a;
              })(10);`);
  } catch (e) {
    exception = e;
    print(e, e.stack);
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  debugger;
};

f();

assertNull(exception);
