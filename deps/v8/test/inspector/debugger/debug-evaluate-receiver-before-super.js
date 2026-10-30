// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-receiver-before-super.');

contextGroup.addScript(`
class A { constructor () { this.a = 239; } }
class B extends A {
  constructor () {
    debugger;
    super();
    debugger;
  }
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'this.a'});
    InspectorTest.logMessage(result);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'new B()'});
  InspectorTest.completeTest();
})();
