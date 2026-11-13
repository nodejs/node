// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests async debug step abort at break.');

const source = `
var late_resolve;

function g() {
  return new Promise( // B2 StepOut
    function(res, rej) {
      late_resolve = res;
    }
  );
}

async function f() {
  var a = 1;
  debugger;          // B0 StepOver
  a +=
       await         // B1 StepInto
             g();    // B2 StepInto
  return a;
}

function run() {
  f();

  // Starting a new step action at an intermediate break point
  // means that we will abort the current async step.
  debugger;            // B3 StepOver

  late_resolve(3);     // B4 Continue

  %PerformMicrotaskCheckpoint();
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let step_count = 0;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let line = source.split('\n')[callFrames[0].location.lineNumber];
    InspectorTest.log(line);
    let match = /\/\/ B(\d) (\w+)$/.exec(line);
    if (!match) {
      InspectorTest.log('FAIL: No match on line: ' + line);
      await Protocol.Debugger.resume();
      return;
    }
    let [, expected_count, step] = match;
    if (step_count++ !== parseInt(expected_count)) {
      InspectorTest.log(
          `FAIL: step_count mismatch: ${step_count - 1} vs ${expected_count}`);
    }
    if (step === 'StepOver') {
      await Protocol.Debugger.stepOver();
    } else if (step === 'StepInto') {
      await Protocol.Debugger.stepInto();
    } else if (step === 'StepOut') {
      await Protocol.Debugger.stepOut();
    } else if (step === 'Continue') {
      await Protocol.Debugger.resume();
    } else {
      InspectorTest.log('FAIL: Unknown step action: ' + step);
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'run();'});
  InspectorTest.log(`step_count: ${step_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
