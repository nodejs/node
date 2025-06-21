// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
  InspectorTest.start(`Don't crash while paused in a class method and evaluating 'this'`);

(async () => {
  await Protocol.Debugger.enable();
  contextGroup.addScript(`
  class A {
    test() {
      debugger;
    }
    f = (x) => {}
  }
  const a = new A();
  a.test();
  `, 0, 0, 'test.js');

  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'run()'});

  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const frame = callFrames[0];

  const { result: { result } } =
    await Protocol.Debugger.evaluateOnCallFrame({ callFrameId: frame.callFrameId, expression: 'this' });
  InspectorTest.logMessage(result);

  InspectorTest.completeTest();
})();
