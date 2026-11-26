// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests optimizing function with try-catch inside forEach with debugger enabled.');

contextGroup.addScript(
    `
var a = [0,1,2,3,4,5,6,7,8,9];

var f = function() {
  a.forEach(function(v) {
    try {
      throw new Error();
    } catch (e) {
    }
  });
};
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({
    expression: `
      %PrepareFunctionForOptimization(f);
      f();
      f();
      %OptimizeFunctionOnNextCall(f);
      f();
      f();
    `
  });
  InspectorTest.completeTest();
})();
