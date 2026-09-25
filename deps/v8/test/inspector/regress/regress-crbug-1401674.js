// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {Protocol} = InspectorTest.start('Regression test for crbug.com/1401674. Properly step through single statement loops.');

(async () => {
  await Protocol.Debugger.enable();

  Protocol.Runtime.evaluate({
    expression: `
    function f() {
      let i = 0;
      debugger;
      while (true) {i++}
    }

    f();
  `});

  await Protocol.Debugger.oncePaused();

  Protocol.Debugger.stepInto();
  await Protocol.Debugger.oncePaused();

  InspectorTest.log('Expecting debugger to pause after the step ...');
  Protocol.Debugger.stepInto();
  await Protocol.Debugger.oncePaused();

  InspectorTest.log('SUCCESS');
  InspectorTest.log('Stepping to the same statement but in the next iteration ...');

  Protocol.Debugger.stepInto();
  await Protocol.Debugger.oncePaused();

  InspectorTest.log('SUCCESS');
  InspectorTest.completeTest();
})();
