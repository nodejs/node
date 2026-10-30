// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-nested-arrow-default-param-this.');

contextGroup.addScript(`
const obj = { x: 42 };
function m() {
  return () => (a = this) => {
    debugger;
    return a;
  };
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'this === obj'});
    InspectorTest.log('this === obj: ' + result.value);
    Protocol.Debugger.resume();
  });

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'm.call(obj)()() === obj'});
  InspectorTest.log('returned obj: ' + result.value);
  InspectorTest.completeTest();
})();
