// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests ignition-debug-break.');

contextGroup.addScript(
    `
var break_count = 0;

function g() {
  throw 1;
}

function f() {
  try {
    g();                         // B1
  } catch (e) {}
  var x = break_count;           // B2
  return 1;                      // B3
}
`,
    0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    Protocol.Debugger.resume();
  });

  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 9, columnNumber: 0, url: 'test.js'});
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 11, columnNumber: 0, url: 'test.js'});
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 12, columnNumber: 0, url: 'test.js'});

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.completeTest();
})();
