// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests setting breakpoint in outer function while paused in generator.');

contextGroup.addScript(
    `
function main() {
  function* boo() {
    debugger;
    yield;
  }

  var gen = boo();
  gen.next();
  gen.next();
}
`,
    0, 0, 'test.js');

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  let n = 0;
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (n++ == 0) {
      InspectorTest.logMessage(await Protocol.Debugger.setBreakpointByUrl(
          {lineNumber: 1, columnNumber: 13, url: 'test.js'}));
    }
    Protocol.Debugger.resume();
  });
  await Protocol.Runtime.evaluate({expression: 'main()'});
  InspectorTest.completeTest();
})();
