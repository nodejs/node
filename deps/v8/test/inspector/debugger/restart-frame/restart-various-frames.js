// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Checks that restarting every frame on the stack works.');

session.setupScriptMap();

contextGroup.addScript(`
function A() {
  console.log('A');
  debugger;
  return 'Magic value';
}
function B(param1, param2) {
  console.log('B');
  return A();
}
function C() {
  console.log('C');
  // Function call with argument adapter is intentional.
  return B();
}
function D() {
  console.log('D');
  // Function call with argument adapter is intentional.
  return C(42, 'foo');
}
function E() {
  console.log('E');
  return D();
}
function F() {
  console.log('F');
  return E();
}
`, 0, 0, 'test.js');

async function testCase(frameIndex) {
  const { callFrames, evaluatePromise } = await InspectorTest.evaluateAndWaitForPause('F()');
  await InspectorTest.restartFrameAndWaitForPause(callFrames, frameIndex);

  Protocol.Debugger.resume();  // Resuming hits the 'debugger' stmt again.
  await Protocol.Debugger.oncePaused();
  await Protocol.Debugger.resume();

  const { result: { result: { value } } } = await evaluatePromise;
  InspectorTest.log(`Evaluating to: ${value}`);
}

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  const consoleMessages = [];
  Protocol.Runtime.onConsoleAPICalled(({ params }) => {
    consoleMessages.push(params.args[0].value);
  });

  for (let index = 0; index < 6; ++index) {
    InspectorTest.log(`\n---- Restarting at frame index ${index} ----`);
    await testCase(index);
    InspectorTest.log(`Called functions: ${consoleMessages.join()}`);
    consoleMessages.splice(0);
  }

  InspectorTest.completeTest();
})();
