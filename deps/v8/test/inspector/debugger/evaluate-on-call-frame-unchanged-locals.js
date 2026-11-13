// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that evaluateOnCallFrame only writes back changed local variables');

Protocol.Debugger.enable();
Protocol.Runtime.enable();

InspectorTest.runAsyncTestSuite([
  async function testShadowedEvalArgumentsNotClobbered() {
    contextGroup.addInlineScript(
        `
      function testShadowedEvalArguments() {
        eval("var arguments = 42;");
        debugger;
        return arguments;
      }
    `,
        'shadowed-arguments.js');

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testShadowedEvalArguments()',
      returnByValue: true,
    });
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    // Evaluating a side-effect-free expression on the call frame materializes
    // stack locals (including synthetic arguments object) onto the materialized
    // scope object. Because arguments is not mutated by the evaluation,
    // UpdateValues must not write it back, preventing it from falling through
    // to the context extension object and clobbering eval's "var arguments =
    // 42". Also verify adding an expando property on the scope object does not
    // get written back.
    const res = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: `
        Object.defineProperty(this, 'expandoProperty', {value: 123, writable: true, enumerable: true, configurable: true});
        1 + 1
      `,
    });
    InspectorTest.log('Eval 1+1 result: ' + res.result.result.value);

    await Protocol.Debugger.resume();
    const finalResult = await evalPromise;
    InspectorTest.log(
        'Return value of arguments: ' + finalResult.result.result.value);
  },

  async function testMutatedLocalsWrittenBack() {
    contextGroup.addInlineScript(
        `
      function testMutation() {
        let a = 10;
        let b = +0;
        let c = NaN;
        debugger;
        return [a, Object.is(b, -0), Number.isNaN(c)];
      }
    `,
        'mutation.js');

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testMutation()',
      returnByValue: true,
    });
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    // Mutate a (10 -> 20), b (+0 -> -0), and keep c as NaN (NaN -> NaN)
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'a = 20; b = -0; c = NaN;',
    });

    await Protocol.Debugger.resume();
    const finalResult = await evalPromise;
    InspectorTest.log(
        'Mutation result: ' + JSON.stringify(finalResult.result.result.value));
  },
]);
