// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-5164.');

contextGroup.addScript(`
function* gen(a, b) {
  debugger;
  yield a;
  yield b;
}

var foo = gen(1, 2);
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'arguments',
    });
    const {result: {result: props}} = await Protocol.Runtime.getProperties({
      objectId: result.objectId,
      ownProperties: true,
    });
    for (const prop of props) {
      if (prop.name === 'length' || prop.name === '0' || prop.name === '1') {
        InspectorTest.log(`${prop.name}: ${prop.value.value}`);
      }
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({
    expression: 'foo.next(); foo.next(); foo.next();',
  });
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
