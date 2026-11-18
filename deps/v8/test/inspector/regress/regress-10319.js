// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-10319.');

contextGroup.addScript(`
function makeCounter() {
  // The debug-evaluate should be able to set this to 3. This was fixed with
  // https://crbug.com/1246907 which delays debug breaks triggered by stack
  // checks until the function context is properly set up.
  if (result !== 3) throw new Error('Expected 3, got ' + result);

  var result = 0;

  // Regardless of how \`result\` is allocated, it should now be initialized.
  if (result !== 0) throw new Error('Expected 0, got ' + result);

  // Close over \`result\` to cause it to be heap-allocated.
  return () => ++result;
}

function run() {
  // Break on entry to a function which includes heap-allocated variables.
  %ScheduleBreak();
  makeCounter();
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evalPromise = Protocol.Runtime.evaluate({expression: 'run()'});
  const {params: {callFrames}} = await pausedPromise;
  const frame = callFrames[0];

  const {result: {result: props}} = await Protocol.Runtime.getProperties(
      {objectId: frame.scopeChain[0].object.objectId, ownProperties: true});
  InspectorTest.logMessage(
      props.map(p => ({name: p.name, value: p.value.type})));
  InspectorTest.logMessage(frame.scopeChain.map(s => s.type));

  // Try changing the value, which hasn't yet been initialized.
  const eval1 = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId: frame.callFrameId, expression: 'result = 3'});
  InspectorTest.log('result = 3 -> ' + eval1.result.result.value);
  const eval2 = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId: frame.callFrameId, expression: 'result'});
  InspectorTest.log('result -> ' + eval2.result.result.value);

  await Protocol.Debugger.resume();
  const {result} = await evalPromise;
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
