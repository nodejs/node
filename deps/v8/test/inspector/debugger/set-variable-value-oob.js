// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { contextGroup, Protocol } = InspectorTest.start(
  'Tests mitigation of OOB stack write in SetExpression'
);

contextGroup.addScript(`
function trigger() {
  let a = 1;
  debugger;
  return a;
}
`);

Protocol.Debugger.enable();
Protocol.Debugger.onPaused(async message => {
  const callFrameId = message.params.callFrames[0].callFrameId;

  InspectorTest.log('Paused in trigger()');

  // Attempt to write a valid local variable to ensure the new CHECK bounds do not regress functionality.
  // Note: Triggering an actual OOB index requires a mismatched ScopeInfo and Frame size, which
  // usually requires bytecode corruption or a sandbox escape mechanism. This test ensures
  // the added mitigation does not crash valid use cases.
  const response = await Protocol.Debugger.setVariableValue({
    scopeNumber: 0,
    variableName: 'a',
    newValue: { value: 0x1337 },
    callFrameId
  });

  if (response.error) {
    InspectorTest.log('Error setting variable: ' + response.error.message);
  } else {
    InspectorTest.log('Successfully set variable a to 0x1337');
  }

  const evalResponse = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'a'
  });

  InspectorTest.log('Value of a: ' + evalResponse.result.result.value);

  await Protocol.Debugger.resume();
});

Protocol.Runtime.evaluate({ expression: 'trigger()' }).then(response => {
  InspectorTest.log('trigger() returned: ' + response.result.result.value);
  InspectorTest.completeTest();
});
