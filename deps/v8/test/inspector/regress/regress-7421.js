// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that side-effect check is not bypassed in optimized code.');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    let callFrameId = message.params.callFrames[0].callFrameId;

    async function evaluate(expression, throwOnSideEffect = false) {
      return await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression, throwOnSideEffect});
    }

    async function fail(expression) {
      let res = await evaluate(expression, true);
      if (res.result.exceptionDetails) {
        InspectorTest.log(`fail("${expression}"): ${
            res.result.exceptionDetails.exception.className}`);
      } else {
        InspectorTest.log(`fail("${expression}"): DID NOT THROW`);
      }
    }

    await evaluate('wrapper1();');
    await evaluate('wrapper1();');
    await fail('wrapper1()');

    await evaluate('%PrepareFunctionForOptimization(wrapper2);');
    await evaluate('wrapper2(true);');
    await evaluate('wrapper2(false);');
    await evaluate('wrapper2(true);');
    await evaluate('%OptimizeFunctionOnNextCall(wrapper2);');
    await evaluate('wrapper2(false);');
    await fail('wrapper2(true)');
    await fail(
        '%PrepareFunctionForOptimization(wrapper2); ' +
        '%OptimizeFunctionOnNextCall(wrapper2); wrapper2(true)');

    await evaluate('%PrepareFunctionForOptimization(wrapper2);');
    await evaluate('%DisableOptimizationFinalization();');
    await evaluate('%OptimizeFunctionOnNextCall(wrapper2, "concurrent");');
    await evaluate('wrapper2(false);');
    await fail('%FinalizeOptimization(); wrapper2(true);');

    await Protocol.Debugger.resume();
    await Protocol.Debugger.disable();
    InspectorTest.completeTest();
  });

  const script = `
    var counter = 0;

    function f1() {
      counter++;
    }

    function wrapper1() {
      for (var i = 0; i < 4; i++) {
        %OptimizeOsr();
        %PrepareFunctionForOptimization(wrapper1);
      }
      f1();
    }
    %PrepareFunctionForOptimization(wrapper1);

    function f2() {
      counter++;
    }

    function wrapper2(call) {
      if (call) f2();
    }

    function f() {
      debugger;
    }

    f();
  `;

  contextGroup.addScript(script, 0, 0, 'test.js');
})();
