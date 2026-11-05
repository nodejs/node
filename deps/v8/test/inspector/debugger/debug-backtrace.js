// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that debugger backtrace returns expected call frames and local ' +
    'variables.');

contextGroup.addScript(
    `
function f(x, y) {
  a=1;
};

var m = (0, function() {
  new f(1);
});

function g() {
  m();
};
`,
    0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 2, columnNumber: 0, url: 'test.js'});

  const pausedPromise = Protocol.Debugger.oncePaused();
  const evalPromise =
      Protocol.Runtime.evaluate({expression: 'g()\n//# sourceURL=expr.js'});

  const {params: {callFrames}} = await pausedPromise;
  InspectorTest.log(`Frame count: ${callFrames.length}`);
  session.logCallFrames(callFrames);

  for (let i = 0; i < 2; ++i) {
    const frame = callFrames[i];
    InspectorTest.log(`Frame ${i}: ${frame.functionName} at ${
        frame.location.lineNumber}:${frame.location.columnNumber}`);
    const localScope = frame.scopeChain.find(s => s.type === 'local');
    const {result: {result}} = await Protocol.Runtime.getProperties(
        {objectId: localScope.object.objectId, ownProperties: true});
    InspectorTest.log(`Frame ${i} local count: ${result.length}`);
    for (const prop of result) {
      InspectorTest.log(`  ${prop.name}: ${prop.value.value}`);
    }
  }

  await Protocol.Debugger.resume();
  await evalPromise;
  InspectorTest.completeTest();
})();
