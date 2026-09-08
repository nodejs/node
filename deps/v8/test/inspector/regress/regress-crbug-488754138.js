// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} =
    InspectorTest.start('Regression test for crbug.com/488754138');

InspectorTest.runAsyncTestSuite([async function repro() {
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({
    expression:
        'async function targetFunction(){let x=await Promise.resolve(42);debugger;return x;} targetFunction();'
  });
  while (true) {
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    const frame = callFrames[0];
    if (frame.functionName === 'targetFunction') {
      const result =
          await Protocol.Debugger.setReturnValue({newValue: {value: 123}});
      if (result.error) {
        await Protocol.Debugger.stepInto();
        continue;
      }
      await Protocol.Debugger.stepOut();
      break;
    }
    await Protocol.Debugger.resume();
  }
}]);
