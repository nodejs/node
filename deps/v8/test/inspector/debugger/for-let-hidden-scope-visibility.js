// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests scope visibility and blocklist behavior for hidden for-let scopes.');

const source = `
var x = 'outer-x';

function testShadowedStackLocal() {
  for (let x = 1, f = () => { debugger; }; ; ) {
    f();
    break;
  }
}

function testObjectLiteralSuper() {
  for (let x = 1, o = { m() { super.foo; debugger; return x; } }; ; ) {
    o.m();
    break;
  }
}
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Debugger.enable();

  InspectorTest.log('--- testObjectLiteralSuper (b/578889024) ---');
  let pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: 'testObjectLiteralSuper()'});
  let {params: {callFrames}} = await pausedPromise;

  let scopeTypes = callFrames[0].scopeChain.map(s => s.type);
  InspectorTest.log('Scope types: ' + JSON.stringify(scopeTypes));

  let initializerScope = callFrames[0].scopeChain[2];
  InspectorTest.log('Initializer scope type: ' + initializerScope.type);
  let {result: {result: props}} = await Protocol.Runtime.getProperties({
    objectId: initializerScope.object.objectId,
  });
  let xProp = props.find(p => p.name === 'x');
  InspectorTest.log(
      `Initializer scope x = ${xProp ? xProp.value.value : '<not found>'}`);

  let evalXInSuper = await Protocol.Debugger.evaluateOnCallFrame({
    expression: 'x',
    callFrameId: callFrames[0].callFrameId,
  });
  InspectorTest.log('eval x: ' + evalXInSuper.result.result.value);

  await Protocol.Debugger.resume();

  InspectorTest.log('--- testShadowedStackLocal (b/578890113) ---');
  pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: 'testShadowedStackLocal()'});
  ({params: {callFrames}} = await pausedPromise);

  let evalTypeOfX = await Protocol.Debugger.evaluateOnCallFrame({
    expression: 'typeof x',
    callFrameId: callFrames[0].callFrameId,
  });
  InspectorTest.log('eval typeof x: ' + evalTypeOfX.result.result.value);

  let evalX = await Protocol.Debugger.evaluateOnCallFrame({
    expression: 'x',
    callFrameId: callFrames[0].callFrameId,
  });
  if (evalX.result.exceptionDetails) {
    InspectorTest.log(
        'eval x threw: ' +
        evalX.result.exceptionDetails.exception.description.split('\n')[0]);
  } else {
    InspectorTest.log('eval x: ' + evalX.result.result.value);
  }

  await Protocol.Debugger.resume();
  InspectorTest.completeTest();
})();
