// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping into builtin function call (Array.prototype.slice).');

const source = `
var a = [1, 2, 3, 4, 5];

function testStepInArraySlice() {
  debugger;
  var s = Array.prototype.slice.call(a, 2, 3);
}  // expected line
`;

contextGroup.addScript(source, 0, 0, 'test.js');

const sourceLines = source.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  let state = 1;
  let reachedExpected = false;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const topFrame = callFrames[0];
    const lineText = sourceLines[topFrame.location.lineNumber].trim();
    if (state === 1) {
      InspectorTest.log(`Break 1: ${topFrame.functionName} at '${lineText}'`);
      state = 2;
      await Protocol.Debugger.stepInto();
    } else if (state === 2) {
      InspectorTest.log(`Break 2: ${topFrame.functionName} at '${lineText}'`);
      state = 3;
      await Protocol.Debugger.stepInto();
    } else if (state === 3) {
      InspectorTest.log(`Break 3: ${topFrame.functionName} at '${lineText}'`);
      if (topFrame.functionName === 'testStepInArraySlice' &&
          lineText.includes('expected line')) {
        reachedExpected = true;
      }
      state = 4;
      await Protocol.Debugger.resume();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'testStepInArraySlice();'});

  InspectorTest.log(`Reached expected line: ${reachedExpected}`);
  InspectorTest.log(`Final state: ${state}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
