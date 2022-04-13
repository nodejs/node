// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
    `Tests that Runtime.evaluate's timeout argument`);

(async function test(){
  {
    InspectorTest.log('Run trivial expression:');
    const result = await Protocol.Runtime.evaluate({
      expression: 'function foo() {} foo()',
      timeout: 0
    });
    InspectorTest.log('Evaluate finished!');
  }
  {
    InspectorTest.log('Run expression without interrupts:');
    const result = await Protocol.Runtime.evaluate({
      expression: '',
      timeout: 0
    });
    InspectorTest.log('Evaluate finished!');
  }
  {
    InspectorTest.log('Run infinite loop:');
    const result = await Protocol.Runtime.evaluate({
      expression: 'for(;;){}',
      timeout: 0
    });
    InspectorTest.log('Evaluate finished!');
  }
  InspectorTest.completeTest();
})();
