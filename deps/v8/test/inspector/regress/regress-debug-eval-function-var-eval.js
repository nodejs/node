// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that evaluateOnCallFrame resolves context-allocated function ' +
    'variables and eval-declared shadowing vars.');

contextGroup.addScript(`
const fShadowed = function myFunc() {
  eval("var myFunc = 42");
  debugger;
  return myFunc;
};

const fSloppyEval = function myFunc() {
  eval("");
  debugger;
};

const fCaptured = function myFunc() {
  debugger;
  return () => myFunc;
};

const fStrict = function myFunc() {
  "use strict";
  debugger;
  return () => myFunc;
};
`);

(async function test() {
  await Protocol.Debugger.enable();

  {
    const evaluatePromise =
        Protocol.Runtime.evaluate({expression: 'fShadowed()'});
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();
    const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'myFunc',
    });
    InspectorTest.logMessage(evalResult);
    await Protocol.Debugger.resume();
    const {result: {result: {value}}} = await evaluatePromise;
    InspectorTest.log(`Returned: ${value}`);
  }

  {
    const evaluatePromise =
        Protocol.Runtime.evaluate({expression: 'fSloppyEval()'});
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();
    const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'myFunc',
    });
    InspectorTest.logMessage(evalResult);
    await Protocol.Debugger.resume();
    await evaluatePromise;
  }

  {
    const evaluatePromise =
        Protocol.Runtime.evaluate({expression: 'fCaptured()'});
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();
    const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'myFunc',
    });
    InspectorTest.logMessage(evalResult);
    await Protocol.Debugger.resume();
    await evaluatePromise;
  }

  {
    const evaluatePromise =
        Protocol.Runtime.evaluate({expression: 'fStrict()'});
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();
    const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'myFunc = 5',
    });
    InspectorTest.logMessage(evalResult);
    await Protocol.Debugger.resume();
    await evaluatePromise;
  }

  InspectorTest.completeTest();
})();
