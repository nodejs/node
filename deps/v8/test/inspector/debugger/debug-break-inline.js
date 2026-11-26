// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This test tests that deoptimization due to debug breaks works for
// inlined functions where the full-code is generated before the
// debugger is attached.
//
// See http://code.google.com/p/chromium/issues/detail?id=105375

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that deoptimization due to debug breaks works for inlined ' +
    'functions compiled before debugger is attached.');

contextGroup.addScript(
    `
var count = 0;

function f() {
  g();
}

function g() {
  count++;
  h();
  var b = 1;  // Break point is set here.
}

function h() {
  debugger;
}

%PrepareFunctionForOptimization(f);
f();f();f();
%OptimizeFunctionOnNextCall(f);
f();
`,
    0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  let break_count = 0;

  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    break_count++;
    InspectorTest.log(`Paused #${break_count}:`);
    session.logCallFrames(callFrames);
    if (break_count === 1) {
      await Protocol.Debugger.setBreakpointByUrl(
          {lineNumber: 10, columnNumber: 0, url: 'test.js'});
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'count'});
  InspectorTest.log(`count: ${result.value}`);
  InspectorTest.log(`break_count: ${break_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
