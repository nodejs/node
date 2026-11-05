// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-ops.');

contextGroup.addScript(`
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
      returnByValue: true,
    });
    if (result.exceptionDetails) {
      InspectorTest.log(source + ' threw: ' + result.result.description);
      return;
    }
    const actual = result.result.value !== undefined ?
        JSON.stringify(result.result.value) :
        result.result.unserializableValue;
    InspectorTest.log(source + ': ' + actual);
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

  await success(false, `Object == {}`);
  await success(false, `Object === {}`);
  await success(true, `Object != {}`);
  await success(true, `Object !== {}`);
  await success(true, `'s' == string`);
  await success(true, `'s' === string`);
  await success(true, `1 < Math.cos(0) * 2`);
  await success(false, `1 < string`);
  await success(true, `'a' < string`);
  await success('s', `string[0]`);
  await success(0, `[0][0]`);
  await success(1, `T^F`);
  await success(0, `T&F`);
  await success(1, `T|F`);
  await success(false, `T&&F`);
  await success(true, `T||F`);
  await success(false, `T?F:T`);
  await success(false, `!T`);
  await success(1, `+one`);
  await success(-1, `-one`);
  await success(-2, `~one`);
  await success(4, `one << two`);
  await success(1, `two >> one`);
  await success(1, `two >>> one`);
  await success(3, `two + one`);
  await success(2, `two * one`);
  await success(0.5, `one / two`);
  await success(0, `(one / two) | 0`);
  await success(1, `one ** two`);
  await success(NaN, `string * two`);
  await success('s2', `string + two`);
  await success('s2', `string + two`);
  await success([1, 2, 3], `[...array]`);
  await success(3, `max(...array)`);
  await success({s: 1}, `({[string]:1})`);
  await fail(`[a, b] = [1, 2]`);
  await success(2, `def(2)`);
  await success(1, `def()`);
  await success('ab', `d1(['a'])`);
  await success('XYz', `d2({x:'X', y:'Y'})`);

  await Protocol.Debugger.resume();
  await evaluatePromise;
  InspectorTest.completeTest();
})();
