// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping into callback from optimized string replace.');

const source = `
function replaceCallback(a) {
  return "x"                                    // Break 2.
  ;                                             // Break 3.
}

var re = /x/g;
// Optimize the inner helper function for string replace.
for (var i = 0; i < 10000; i++) "x".replace(re, replaceCallback);

function testRun() {
  debugger;                                       // Break 0.
  var result = "x".replace(re, replaceCallback);  // Break 1.
  return result;                                  // Break 4.
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

const sourceLines = source.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const topFrame = callFrames[0];
    const lineText = sourceLines[topFrame.location.lineNumber].trim();
    InspectorTest.log(`Break ${breakCount}: ${lineText}`);
    const expectedBreak = `Break ${breakCount}.`;
    if (!lineText.includes(expectedBreak)) {
      InspectorTest.log(`ERROR: Expected '${expectedBreak}' in '${lineText}'`);
    }
    breakCount++;
    if (breakCount <= 4) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'testRun();'});

  InspectorTest.log(`Total breaks: ${breakCount}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
