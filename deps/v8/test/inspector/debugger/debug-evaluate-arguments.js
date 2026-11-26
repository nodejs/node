// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests evaluating arguments object on unoptimized and optimized frames.');

contextGroup.addScript(
    `
function foo(a) {
  function bar(a, b, c) {
    debugger;
    return a + b + c;
  }
  return bar(1, 2, a);
}
%PrepareFunctionForOptimization(foo);
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  async function evalArguments(callFrame) {
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrame.callFrameId, expression: 'arguments'});
    const {result: {result: props}} = await Protocol.Runtime.getProperties(
        {objectId: result.objectId, ownProperties: true});
    const obj = {};
    props.forEach(elem => {
      const key = elem.name;
      let value;
      if (elem.value) {
        value = elem.value.type === 'undefined' ? undefined : elem.value.value;
      }
      obj[key] = value;
    });
    const values = [];
    for (let i = 0; i < obj.length; i++) {
      values[i] = obj[i];
    }
    return values;
  }

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const fooArgs = await evalArguments(callFrames[1]);
    const barArgs = await evalArguments(callFrames[0]);
    InspectorTest.log(`foo arguments: [${fooArgs.join(', ')}]`);
    InspectorTest.log(`bar arguments: [${barArgs.join(', ')}]`);
    await Protocol.Debugger.resume();
  });

  let {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'foo(3)'});
  InspectorTest.log(`Run 1 result: ${result.value}`);

  ({result: {result}} =
       await Protocol.Runtime.evaluate({expression: 'foo(3)'}));
  InspectorTest.log(`Run 2 result: ${result.value}`);

  ({result: {result}} = await Protocol.Runtime.evaluate(
       {expression: '%OptimizeFunctionOnNextCall(foo); foo(3)'}));
  InspectorTest.log(`Run 3 (optimized) result: ${result.value}`);

  ({result: {result}} = await Protocol.Runtime.evaluate({
    expression: '%PrepareFunctionForOptimization(foo); ' +
        '%OptimizeFunctionOnNextCall(foo); foo(3, 4, 5)'
  }));
  InspectorTest.log(`Run 4 (optimized extra args) result: ${result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
