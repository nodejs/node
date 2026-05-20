// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
    `Tests that Debugger.evaluateOnCallFrame's timeout argument`);

(async function test(){
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: "debugger;"});
  const {params:{callFrames:[{callFrameId}]}} = await Protocol.Debugger.oncePaused();
  {
    InspectorTest.log('Run trivial expression:');
    const result = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'function foo() {} foo()',
      timeout: 0
    });
    InspectorTest.log('Evaluate finished!');
  }
  {
    InspectorTest.log('Run expression without interrupts:');
    const result = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: '',
      timeout: 0
    });
    InspectorTest.log('Evaluate finished!');
  }
  {
    InspectorTest.log('Run infinite loop:');
    const result = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'for(;;){}',
      timeout: 0
    });
    InspectorTest.log('Evaluate finished!');
  }
  InspectorTest.completeTest();
})();
