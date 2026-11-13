// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-builtins-2.');

contextGroup.addScript(`
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
}

function getDateFunctions() {
  const result = [];
  for (const f of Object.getOwnPropertyNames(Date.prototype)) {
    if (typeof Date.prototype[f] === "function") {
      if (f.startsWith("toLocale") && typeof Intl === "undefined") {
        continue;
      }
      result.push(f);
    }
  }
  return result;
}

function f() {
  debugger;
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  const evaluatePromise = Protocol.Runtime.evaluate({expression: 'f()'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const callFrameId = callFrames[0].callFrameId;

  async function success(expectation, source) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    if (result.exceptionDetails) {
      InspectorTest.log(source + ' threw: ' + result.result.description);
      return;
    }
    if (expectation !== undefined) {
      const actual = result.result.value !== undefined ?
          result.result.value :
          (result.result.unserializableValue || result.result.description);
      InspectorTest.log(source + ': ' + actual);
    }
  }

  async function fail(source) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    InspectorTest.log(
        source + ': ' +
        (result.exceptionDetails ? result.result.className : 'succeeded'));
  }

  // Test Function-related functions.
  await success('func', `(function func(){}).prototype.constructor.name`);
  await success('[object Object]', `(function func(){}).prototype.toString()`);
  await success(12, `add.bind(null, 10)(2)`);
  await success(3, `add.call(null, 1, 2)`);
  await success(3, `add.apply(null, [1, 2])`);

  // Test Date.prototype functions.
  await success(undefined, `Date()`);
  await success(undefined, `new Date()`);
  await success(undefined, `Date.now()`);
  await success(undefined, `Date.parse(1)`);
  const {result: {result: {value: dateFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getDateFunctions()',
        returnByValue: true,
      });
  for (const f of dateFunctions) {
    if (f.startsWith('set')) {
      await fail(`date.${f}(5);`);
    } else {
      await success(undefined, `date.${f}();`);
    }
  }

  // Test Boolean.
  await success(true, `Boolean(1)`);
  await success(new Boolean(true), `new Boolean(1)`);
  await success('true', `true.toString()`);
  await success(true, `true.valueOf()`);

  // Test global functions.
  await success(1, `parseInt("1")`);
  await success(1.3, `parseFloat("1.3")`);
  await success('abc', `decodeURI("abc")`);
  await success('abc', `encodeURI("abc")`);
  await success('abc', `decodeURIComponent("abc")`);
  await success('abc', `encodeURIComponent("abc")`);
  await success('abc', `escape("abc")`);
  await success('abc', `unescape("abc")`);
  await success(true, `isFinite(0)`);
  await success(true, `isNaN(0/0)`);
  await success('object', `typeof date`);
  await success('number', `get_number()`);

  // Test Map functions.
  await success(undefined, `new Map()`);
  await success('[object Map]', `map.toString()`);
  await success('b', `map.get("a")`);
  await success(true, `map.get("x") === undefined`);
  await success(undefined, `map.entries()`);
  await success(undefined, `map.keys()`);
  await success(undefined, `map.values()`);
  await success(undefined, `map.forEach(()=>1)`);
  await success(true, `map.has("c")`);
  await success(2, `map.size`);
  await success(undefined, `new Map([[1, 2]])`);
  await fail(`map.delete("a")`);
  await fail(`map.clear()`);
  await fail(`map.set("x", "y")`);

  // Test Set functions.
  await success(undefined, `new Set()`);
  await success('[object Set]', `set.toString()`);
  await success(undefined, `set.entries()`);
  await success(undefined, `set.keys()`);
  await success(undefined, `set.values()`);
  await success(undefined, `set.forEach(()=>1)`);
  await success(true, `set.has(1)`);
  await success(2, `set.size`);
  await success(1, `new Set([1]).size`);
  await fail(`set.add(2)`);
  await fail(`set.delete(1)`);
  await fail(`set.clear()`);

  // Test WeakMap functions.
  await success(undefined, `new WeakMap()`);
  await success('[object WeakMap]', `weak_map.toString()`);
  await success('a', `weak_map.get(weak_key)`);
  await success(true, `weak_map.get([]) === undefined`);
  await success(true, `weak_map.has(weak_key)`);
  await fail(`new WeakMap([[[], {}]])`);
  await fail(`weak_map.delete("a")`);
  await fail(`weak_map.set("x", "y")`);

  // Test WeakSet functions.
  await success(undefined, `new WeakSet()`);
  await success('[object WeakSet]', `weak_set.toString()`);
  await success(true, `weak_set.has(weak_key)`);
  await fail(`new WeakSet([[], {}])`);
  await fail(`weak_set.add([])`);
  await fail(`weak_set.delete("a")`);

  // Test BigInt functions.
  await success(10n, `BigInt('10')`);
  await success(10n, `BigInt.asIntN(10, 10n)`);
  await success(10n, `BigInt.asUintN(10, 10n)`);
  await success('10', `10n.toString()`);
  await success(10n, `10n.valueOf()`);

  await Protocol.Debugger.resume();
  await evaluatePromise;
  InspectorTest.completeTest();
})();
