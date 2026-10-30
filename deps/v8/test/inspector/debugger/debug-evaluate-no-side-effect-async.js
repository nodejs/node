// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-async.');

contextGroup.addScript(`
function* generator() {
  yield 1;
}

async function async() {
  return 1;
}

var g = generator();
var p = new Promise(() => {});

function f() {
  debugger;
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;

    async function fail(expression) {
      const {result} = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression, throwOnSideEffect: true});
      InspectorTest.log(`${expression}: ${
          result.exceptionDetails ? result.result.className : 'ok'}`);
    }

    async function succeed(expression) {
      const {result} = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression, throwOnSideEffect: true});
      InspectorTest.log(`${expression}: ${
          result.exceptionDetails ? result.result.className : 'ok'}`);
    }

    await fail('new Promise()');
    await fail('generator()');
    await fail('g.next()');
    await fail('Promise.resolve()');
    await fail('Promise.reject()');
    await fail('p.then(() => {})');
    await fail('p.catch(() => {})');
    await fail('p.finally(() => {})');
    await fail('Promise.all([p, p])');
    await fail('Promise.race([p, p])');
    await fail('(async function() { await 1; })()');

    await succeed('async()');
    await succeed('(async function() {})()');

    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.completeTest();
})();
