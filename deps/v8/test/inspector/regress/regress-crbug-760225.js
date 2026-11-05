// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/760225.');

contextGroup.addScript(`
class Foo {}
class Bar extends Foo {
  constructor() {
    super();
    var b = () => this;
    this.c = 'b';  // <-- Inspect 'this' (it will be undefined)
    debugger;
  }
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'this'});
    InspectorTest.logMessage(result.result);
    Protocol.Debugger.resume();
  });
  await Protocol.Runtime.evaluate({expression: 'new Bar()'});
  InspectorTest.completeTest();
})();
