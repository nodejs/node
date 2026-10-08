// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start('Migrated d8 inspector test for crbug 502724368');

(async () => {
  await Protocol.Debugger.enable();

  const expression = 'class C { get #x() { return 1; } m() { C; debugger; return this.#x; } }; new C().m();';
  InspectorTest.log('Evaluating: ' + expression);
  const evalPromise = Protocol.Runtime.evaluate({expression});

  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log('Debugger paused!');

  const {callFrameId} = callFrames[0];
  const newValue = 0.00001111;

  InspectorTest.log('Setting #x to ' + newValue);
  const setVarResponse = await Protocol.Debugger.setVariableValue({
    scopeNumber: 1,
    variableName: '#x',
    newValue: { unserializableValue: String(newValue) },
    callFrameId: callFrameId
  });
  InspectorTest.logMessage(setVarResponse);

  InspectorTest.log('Evaluating this.#x');
  const evalOnFrameResponse = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: callFrameId,
    expression: 'this.#x'
  });
  InspectorTest.logMessage(evalOnFrameResponse);

  await Protocol.Debugger.resume();

  await evalPromise;
  InspectorTest.completeTest();
})();
