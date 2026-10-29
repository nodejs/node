// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { contextGroup, Protocol } = InspectorTest.start(`Evaluate getting private member from Smi`);

async function runAndLog(expression) {
  Protocol.Runtime.evaluate({expression: 'debugger;'});
  const { params: { callFrames } } = await Protocol.Debugger.oncePaused();
  const frame = callFrames[0];
  InspectorTest.log(`Debugger.evaluateOnCallFrame: \`${expression}\``);
  const { result: { result } } =
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: frame.callFrameId,
      expression
    });
  InspectorTest.logMessage(result);
  Protocol.Debugger.resume();
}

InspectorTest.runAsyncTestSuite([
  async function evaluatePrivateMember() {
    Protocol.Debugger.enable();
    Protocol.Runtime.enable();

    await runAndLog('(1).#test = 1');
    await runAndLog('(1).#test');
    await runAndLog('(null).#test = 1');
    await runAndLog('(null).#test');
    await runAndLog('(undefined).#test = 1');
    await runAndLog('(undefined).#test');
    await runAndLog('(true).#test = 1');
    await runAndLog('(true).#test');
    await runAndLog('("str").#test = 1');
    await runAndLog('("str").#test');
    await runAndLog('Symbol("str").#test = 1');
    await runAndLog('Symbol("str").#test');

    InspectorTest.completeTest();
  }]
);
