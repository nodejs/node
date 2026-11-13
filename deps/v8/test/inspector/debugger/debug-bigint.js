// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests evaluating BigInt expressions on a call frame.');

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evalPromise = Protocol.Runtime.evaluate(
      {expression: '!function() { debugger; }();\n0;'});
  const {params: {callFrames}} = await pausedPromise;
  const {result} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: callFrames[0].callFrameId,
    expression: '42n',
    throwOnSideEffect: true
  });
  InspectorTest.logMessage(result);
  await Protocol.Debugger.resume();
  await evalPromise;
  InspectorTest.completeTest();
})();
