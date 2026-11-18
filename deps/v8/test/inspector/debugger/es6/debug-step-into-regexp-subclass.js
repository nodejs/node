// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping into RegExp subclass Symbol.split method.');

const source = `
function customSplit() {
  return "x"               // Break 2.
  ;                        // Break 3.
}
var o = {};
o[Symbol.split] = customSplit;

function run() {
  debugger;                  // Break 0.
  var result = "".split(o);  // Break 1.
  return result;             // Break 4.
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let break_count = 0;
  const max_breaks = 5;

  Protocol.Debugger.onPaused(async message => {
    let callFrames = message.params.callFrames;
    let loc = callFrames[0].location;
    let line = source.split('\n')[loc.lineNumber];
    InspectorTest.log(line.trim());
    if (!line.includes(`Break ${break_count}.`)) {
      InspectorTest.log(
          `FAILED: expected Break ${break_count}. but got: ${line.trim()}`);
    }
    break_count++;
    if (break_count < max_breaks) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'run()'});
  InspectorTest.log('result: ' + result.value);
  InspectorTest.log('total breaks: ' + break_count);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
