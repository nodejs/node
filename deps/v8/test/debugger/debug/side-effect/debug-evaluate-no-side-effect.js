// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

var exception = null;
let a = 1;
var object = { property : 2,
               get getter() { return 3; }
             };
var string0 = new String("string");
var string1 = { toString() { return "x"; } };
var string2 = { toString() { print("x"); return "x"; } };
var array = [4, 5];
var error = new Error();

function simple_return(x) { return x; }
function set_a() { a = 2; }
function get_a() { return a; }
var bound = get_a.bind(0);

function return_arg0() { return return_arg0.arguments[0]; }
function return_caller_name() { return return_caller_name.caller.name; }

var global_eval = eval;

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
    success(1, "bound()");
    success({}, "new get_a()");
    // Call to read-only function within try-catch.
    success(1, "try { get_a() } catch (e) {}");
    // Call to C++ built-in.
    success(Math.sin(2), "Math.sin(2)");
    // Call to whitelisted get accessors.
    success(3, "'abc'.length");
    success(2, "array.length");
    success(1, "'x'.length");
    success(0, "set_a.length");
    success("set_a", "set_a.name");
    success(0, "bound.length");
    success("bound get_a", "bound.name");
    success(1, "return_arg0(1)");
    success("f", "(function f() { return return_caller_name() })()");
    // Non-evaluated call.
    // Constructed literals.
    success([1], "[1]");
    success({x: 1}, "({x: 1})");
    success([1], "[a]");
    success({x: 1}, "({x: a})");
    // Test that template literal evaluation fails.
    fail("simple_return`1`");
    // Test that non-read-only code fails.
    fail("exception = 1");
    // Test that calling a non-read-only function fails.
    fail("set_a()");
    fail("new set_a()");
    // Test that implicit call to a non-read-only function fails.
    fail("string2 + 'y'");
    // Test that try-catch does not catch the EvalError.
    fail("try { set_a() } catch (e) {}");
    // Test that call to set accessor fails.
    fail("array.length = 4");
    fail("set_a.name = 'set_b'");
    fail("set_a.length = 1");
    fail("bound.name = 'bound'");
    fail("bound.length = 1");
    fail("set_a.prototype = null");
    // Test that call to non-whitelisted get accessor fails.
    fail("error.stack");
    // Call to set accessors with receiver check.
    success(1, "[].length = 1");
    success(1, "'x'.length = 1");
    fail("string0.length = 1");
    success(1, "(new String('abc')).length = 1");
    success("g", "(function(){}).name = 'g'");
    success(1, "(function(){}).length = 1");
    success("g", "get_a.bind(0).name = 'g'");
    success(1, "get_a.bind(0).length = 1");
    success(null, "(function(){}).prototype = null");
    success(true, "(new Error()).stack.length > 1");
    success("a", "(new Error()).stack = 'a'");
    // Eval is not allowed.
    fail("eval('Math.sin(1)')");
    fail("eval('exception = 1')");
    fail("global_eval('1')");
    success(1, "(() => { var a = 1; return a++; })()")
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
