// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through do-while loop with step-over.');

const source = `
function condition() {
  return false;
}

function runDoWhile() {
  do {
    debugger;
  } while(condition());
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  const lines = source.split('\n');
  let break_point_hit_count = 0;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const topFrame = callFrames[0];
    const funcName = topFrame.functionName || '(anonymous)';
    const lineText = topFrame.functionName === 'runDoWhile' ?
        lines[topFrame.location.lineNumber].trim() :
        '(caller)';

    InspectorTest.log(
        `Hit ${break_point_hit_count}: ${funcName} at "${lineText}"`);

    break_point_hit_count++;
    if (callFrames.length > 1) {
      await Protocol.Debugger.stepOver();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'runDoWhile()'});
  InspectorTest.log(`Total break count: ${break_point_hit_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
