// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
  'Checks that setScriptSource works for editing the top-most stack frame');

contextGroup.addScript(`
function testExpression(a, b) {
  debugger;
  return a + b;
}`);

const replacementScript = `
function testExpression(a, b) {
  return a * b;
}`;

session.setupScriptMap();

(async () => {
  Protocol.Debugger.enable();
  const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

  const evaluatePromise = Protocol.Runtime.evaluate({ expression: 'testExpression(3, 5)' });
  let { params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused at (before live edit):');
  await session.logSourceLocation(pausedCallFrames[0].location);

  Protocol.Debugger.setScriptSource({ scriptId, scriptSource: replacementScript, allowTopFrameEditing: true });
  ({ params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused());
  InspectorTest.log('Paused at (after live edit):');
  await session.logSourceLocation(pausedCallFrames[0].location, /* forceSourceRequest */ true);

  Protocol.Debugger.resume();

  const { result: { result } } = await evaluatePromise;
  InspectorTest.log('Result:');
  InspectorTest.logMessage(result);

  InspectorTest.completeTest();
})();
