// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { contextGroup, Protocol } = InspectorTest.start(
  'Tests setVariableValue with scopeNumber equal to scopeChain length (off-by-one boundary)'
);

contextGroup.addScript(`
function test() {
  let a = 10;
  debugger;
  return a;
}
`);

Protocol.Debugger.enable();
Protocol.Debugger.onPaused(async message => {
  const callFrame = message.params.callFrames[0];
  const callFrameId = callFrame.callFrameId;
  const numScopes = callFrame.scopeChain.length;

  InspectorTest.log('Paused in test(). Scope chain length: ' + numScopes);

  // Call setVariableValue with scopeNumber = numScopes (off-by-one out-of-bounds)
  InspectorTest.log('Calling setVariableValue with scopeNumber = ' + numScopes);
  const response = await Protocol.Debugger.setVariableValue({
    scopeNumber: numScopes,
    variableName: 'a',
    newValue: { value: 99 },
    callFrameId
  });

  if (response.error) {
    InspectorTest.log('Received error (expected): ' + response.error.message);
  } else {
    InspectorTest.log('SUCCESS (UNEXPECTED! Should have returned error)');
  }

  await Protocol.Debugger.resume();
});

Protocol.Runtime.evaluate({ expression: 'test()' }).then(response => {
  InspectorTest.log('test() finished executing.');
  InspectorTest.completeTest();
});
