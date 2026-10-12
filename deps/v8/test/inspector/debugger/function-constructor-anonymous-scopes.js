// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that functions created via the Function constructor do not ' +
    'expose a self-binding named "anonymous" to the debugger.');

contextGroup.addScript(`
var anonymous = 'global anonymous';
var direct = new Function('a', 'debugger; return anonymous;');
var nested = new Function('a',
    'return function inner() { debugger; return anonymous; }');
//# sourceURL=test.js`);

async function pauseAndCheck(expression) {
  const paused = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression});
  const {params: {callFrames: [callFrame]}} = await paused;
  InspectorTest.log(`Paused in '${callFrame.functionName}':`);
  for (let i = 0; i < callFrame.scopeChain.length; ++i) {
    const scope = callFrame.scopeChain[i];
    if (scope.type === 'global') continue;
    const {result: {result: variables}} = await Protocol.Runtime.getProperties(
        {objectId: scope.object.objectId, ownProperties: true});
    InspectorTest.log(
        `  [${i}] ${scope.type}: ${variables.map(v => v.name).join(', ')}`);
  }
  const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: callFrame.callFrameId,
    expression: 'typeof anonymous + ": " + String(anonymous).split("\\n")[0]'
  });
  InspectorTest.log(`  anonymous = ${result.value}`);
  await Protocol.Debugger.resume();
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();
  },

  async function testDirect() {
    await pauseAndCheck('direct(1)');
  },

  async function testNested() {
    await pauseAndCheck('nested(1)()');
  },
]);
