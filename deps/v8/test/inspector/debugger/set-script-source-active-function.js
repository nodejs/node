// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
  'Checks that setScriptSource fails for editing functions that are below the top-most frame on the stack');

const originalScript = `
function foo(b) {
  debugger;
  return b + 25;
}
function testExpression(a, b) {
  return a + foo(b);
}`;

contextGroup.addScript(originalScript);

const replacementScript = originalScript.replace('a + foo(b)', 'a * foo(b)');

session.setupScriptMap();

(async () => {
  Protocol.Debugger.enable();
  const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

  const evaluatePromise = Protocol.Runtime.evaluate({ expression: 'testExpression(3, 5)' });
  let { params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused at (before live edit):');
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
