// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test evaluating await expression on a breakpoint');

contextGroup.addScript(`
async function run() {
  debugger;
}`);

InspectorTest.runAsyncTestSuite([async function testScopesPaused() {
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'run()'});

  let {params: {callFrames}} =
      await Protocol.Debugger.oncePaused();  // inside a.fn()

  ({result} = await Protocol.Debugger.evaluateOnCallFrame({
    expression: 'await Promise.resolve(0)',
    callFrameId: callFrames[0].callFrameId
  }));

  InspectorTest.log('Evaluating await expression');
  InspectorTest.logObject(result);

  ({result} = await Protocol.Debugger.evaluateOnCallFrame({
    expression:
        'async function nested() { await fetch(\'http://google.com\'); } nested()',
    callFrameId: callFrames[0].callFrameId
  }));

  InspectorTest.log('Evaluating await expression in async function');
  InspectorTest.logObject(result);

  Protocol.Debugger.resume();
  Protocol.Debugger.disable();
}]);
