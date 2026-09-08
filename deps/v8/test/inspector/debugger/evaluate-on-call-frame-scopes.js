// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start(`Test for Debugger.evaluateOnCallFrame with scopeNumber`);

Protocol.Debugger.enable();

InspectorTest.runAsyncTestSuite([
  async function testEvaluateOnScopes() {
    contextGroup.addInlineScript(`
      var z = 'global_z';
      let script_a = 'script_outer';
      function outer(y) {
        let a = 'closure_outer';
        let closure_y = y;
        () => a;
        function inner(x) {
          let a = 'inner_stack';
          debugger;
          return x + closure_y + a;
        }
        return inner(10);
      }
    `, 'test.js');

    Protocol.Runtime.evaluate({ expression: 'outer(20)' });
    const { params: { callFrames: [ { callFrameId, scopeChain } ] } } =
        await Protocol.Debugger.oncePaused();

    InspectorTest.log(`Paused with ${scopeChain.length} scopes in scopeChain.`);
    for (let i = 0; i < scopeChain.length; i++) {
      InspectorTest.log(`Scope ${i}: ${scopeChain[i].type}`);
    }

    async function evaluateAndLog(expression, scopeNumber) {
      const params = { callFrameId, expression };
      if (scopeNumber !== undefined) params.scopeNumber = scopeNumber;
      const response = await Protocol.Debugger.evaluateOnCallFrame(params);
      InspectorTest.log(`Eval "${expression}" at scopeNumber=${scopeNumber}:`);
      if (response.error) {
        InspectorTest.log(`  Error: ${response.error.message}`);
      } else if (response.result.exceptionDetails) {
        InspectorTest.log(`  Exception: ${response.result.exceptionDetails.exception.description.split('\\n')[0]}`);
      } else {
        InspectorTest.log(`  Result: ${response.result.result.value}`);
      }
    }

    await evaluateAndLog('a', undefined); // Default scope 0
    await evaluateAndLog('a', 0);         // Scope 0 ('inner_stack')
    await evaluateAndLog('a', 1);         // Scope 1 ('closure_outer')
    await evaluateAndLog('closure_y', 0); // Scope 0 (can see outer closure_y)
    await evaluateAndLog('closure_y', 1); // Scope 1 (can see closure_y)
    await evaluateAndLog('x', 0);         // Scope 0 (10)
    await evaluateAndLog('x', 1);         // Scope 1 (should throw ReferenceError)
    await evaluateAndLog('z', 0);         // Scope 0 ('global_z')
    await evaluateAndLog('z', 1);         // Scope 1 ('global_z')
    await evaluateAndLog('a', 999);       // Invalid scopeNumber
    await evaluateAndLog('a', -1);        // Invalid scopeNumber

    await Protocol.Debugger.resume();
  }
]);
