// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-baseline-batch-compilation

let {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/736758.');

contextGroup.addScript(`
function f() { [1,2,3].forEach(g) }
function g() { debugger }
`);

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  let pauses = 0;
  Protocol.Debugger.onPaused(() => {
    pauses++;
    Protocol.Debugger.resume();
  });
  await Protocol.Runtime.evaluate({
    expression: `
      %PrepareFunctionForOptimization(f);
      f();
      f();
      %OptimizeFunctionOnNextCall(f);
      f();
    `
  });
  InspectorTest.log('pauses: ' + pauses);
  InspectorTest.completeTest();
})();
