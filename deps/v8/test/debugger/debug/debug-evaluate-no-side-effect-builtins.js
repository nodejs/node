// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --side-effect-free-debug-evaluate

Debug = debug.Debug

var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    function success(expectation, source) {
      assertEquals(expectation, exec_state.frame(0).evaluate(source).value());
    }
    function fail(source) {
      assertThrows(() => exec_state.frame(0).evaluate(source), EvalError);
    }

    // Test Math functions.
    for (f of Object.getOwnPropertyNames(Math)) {
      if (typeof Math[f] === "function") {
        var result = exec_state.frame(0).evaluate(
                         `Math.${f}(0.5, -0.5);`).value();
        if (f != "random") assertEquals(Math[f](0.5, -0.5), result);
      }
    }

    // Test Number functions.
    for (f of Object.getOwnPropertyNames(Number)) {
      if (typeof Number[f] === "function") {
        success(Number[f](0.5), `Number.${f}(0.5);`);
      }
    }
    for (f of Object.getOwnPropertyNames(Number.prototype)) {
      if (typeof Number.prototype[f] === "function") {
        if (f == "toLocaleString") continue;
        success(Number(0.5)[f](5), `Number(0.5).${f}(5);`);
      }
    }

    // Test String functions.
    success(" ", "String.fromCodePoint(0x20)");
    success(" ", "String.fromCharCode(0x20)");
    for (f of Object.getOwnPropertyNames(String.prototype)) {
      if (typeof String.prototype[f] === "function") {
        // Do not expect locale-specific or regexp-related functions to work.
        if (f.indexOf("locale") >= 0) continue;
        if (f == "normalize") continue;
        if (f == "match") continue;
        if (f == "search") continue;
        if (f == "split") continue;
        success("abcd"[f](2), `"abcd".${f}(2);`);
      }
    }
    fail("'abcd'.match(/a/)");
    fail("'abcd'.replace(/a/)");
    fail("'abcd'.search(/a/)");
    fail("'abcd'.split(/a/)");

    // Test JSON functions.
    success('{"abc":[1,2]}', "JSON.stringify(JSON.parse('{\"abc\":[1,2]}'))");
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
