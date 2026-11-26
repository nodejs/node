// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --noanalyze-environment-liveness

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-5071.');

contextGroup.addScript(`
function f() {
  var a = 1;
  {
    let b = 2;
    debugger;
  }
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    const callFrames = message.params.callFrames;
    InspectorTest.log(`frameCount: ${callFrames.length}`);
    const localScope = callFrames[0].scopeChain.find(s => s.type === 'local');
    const {result: {result}} = await Protocol.Runtime.getProperties({
      objectId: localScope.object.objectId,
      ownProperties: true,
    });
    InspectorTest.log(`localCount: ${result.length}`);
    for (const prop of result) {
      InspectorTest.log(`${prop.name}: ${prop.value.value}`);
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
