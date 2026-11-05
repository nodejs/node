// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that setting a breakpoint on an active outer function does not break suspended inner generator objects.');

contextGroup.addScript(`
function main() {
  function* boo() {
    debugger;
    yield 42;
  }

  var gen = boo();
  gen.next();
  debugger;
  return gen.next();
}
`, 0, 0, 'test.js');

session.setupScriptMap();

(async function() {
  await Protocol.Debugger.enable();
  let pauseCount = 0;
  Protocol.Debugger.onPaused(async message => {
    pauseCount++;
    InspectorTest.log(`pause #${pauseCount}:`);
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (pauseCount === 2) {
      await Protocol.Debugger.setBreakpointByUrl({
        lineNumber: 1,
        columnNumber: 0,
        url: 'test.js',
      });
    }
    await Protocol.Debugger.resume();
  });
  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: 'main()',
    returnByValue: true,
  });
  InspectorTest.logObject(result.value);
  InspectorTest.completeTest();
})();
