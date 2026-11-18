// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that numeric property keys are skipped when synchronizing scopes.');

contextGroup.addScript(`
function test() {
  var x = 123;
  var local_func = function() { return this; };
  debugger;
}
`);

(async function() {
  await Protocol.Debugger.enable();
  const evaluatePromise = Protocol.Runtime.evaluate({expression: 'test()'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: callFrames[0].callFrameId,
    expression: 'var obj = local_func(); obj[0] = 1;',
  });
  InspectorTest.logMessage(evalResult);
  await Protocol.Debugger.resume();
  await evaluatePromise;
  InspectorTest.completeTest();
})();
