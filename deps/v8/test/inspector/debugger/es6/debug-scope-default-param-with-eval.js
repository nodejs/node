// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that the parameter initialization block scope set up for ' +
    'sloppy eval is visible to the debugger.');

const source = `
function call_for_break() {
  return 5;
}

function test(x = eval("var y = 7; debugger; y") + call_for_break()) {
  return x;
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let break_count = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let frame = callFrames[0];
    let scopeTypes = frame.scopeChain.map(s => s.type);
    InspectorTest.log(`Break ${break_count} scopes: ${scopeTypes.join(',')}`);

    let blockScope = frame.scopeChain[break_count === 0 ? 1 : 0];
    let {result: {result}} = await Protocol.Runtime.getProperties(
        {objectId: blockScope.object.objectId});
    let yProp = result.find(p => p.name === 'y');
    InspectorTest.log(
        `Break ${break_count} y: ${yProp ? yProp.value.value : undefined}`);

    if (break_count++ === 0) {
      await Protocol.Debugger.stepOut();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  let {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'test()'});
  InspectorTest.log(`test() result: ${result.value}`);
  InspectorTest.log(`Total breaks: ${break_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
