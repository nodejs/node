// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that initialized undefined switch locals are accessible and skipped ones are in TDZ.');

Protocol.Debugger.enable();

contextGroup.addScript(`
function testSwitch(v) {
  switch (v) {
    case 1:
      let x = undefined;
      debugger;
      return x;
    case 2:
      debugger;
      break;
  }
}
`);

(async function() {
  Protocol.Runtime.evaluate({expression: 'testSwitch(1)'});
  let message = await Protocol.Debugger.oncePaused();
  let callFrameId = message.params.callFrames[0].callFrameId;
  InspectorTest.log('Paused in case 1 (initialized undefined):');
  let {result: evalResult} = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId, expression: 'x'});
  InspectorTest.log(
      `  evaluate x: ${evalResult.result.type} ${evalResult.result.value}`);

  let scope =
      message.params.callFrames[0].scopeChain.find(s => s.type === 'block');
  if (scope) {
    let {result: {result: locals}} =
        await Protocol.Runtime.getProperties({objectId: scope.object.objectId});
    for (let local of locals) {
      if (local.name === 'x') {
        InspectorTest.log(`  scope property x: ${
                          'value' in local ? local.value.value :
                                             '<unavailable>'}`);
      }
    }
  }
  await Protocol.Debugger.resume();

  Protocol.Runtime.evaluate({expression: 'testSwitch(2)'});
  message = await Protocol.Debugger.oncePaused();
  callFrameId = message.params.callFrames[0].callFrameId;
  InspectorTest.log('Paused in case 2 (skipped declaration in TDZ):');
  let {result: evalSkipped} = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId, expression: 'x'});
  if (evalSkipped.exceptionDetails) {
    InspectorTest.log(`  evaluate x threw: ${
        evalSkipped.exceptionDetails.exception.description}`);
  } else {
    InspectorTest.log(`  evaluate x: ${evalSkipped.result.value}`);
  }

  scope = message.params.callFrames[0].scopeChain.find(s => s.type === 'block');
  if (scope) {
    let {result: {result: locals}} =
        await Protocol.Runtime.getProperties({objectId: scope.object.objectId});
    for (let local of locals) {
      if (local.name === 'x') {
        InspectorTest.log(`  scope property x: ${
                          'value' in local ? local.value.value :
                                             '<unavailable>'}`);
      }
    }
  }
  await Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();
