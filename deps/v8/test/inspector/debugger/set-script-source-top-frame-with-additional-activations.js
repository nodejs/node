// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
  'Checks that setScriptSource fails when editing the top-most stack frame, but that function also has an activation further down the stack');

const script = `
function callsTestExpression(a, b) {
  return testExpression(a, b);
}

function testExpression(a, b) {
  debugger;
  if (!a && !b) {
    return callsTestExpression(5, 3);
  }
  return a + b;
}`;
const replacementScript = script.replace('a + b', 'a * b');

contextGroup.addScript(script);

session.setupScriptMap();

(async () => {
  Protocol.Debugger.enable();
  const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

  const evaluatePromise = Protocol.Runtime.evaluate({ expression: 'callsTestExpression()' });
  let { params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused();
  InspectorTest.log('First pause at (before live edit):');
  await session.logSourceLocation(pausedCallFrames[0].location);

  Protocol.Debugger.resume();
  ({ params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused());
  InspectorTest.log('Second pause at (before live edit):');
  await session.logSourceLocation(pausedCallFrames[0].location);

  const response = await Protocol.Debugger.setScriptSource({ scriptId, scriptSource: replacementScript, allowTopFrameEditing: true })
  InspectorTest.log('Debugger.setScriptSource result:');
  InspectorTest.logMessage(response.result);

  Protocol.Debugger.resume();

  const { result: { result } } = await evaluatePromise;
  InspectorTest.log('Evaluation result:');
  InspectorTest.logMessage(result);

  InspectorTest.completeTest();
})();
