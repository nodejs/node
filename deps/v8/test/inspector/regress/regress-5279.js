// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-5279.');

contextGroup.addScript(`
function f() {
  const myObj = {};

  for (let i = 0; i < 10; i++) {
    %OptimizeOsr();
    %ScheduleBreak();
    %PrepareFunctionForOptimization(f);
  }
}
%PrepareFunctionForOptimization(f);
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(() => {
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
