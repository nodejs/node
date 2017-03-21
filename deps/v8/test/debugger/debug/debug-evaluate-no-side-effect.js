// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --side-effect-free-debug-evaluate

Debug = debug.Debug

var exception = null;
let a = 1;
var object = { property : 2,
               get getter() { return 3; }
             };
var string1 = { toString() { return "x"; } };
var string2 = { toString() { print("x"); return "x"; } };
var array = [4, 5];
var error = new Error();

function set_a() { a = 2; }

function get_a() { return a; }

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    function success(expectation, source) {
      assertEquals(expectation, exec_state.frame(0).evaluate(source).value());
    }
    function fail(source) {
      assertThrows(() => exec_state.frame(0).evaluate(source), EvalError);
    }
    // Simple test.
    success(3, "1 + 2");
    // Dymanic load.
    success(array, "array");
    // Context load.
    success(1, "a");
    // Global and named property load.
    success(2, "object.property");
    // Load via read-only getter.
    success(3, "object.getter");
    // Implicit call to read-only toString.
    success("xy", "string1 + 'y'");
    // Keyed property load.
    success(5, "array[1]");
    // Call to read-only function.
    success(1, "get_a()");
    // Call to read-only function within try-catch.
    success(1, "try { get_a() } catch (e) {}");
    // Call to C++ built-in.
    success(Math.sin(2), "Math.sin(2)");
    // Call to whitelisted get accessors.
    success(3, "'abc'.length");
    success(2, "array.length");
    // Test that non-read-only code fails.
    fail("exception = 1");
    // Test that calling a non-read-only function fails.
    fail("set_a()");
    // Test that implicit call to a non-read-only function fails.
    fail("string2 + 'y'");
    // Test that try-catch does not catch the EvalError.
    fail("try { set_a() } catch (e) {}");
    // Test that call to set accessor fails.
    fail("array.length = 4");
    // Test that call to non-whitelisted get accessor fails.
    fail("error.stack");
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
assertEquals(1, a);
