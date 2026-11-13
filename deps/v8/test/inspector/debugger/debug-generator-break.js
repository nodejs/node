// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests breaking on breakpoints inside a generator with yield*.');

contextGroup.addScript(
    `
var break_count = 0;

function* g() {
  yield 1;
}

function* f() {
  yield* g();                    // B1
  if (break_count !== 2) throw new Error('Expected break_count === 2');  // B2
  return 1;                      // B3
}
`,
    0, 0, 'test.js');

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();

  await Protocol.Debugger.setBreakpointByUrl({lineNumber: 8, url: 'test.js'});
  await Protocol.Debugger.setBreakpointByUrl({lineNumber: 9, url: 'test.js'});
  await Protocol.Debugger.setBreakpointByUrl({lineNumber: 10, url: 'test.js'});

  Protocol.Debugger.onPaused(async (message) => {
    await Protocol.Runtime.evaluate({expression: 'break_count++;'});
    await session.logSourceLocation(message.params.callFrames[0].location);
    await Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate({
    expression: 'for (let _ of f()) {}; break_count;',
    returnByValue: true,
  });
  InspectorTest.log('hasException: ' + Boolean(result.exceptionDetails));
  InspectorTest.log('break_count: ' + result.result.value);
  InspectorTest.completeTest();
})();
