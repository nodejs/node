// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --inspector-live-edit

const {session, contextGroup, Protocol} = InspectorTest.start(
  'Checks that setScriptSource works for editing the top-most stack frame');

contextGroup.addScript(`
function testExpression(a, b) {
  debugger;
  return a + b;
}`);

const replacementScript1 = `
function testExpression(a, b) {
  return a * b;
}`;

const replacementScript2 = `
function testExpression(a, b, c, d, e, f, g, h) {
  return a * b + String(h).length;  // +"undefined".length
}`;

const replacementScript3 = `
function testExpression(a, b) {
  return a * b + 3;
}`;

session.setupScriptMap();

(async () => {
  Protocol.Debugger.enable();
  const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

  const evaluatePromise = Protocol.Runtime.evaluate({ expression: 'testExpression(3, 5)' });
  let { params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused at (before live edit):');
  await session.logSourceLocation(pausedCallFrames[0].location);

  Protocol.Debugger.setScriptSource({ scriptId, scriptSource: replacementScript1, allowTopFrameEditing: true });
  ({ params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused());
  InspectorTest.log('Paused at (after live edit 1):');
  await session.logSourceLocation(pausedCallFrames[0].location, /* forceSourceRequest */ true);
  let { result: { result: r1 } } = await Protocol.Runtime.evaluate({ expression: 'testExpression(3, 5)' });
  InspectorTest.log('Nested result 1: ' + r1.value);

  Protocol.Debugger.setScriptSource({ scriptId, scriptSource: replacementScript2, allowTopFrameEditing: true });
  ({ params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused());
  InspectorTest.log('Paused at (after live edit 2):');
  await session.logSourceLocation(pausedCallFrames[0].location, /* forceSourceRequest */ true);
  let { result: { result: r2 } } = await Protocol.Runtime.evaluate({ expression: 'testExpression(3, 5)' });
  InspectorTest.log('Nested result 2: ' + r2.value);

  Protocol.Debugger.setScriptSource({ scriptId, scriptSource: replacementScript3, allowTopFrameEditing: true });
  ({ params: { callFrames: pausedCallFrames } } = await Protocol.Debugger.oncePaused());
  InspectorTest.log('Paused at (after live edit 3):');
  await session.logSourceLocation(pausedCallFrames[0].location, /* forceSourceRequest */ true);
  let { result: { result: r3 } } = await Protocol.Runtime.evaluate({ expression: 'testExpression(3, 5)' });
  InspectorTest.log('Nested result 3: ' + r3.value);

  Protocol.Debugger.resume();

  const { result: { result } } = await evaluatePromise;
  InspectorTest.log('Result:');
  InspectorTest.logMessage(result);

  InspectorTest.completeTest();
})();
