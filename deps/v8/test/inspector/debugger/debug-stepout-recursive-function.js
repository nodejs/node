// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping out of recursive functions.');

const source = `
var shouldBreak = null;
function fact(x) {
  if (shouldBreak(x)) {
    debugger;
  }
  if (x < 2) {
    return 1;
  } else {
    return x * fact(x - 1);
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let break_point_hit_count = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    break_point_hit_count++;
    if (callFrames.length > 1) {
      await Protocol.Debugger.stepOut();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  async function runTest(name, shouldBreakExpr, expectedHits) {
    break_point_hit_count = 0;
    await Protocol.Runtime.evaluate(
        {expression: `shouldBreak = ${shouldBreakExpr}; fact(3);`});
    InspectorTest.log(
        `${name}: break hits = ${break_point_hit_count} ` +
        `(expected ${expectedHits})`);
  }

  await runTest('Test 1', 'function(x) { return x == 3; }', 2);
  await runTest('Test 2', 'function(x) { return x == 2; }', 3);
  await runTest('Test 3', 'function(x) { return x == 1; }', 4);
  await runTest('Test 4', 'function(x) { return x == 1 || x == 3; }', 5);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
