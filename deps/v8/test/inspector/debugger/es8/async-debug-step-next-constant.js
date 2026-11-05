// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping over await with constant in async function.');

const source = `
async function f() {
  var a = 1;
  debugger;          // B0 StepOver
  a +=               // B1 StepOver
       await
             5;
  return a;          // B2 Continue
}

function run() {
  f();
  %PerformMicrotaskCheckpoint();
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let step_count = 0;
  Protocol.Debugger.onPaused(async message => {
    let callFrame = message.params.callFrames[0];
    let line = source.split('\n')[callFrame.location.lineNumber];
    InspectorTest.log(line);
    let [match, expected_count, step] = /\/\/ B(\d) (\w+)$/.exec(line);
    if (step_count !== parseInt(expected_count)) {
      InspectorTest.log(`Expected break ${expected_count}, got ${step_count}`);
    }
    step_count++;
    if (step === 'StepOver') {
      await Protocol.Debugger.stepOver();
    } else if (step === 'StepInto') {
      await Protocol.Debugger.stepInto();
    } else if (step === 'StepOut') {
      await Protocol.Debugger.stepOut();
    } else if (step === 'Continue') {
      await Protocol.Debugger.resume();
    }
  });

  let res = await Protocol.Runtime.evaluate({expression: 'run()'});
  if (res.result.exceptionDetails) {
    InspectorTest.log(res.result.exceptionDetails.text);
  }

  if (step_count !== 3) {
    InspectorTest.log('Expected step_count 3, got ' + step_count);
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
