// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that evaluating an async generator on a call frame with a colliding ' +
    'unique ID in script does not crash.');

const source = `
function testFunction() {
  let outer_var = 1; // force context
  {
    let a = 1; // this block scope will have context slots
    debugger;  // Pause inside this block scope
    function inner() { return a + outer_var; } // force context allocation
  }
}
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: 'testFunction()'});
  const {params: {callFrames}} = await pausedPromise;
  const callFrameId = callFrames[0].callFrameId;

  for (let pad = 0; pad < 150; pad++) {
    const padding = ' '.repeat(pad);
    const expression = padding + `(() => {
      let g = (async function*() {
        for await (let [v0, v1, v2, v3, v4] of [[1, 2, 3, 4, 5]]) {
          function closure() { return v0 + v1 + v2 + v3 + v4; }
        }
      })();
      g.next();
      return g;
    })()`;

    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression,
    });
    if (result && result.objectId) {
      await Protocol.Runtime.getProperties({
        objectId: result.objectId,
        generatePreview: true,
      });
    }
  }

  InspectorTest.log('Success');
  InspectorTest.completeTest();
})();
