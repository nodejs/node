// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --turbo

Debug = debug.Debug

var exception = null;
var object_with_symbol_key = {[Symbol("a")]: 1};
var object_with_callbacks = { toString: () => "string", valueOf: () => 3};
var symbol_for_a = Symbol.for("a");

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

    // Test some Object functions.
    success({}, `new Object()`);
    success({p : 3}, `Object.create({}, { p: { value: 3 } })`);
    success("[[\"a\",1],[\"b\",2]]",
            `JSON.stringify(Object.entries({a:1, b:2}))`);
    success({value: 1, writable: true, enumerable: true, configurable: true},
            `Object.getOwnPropertyDescriptor({a: 1}, "a")`);
    success("{\"a\":{\"value\":1,\"writable\":true," +
            "\"enumerable\":true,\"configurable\":true}}",
            `JSON.stringify(Object.getOwnPropertyDescriptors({a: 1}))`);
    success(["a"], `Object.getOwnPropertyNames({a: 1})`);
    success(undefined, `Object.getOwnPropertySymbols(object_with_symbol_key)`);
    success({}, `Object.getPrototypeOf(Object.create({}))`);
    success(true, `Object.is(Object, Object)`);
    success(true, `Object.isExtensible({})`);
    success(false, `Object.isFrozen({})`);
    success(false, `Object.isSealed({})`);
    success([1, 2], `Object.values({a:1, b:2})`);

    fail(`Object.assign({}, {})`);
    fail(`Object.defineProperties({}, [{p:{value:3}}])`);
    fail(`Object.defineProperty({}, {p:{value:3}})`);
    fail(`Object.freeze({})`);
    fail(`Object.preventExtensions({})`);
    fail(`Object.seal({})`);
    fail(`Object.setPrototypeOf({}, {})`);

    // Test some Object.prototype functions.
    success(true, `({a:1}).hasOwnProperty("a")`);
    success(true, `Object.prototype.isPrototypeOf({})`);
    success(true, `({a:1}).propertyIsEnumerable("a")`);
    success("[object Object]", `({a:1}).toString()`);
    success("string", `(object_with_callbacks).toString()`);
    success(3, `(object_with_callbacks).valueOf()`);

    // Test Array functions.
    success([], `new Array()`);
    var function_param = [
      "forEach", "every", "some", "reduce", "reduceRight", "find", "filter",
      "map", "findIndex"
    ];
    var fails = ["toString", "join", "toLocaleString", "pop", "push",
      "reverse", "shift", "unshift", "slice", "splice", "sort", "filter",
      "map", "copyWithin", "fill", "concat"];
    for (f of Object.getOwnPropertyNames(Array.prototype)) {
      if (typeof Array.prototype[f] === "function") {
        if (fails.includes(f)) {
          if (function_param.includes(f)) {
            fail(`[1, 2, 3].${f}(()=>{});`);
          } else {
            fail(`[1, 2, 3].${f}();`);
          }
        } else if (function_param.includes(f)) {
          exec_state.frame(0).evaluate(`[1, 2, 3].${f}(()=>{});`, true);
        } else {
          exec_state.frame(0).evaluate(`[1, 2, 3].${f}();`, true);
        }
      }
    }

    // Test Math functions.
    for (f of Object.getOwnPropertyNames(Math)) {
      if (typeof Math[f] === "function") {
        var result = exec_state.frame(0).evaluate(
                         `Math.${f}(0.5, -0.5);`, true).value();
        if (f != "random") assertEquals(Math[f](0.5, -0.5), result);
      }
    }

    // Test Number functions.
    success(new Number(0), `new Number()`);
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
    success(new String(), `new String()`);
    success(" ", "String.fromCodePoint(0x20)");
    success(" ", "String.fromCharCode(0x20)");
    for (f of Object.getOwnPropertyNames(String.prototype)) {
      if (typeof String.prototype[f] === "function") {
        // Do not expect locale-specific or regexp-related functions to work.
        // {Lower,Upper}Case (Locale-specific or not) do not work either
        // if Intl is enabled.
        if (f.indexOf("locale") >= 0) continue;
        if (f.indexOf("Locale") >= 0) continue;
        if (typeof Intl !== 'undefined') {
          if (f == "toUpperCase") continue;
          if (f == "toLowerCase") continue;
        }
        if (f == "normalize") continue;
        if (f == "match") continue;
        if (f == "search") continue;
        if (f == "split" || f == "replace") {
          fail(`'abcd'.${f}(2)`);
          continue;
        }
        success("abcd"[f](2), `"abcd".${f}(2);`);
      }
    }
    fail("'abCd'.toLocaleLowerCase()");
    fail("'abcd'.toLocaleUpperCase()");
    if (typeof Intl !== 'undefined') {
      fail("'abCd'.toLowerCase()");
      fail("'abcd'.toUpperCase()");
    }
    fail("'abcd'.match(/a/)");
    fail("'abcd'.replace(/a/)");
    fail("'abcd'.search(/a/)");
    fail("'abcd'.split(/a/)");

    // Test JSON functions.
    success('{"abc":[1,2]}', "JSON.stringify(JSON.parse('{\"abc\":[1,2]}'))");

    // Test Symbol functions.
    success(undefined, `Symbol("a")`);
    fail(`Symbol.for("a")`);  // Symbol.for can be observed via Symbol.keyFor.
    success("a", `Symbol.keyFor(symbol_for_a)`);
    success("Symbol(a)", `symbol_for_a.valueOf().toString()`);
    success("Symbol(a)", `symbol_for_a[Symbol.toPrimitive]().toString()`);
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
