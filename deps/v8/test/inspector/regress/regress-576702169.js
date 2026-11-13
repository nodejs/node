// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that closure in for (let) initializer sees the initializer block scope.');

const source = `
function f() {
  for (let x = 42, y = (() => { debugger; return x; })(); ; ) {
    return y;
  }
}
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: 'f()'});
  const {params: {callFrames}} = await pausedPromise;

  const scopeTypes = callFrames[0].scopeChain.map(s => s.type);
  InspectorTest.log('Scope types: ' + JSON.stringify(scopeTypes));

  const blockScope = callFrames[0].scopeChain[1];
  InspectorTest.log('Block scope type: ' + blockScope.type);

  const {result: {result: properties}} = await Protocol.Runtime.getProperties({
    objectId: blockScope.object.objectId,
  });
  const xProp = properties.find(p => p.name === 'x');
  InspectorTest.log(`x = ${xProp ? xProp.value.value : '<not found>'}`);

  await Protocol.Debugger.resume();
  InspectorTest.completeTest();
})();
