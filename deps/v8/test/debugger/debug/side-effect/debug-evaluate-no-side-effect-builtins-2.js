// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

var exception = null;
var date = new Date();
var map = new Map().set("a", "b").set("c", "d");
var set = new Set([1, 2]);
var weak_key = [];
var weak_map = new WeakMap().set(weak_key, "a").set({}, "b");
var weak_set = new WeakSet([weak_key, {}]);
var add = function (a, b) { return a + b; };
var number_value = 13;
function get_number() {
  return typeof(number_value);
};

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    function success(expectation, source) {
      var result = exec_state.frame(0).evaluate(source, true).value();
      if (expectation !== undefined) assertEquals(expectation, result);
    }
    function fail(source) {
      assertThrows(() => exec_state.frame(0).evaluate(source, true),
                   EvalError);
    }

    // Test Function-related functions.
    success("func", `(function func(){}).prototype.constructor.name`);
    success("[object Object]", `(function func(){}).prototype.toString()`);
    success(12, `add.bind(null, 10)(2)`);
    success(3, `add.call(null, 1, 2)`);
    success(3, `add.apply(null, [1, 2])`);

    // Test Date.prototype functions.
    success(undefined, `Date()`);
    success(undefined, `new Date()`);
    success(undefined, `Date.now()`);
    success(undefined, `Date.parse(1)`);
    for (f of Object.getOwnPropertyNames(Date.prototype)) {
      if (typeof Date.prototype[f] === "function") {
        if (f.startsWith("set")) {
          fail(`date.${f}(5);`, true);
        } else if (f.startsWith("toLocale")) {
          if (typeof Intl === "undefined") continue;
          fail(`date.${f}();`, true);
        } else {
          success(undefined, `date.${f}();`, true);
        }
      }
    }

    // Test Boolean.
    success(true, `Boolean(1)`);
    success(new Boolean(true), `new Boolean(1)`);
    success("true", `true.toString()`);
    success(true, `true.valueOf()`);

    // Test global functions.
    success(1, `parseInt("1")`);
    success(1.3, `parseFloat("1.3")`);
    success("abc", `decodeURI("abc")`);
    success("abc", `encodeURI("abc")`);
    success("abc", `decodeURIComponent("abc")`);
    success("abc", `encodeURIComponent("abc")`);
    success("abc", `escape("abc")`);
    success("abc", `unescape("abc")`);
    success(true, `isFinite(0)`);
    success(true, `isNaN(0/0)`);
    success("object", `typeof date`);
    success("number", `get_number()`);

    // Test Map functions.
    success(undefined, `new Map()`);
    success("[object Map]", `map.toString()`);
    success("b", `map.get("a")`);
    success(true, `map.get("x") === undefined`);
    success(undefined, `map.entries()`);
    success(undefined, `map.keys()`);
    success(undefined, `map.values()`);
    success(undefined, `map.forEach(()=>1)`);
    success(true, `map.has("c")`);
    success(2, `map.size`);
    success(undefined, `new Map([[1, 2]])`);
    fail(`map.delete("a")`);
    fail(`map.clear()`);
    fail(`map.set("x", "y")`);

    // Test Set functions.
    success(undefined, `new Set()`);
    success("[object Set]", `set.toString()`);
    success(undefined, `set.entries()`);
    success(undefined, `set.keys()`);
    success(undefined, `set.values()`);
    success(undefined, `set.forEach(()=>1)`);
    success(true, `set.has(1)`);
    success(2, `set.size`);
    success(1, `new Set([1]).size`);
    fail(`set.add(2)`);
    fail(`set.delete(1)`);
    fail(`set.clear()`);

    // Test WeakMap functions.
    success(undefined, `new WeakMap()`);
    success("[object WeakMap]", `weak_map.toString()`);
    success("a", `weak_map.get(weak_key)`);
    success(true, `weak_map.get([]) === undefined`);
    success(true, `weak_map.has(weak_key)`);
    fail(`new WeakMap([[[], {}]])`);
    fail(`weak_map.delete("a")`);
    fail(`weak_map.set("x", "y")`);

    // Test WeakSet functions.
    success(undefined, `new WeakSet()`);
    success("[object WeakSet]", `weak_set.toString()`);
    success(true, `weak_set.has(weak_key)`);
    fail(`new WeakSet([[], {}])`);
    fail(`weak_set.add([])`);
    fail(`weak_set.delete("a")`);

    // Test BigInt functions.
    success(10n, `BigInt('10')`);
    success(10n, `BigInt.asIntN(10, 10n)`);
    success(10n, `BigInt.asUintN(10, 10n)`);
    success("10", `10n.toString()`);
    success(10n, `10n.valueOf()`);
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
