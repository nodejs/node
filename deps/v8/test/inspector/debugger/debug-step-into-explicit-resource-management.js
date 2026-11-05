// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into explicit resource management.');

const script = `
function f() {
  let a = 1;  // Break 2
  {
    using b = { // Break 3
      value: 2,
      [Symbol.dispose]() {
        console.log(42);  // Break 7
      }  // Break 8
    };
    using c = { // Break 4
      value: 3,
      [Symbol.dispose]() {
        console.log(43);  // Break 5
      }  // Break 6
    };
  }
  let d = 4;  // Break 9
};  // Break 10

debugger;                 // Break 0
f();                      // Break 1
let finished = true;      // Break 11
`;

const lines = script.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  Protocol.Debugger.onPaused(async message => {
    const loc = message.params.callFrames[0].location;
    const lineText = lines[loc.lineNumber];
    InspectorTest.log(lineText);
    if (!lineText.includes(`Break ${breakCount++}`)) {
      InspectorTest.log(`Unexpected break at: ${lineText}`);
    }
    if (breakCount === 12) {
      await Protocol.Debugger.resume();
    } else {
      await Protocol.Debugger.stepInto();
    }
  });

  await Protocol.Runtime.evaluate({expression: script});
  InspectorTest.completeTest();
})();
