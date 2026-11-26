// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that setting a breakpoint on an outer function does not break suspended inner generator objects.');

contextGroup.addScript(`
function factory() {
  return function* generator() {
    debugger;
    yield 42;
  };
}
function dummy() {}
`, 0, 0, 'test.js');

session.setupScriptMap();

(async function() {
  await Protocol.Debugger.enable();
  let pauseCount = 0;
  Protocol.Debugger.onPaused(async message => {
    pauseCount++;
    InspectorTest.log(`pause #${pauseCount}:`);
    await session.logSourceLocation(message.params.callFrames[0].location);
    await Protocol.Debugger.resume();
  });
  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 7,
    columnNumber: 0,
    url: 'test.js',
  });
  await Protocol.Runtime.evaluate({
    expression: `
var generator = factory();
var obj = generator();
obj.next();
`,
  });
  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 1,
    columnNumber: 0,
    url: 'test.js',
  });
  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: 'obj.next()',
    returnByValue: true,
  });
  InspectorTest.logObject(result.value);
  InspectorTest.completeTest();
})();
