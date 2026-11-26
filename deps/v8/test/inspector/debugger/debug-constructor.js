// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into constructors.');

contextGroup.addScript(
    `
function a() {
  new c();
}

function b() {
  x = 1;
  new c();
}

function c() {
  this.x = 1;
  d();
}

function d() {
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let call_graph = '';
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    call_graph += callFrames[0].functionName;
    await Protocol.Debugger.stepInto();
  });

  // Breakpoint stops on "new c()" and steps into c.
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 2, columnNumber: 0, url: 'test.js'});
  call_graph = '';
  await Protocol.Runtime.evaluate({expression: 'a(); %ClearStepping();'});
  InspectorTest.log(`call_graph for a(): ${call_graph}`);

  // Breakpoint stops on "x = 1" and steps to "new c()" and then into c.
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 6, columnNumber: 0, url: 'test.js'});
  call_graph = '';
  await Protocol.Runtime.evaluate({expression: 'b(); %ClearStepping();'});
  InspectorTest.log(`call_graph for b(): ${call_graph}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
