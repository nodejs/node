// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-387599.');

contextGroup.addScript(
    `
function f() {
  for (var i = 0; i < 100; i++) {
    %OptimizeOsr();
    %PrepareFunctionForOptimization(f);
  }
}
%PrepareFunctionForOptimization(f);
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(() => Protocol.Debugger.resume());
  await Protocol.Debugger.setBreakpointByUrl({lineNumber: 2, url: 'test.js'});
  await Protocol.Runtime.evaluate(
      {expression: 'f(); %PrepareFunctionForOptimization(f); f();'});
  InspectorTest.log('Done.');
  InspectorTest.completeTest();
})();
