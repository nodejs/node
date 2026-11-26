// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that the (strict) eval scope is visible to the debugger.');

contextGroup.addScript(
    `
let scriptContextVar = 0;
function pauseInOuterScript() {
  debugger;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  async function getScopeProp(frame, scopeIndex, propName) {
    const scope = frame.scopeChain[scopeIndex];
    const {result: {result}} = await Protocol.Runtime.getProperties(
        {objectId: scope.object.objectId, ownProperties: true});
    const found = result.find(p => p.name === propName);
    return found ? found.value.value : '<undefined>';
  }

  async function evalOnFrame(frame, expression) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: frame.callFrameId, expression});
    if (result.exceptionDetails) {
      return `Exception: ${result.result.className}`;
    }
    return result.result.value;
  }

  async function setScopeVar(frame, scopeNumber, variableName, value) {
    await Protocol.Debugger.setVariableValue({
      callFrameId: frame.callFrameId,
      scopeNumber,
      variableName,
      newValue: {value}
    });
  }

  let delegate = null;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    await delegate(callFrames);
    await Protocol.Debugger.resume();
  });

  // Case 1 & 2: Current function is the top-level eval.
  // Stack- and context-allocated values in the eval-scope are accessible.
  delegate = async (callFrames) => {
    InspectorTest.log(`frame 0 scopes: ${
        callFrames[0].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(
        `scope(0).a: ${await getScopeProp(callFrames[0], 0, 'a')}`);
    InspectorTest.log(`eval('a'): ${await evalOnFrame(callFrames[0], 'a')}`);
    await setScopeVar(callFrames[0], 0, 'a', 2);
    InspectorTest.log(
        `eval('a++'): ${await evalOnFrame(callFrames[0], 'a++')}`);
  };

  InspectorTest.log('--- Case 1: stack-allocated in eval ---');
  await Protocol.Runtime.evaluate({
    expression: `eval(\`
      'use strict';
      var a = 1;
      debugger;
      if (a !== 3) throw new Error('Expected 3, got ' + a);
    \`);`
  });

  InspectorTest.log('--- Case 2: context-allocated in eval ---');
  await Protocol.Runtime.evaluate({
    expression: `eval(\`
      'use strict';
      var a = 1;
      (x => a);
      debugger;
      if (a !== 3) throw new Error('Expected 3, got ' + a);
    \`);`
  });

  // Case 3: Current function is an inner function.
  // Stack-allocated values in the eval-scope are not accessible.
  delegate = async (callFrames) => {
    InspectorTest.log(`frame 0 scopes: ${
        callFrames[0].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(`frame 1 scopes: ${
        callFrames[1].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(`eval('a'): ${await evalOnFrame(callFrames[0], 'a')}`);
    InspectorTest.log(
        `scope(0).a: ${await getScopeProp(callFrames[0], 0, 'a')}`);
  };
  InspectorTest.log('--- Case 3: inner function, stack-allocated in eval ---');
  await Protocol.Runtime.evaluate({
    expression: `eval(\`
      'use strict';
      var a = 1;
      (() => { debugger; })();
    \`);`
  });

  // Case 4: Current function is an escaped inner function.
  delegate = async (callFrames) => {
    InspectorTest.log(`frame 0 scopes: ${
        callFrames[0].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(`frame 1 scopes: ${
        callFrames[1].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(`eval('a'): ${await evalOnFrame(callFrames[0], 'a')}`);
    InspectorTest.log(
        `scope(0).a: ${await getScopeProp(callFrames[0], 0, 'a')}`);
  };
  InspectorTest.log(
      '--- Case 4: escaped inner function, stack-allocated in eval ---');
  await Protocol.Runtime.evaluate({
    expression: `var f = eval(\`
      'use strict';
      var a = 1;
      () => { debugger; }
    \`); f();`
  });

  // Case 5: Current function is an inner function.
  // Context-allocated values in the eval-scope are accessible.
  delegate = async (callFrames) => {
    InspectorTest.log(`frame 0 scopes: ${
        callFrames[0].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(`frame 1 scopes: ${
        callFrames[1].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(
        `frame 1 scope(0).a: ${await getScopeProp(callFrames[1], 0, 'a')}`);
    InspectorTest.log(
        `frame 1 eval('a'): ${await evalOnFrame(callFrames[1], 'a')}`);
    InspectorTest.log(
        `frame 0 eval('a'): ${await evalOnFrame(callFrames[0], 'a')}`);
    await setScopeVar(callFrames[1], 0, 'a', 2);
    InspectorTest.log(
        `frame 0 eval('a++'): ${await evalOnFrame(callFrames[0], 'a++')}`);
    InspectorTest.log(
        `frame 1 eval('a++'): ${await evalOnFrame(callFrames[1], 'a++')}`);
  };
  InspectorTest.log(
      '--- Case 5: inner function, context-allocated in eval ---');
  await Protocol.Runtime.evaluate({
    expression: `eval(\`
      'use strict';
      var a = 1;
      (() => {
        a;
        debugger;
        if (a !== 4) throw new Error('Expected 4, got ' + a);
      })();
    \`);`
  });

  // Case 6: Current function is an escaped inner function.
  // Context-allocated values in the eval-scope are accessible.
  delegate = async (callFrames) => {
    InspectorTest.log(`frame 0 scopes: ${
        callFrames[0].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(`frame 1 scopes: ${
        callFrames[1].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(
        `frame 0 scope(1).a: ${await getScopeProp(callFrames[0], 1, 'a')}`);
    InspectorTest.log(
        `frame 0 eval('a'): ${await evalOnFrame(callFrames[0], 'a')}`);
    await setScopeVar(callFrames[0], 1, 'a', 2);
    InspectorTest.log(
        `frame 0 eval('a++'): ${await evalOnFrame(callFrames[0], 'a++')}`);
  };
  InspectorTest.log(
      '--- Case 6: escaped inner function, context-allocated in eval ---');
  await Protocol.Runtime.evaluate({
    expression: `var g = eval(\`
      'use strict';
      var a = 1;
      () => {
        a;
        debugger;
        if (a !== 3) throw new Error('Expected 3, got ' + a);
      }
    \`); g();`
  });

  // Case 7: Inner function in eval calls a function in outer non-eval script.
  delegate = async (callFrames) => {
    InspectorTest.log(`frame 1 scopes: ${
        callFrames[1].scopeChain.map(s => s.type).join(',')}`);
    InspectorTest.log(
        `frame 1 eval('a'): ${await evalOnFrame(callFrames[1], 'a')}`);
  };
  InspectorTest.log(
      '--- Case 7: outer function called from inner function in eval ---');
  await Protocol.Runtime.evaluate({
    expression: `eval(\`
      'use strict';
      var a = 1;
      (() => {
        a;
        pauseInOuterScript();
      })();
    \`);`
  });

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
