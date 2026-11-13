// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through generator yield expressions.');

const source = `function * g() {
  debugger;  // B0
  yield 1;   // B1
  yield 2;   // B2 step out
  yield 3;   // B5
  yield 4;   // B6 step out
  return 2 * (yield 5);
}

function assertEquals(expected, actual) {
  if (expected !== actual) throw new Error('Expected ' + expected + ', got ' + actual);
}

var i = g();
assertEquals(1, i.next().value);
assertEquals(2, i.next().value);   // B3
assertEquals(3, i.next().value);   // B4 step in
assertEquals(4, i.next().value);   // B7
assertEquals(5, i.next().value);   // B8
assertEquals(6, i.next(3).value);  // B9 stop
`;

(async function test() {
  await Protocol.Debugger.enable();

  let breaks = 0;
  const lines = source.split('\n');

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let loc = callFrames[0].location;
    let line = lines[loc.lineNumber];
    InspectorTest.log('paused at: ' + line.trim());
    if (!RegExp(`B${breaks}`).test(line)) {
      InspectorTest.log(`FAIL: expected B${breaks} in line: ${line}`);
    }
    breaks++;
    if (/stop/.test(line)) {
      await Protocol.Debugger.resume();
    } else if (/step out/.test(line)) {
      await Protocol.Debugger.stepOut();
    } else if (/step in/.test(line)) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.stepOver();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await InspectorTest.waitForPendingTasks();

  InspectorTest.log('breaks: ' + breaks);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
