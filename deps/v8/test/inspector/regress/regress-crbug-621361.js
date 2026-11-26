// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-621361.');

(async function test() {
  await Protocol.Debugger.enable();
  let steps = 0;
  Protocol.Debugger.onPaused(async message => {
    const frame = message.params.callFrames[0];
    const scopes = frame.scopeChain.map(s => s.type);
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: frame.callFrameId, expression: 'String(x)'});
    InspectorTest.log(
        `step ${steps}: scopes=[${scopes.join(', ')}], x=${result.value}`);
    if (steps < 2) {
      Protocol.Debugger.stepInto();
    } else {
      Protocol.Debugger.resume();
    }
    steps++;
  });

  await Protocol.Runtime.evaluate(
      {expression: '(function() {\n  debugger;\n  var x = l => l;\n})();'});
  InspectorTest.completeTest();
})();
