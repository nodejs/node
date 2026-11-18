// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --no-turbo --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-633999.');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  let step = 0;
  Protocol.Debugger.onPaused(message => {
    if (message.params.reason === 'exception') {
      step++;
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({
    expression: `(function () {
  "use asm";
  function f() {
    try {
      throw 666;
    } catch (e) {
    }
  }
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();`
  });
  InspectorTest.log('exception pauses: ' + step);
  InspectorTest.completeTest();
})();
