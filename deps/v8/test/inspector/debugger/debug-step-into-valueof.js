// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into valueOf callback.');

const script = `
function valueOfCallback() {
  return 2                // Break 2.
  ;                       // Break 3.
}
var o = {};
o.valueOf = valueOfCallback;

debugger;                 // Break 0.
var result = 1 + o;       // Break 1.
let finished = true;      // Break 4.
`;

const lines = script.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  Protocol.Debugger.onPaused(async message => {
    const loc = message.params.callFrames[0].location;
    const lineText = lines[loc.lineNumber];
    InspectorTest.log(lineText);
    if (!lineText.includes(`Break ${breakCount++}.`)) {
      InspectorTest.log(`Unexpected break at: ${lineText}`);
    }
    if (breakCount === 5) {
      await Protocol.Debugger.resume();
    } else {
      await Protocol.Debugger.stepInto();
    }
  });

  await Protocol.Runtime.evaluate({expression: script});
  const {result} = await Protocol.Runtime.evaluate({expression: 'result'});
  InspectorTest.log(`result: ${result.result.value}`);

  InspectorTest.completeTest();
})();
