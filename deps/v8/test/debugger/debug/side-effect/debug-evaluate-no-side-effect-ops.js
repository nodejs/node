// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

var exception = null;
var date = new Date();
var T = true;
var F = false;
var one = 1;
var two = 2;
var string = "s";
var array = [1, 2, 3];
function max(...rest) {
  return Math.max(...rest);
}

function def(a = 1) {
  return a;
}

function d1([a, b = 'b']) {
  return a + b;
}

function d2({ x: c, y, z = 'z' } = {x: 'x', y: 'y' }) {
  return c + y + z;
}

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
    success(false, `Object == {}`);
    success(false, `Object === {}`);
    success(true, `Object != {}`);
    success(true, `Object !== {}`);
    success(true, `'s' == string`);
    success(true, `'s' === string`);
    success(true, `1 < Math.cos(0) * 2`);
    success(false, `1 < string`);
    success(true, `'a' < string`);
    success("s", `string[0]`);
    success(0, `[0][0]`);
    success(1, `T^F`);
    success(0, `T&F`);
    success(1, `T|F`);
    success(false, `T&&F`);
    success(true, `T||F`);
    success(false, `T?F:T`);
    success(false, `!T`);
    success(1, `+one`);
    success(-1, `-one`);
    success(-2, `~one`);
    success(4, `one << two`);
    success(1, `two >> one`);
    success(1, `two >>> one`);
    success(3, `two + one`);
    success(2, `two * one`);
    success(0.5, `one / two`);
    success(0, `(one / two) | 0`);
    success(1, `one ** two`);
    success(NaN, `string * two`);
    success("s2", `string + two`);
    success("s2", `string + two`);
    fail(`[...array]`);
    success(3, `max(...array)`);
    fail(`({[string]:1})`);
    fail(`[a, b] = [1, 2]`);
    success(2, `def(2)`);
    success(1, `def()`);
    fail(`d1(['a'])`);  // Iterator.prototype.next performs stores.
    success("XYz", `d2({x:'X', y:'Y'})`);
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
