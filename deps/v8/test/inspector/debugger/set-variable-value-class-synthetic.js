// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that Debugger.setVariableValue cannot overwrite synthetic class context slots');

const source = `
class B {}
class C extends B {
  #m() { return 42; }
  foo() { return C; }
  constructor() { debugger; super(); }
  run() { return this.#m(); }
}
globalThis.inst = new C();
globalThis.inst.run();
`;

(async () => {
  await Protocol.Debugger.enable();

  const evalPromise = Protocol.Runtime.evaluate({expression: source});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const {callFrameId, scopeChain} = callFrames[0];

  let scopeNumber = -1;
  for (let i = 0; i < scopeChain.length; ++i) {
    if (scopeChain[i].type !== 'block') continue;
    const {result: {result}} = await Protocol.Runtime.getProperties(
        {objectId: scopeChain[i].object.objectId});
    if (result.some(p => p.name === 'C')) {
      scopeNumber = i;
      break;
    }
  }
  InspectorTest.log('Class block scope found: ' + (scopeNumber >= 0));

  InspectorTest.log('Setting .brand to "foo"');
  InspectorTest.logMessage(await Protocol.Debugger.setVariableValue({
    scopeNumber,
    variableName: '.brand',
    newValue: {value: 'foo'},
    callFrameId,
  }));

  await Protocol.Debugger.resume();
  await evalPromise;

  InspectorTest.log('Own property names of inst:');
  InspectorTest.logMessage(await Protocol.Runtime.evaluate(
      {expression: 'JSON.stringify(Object.getOwnPropertyNames(globalThis.inst))'}));

  InspectorTest.log('inst.run():');
  InspectorTest.logMessage(await Protocol.Runtime.evaluate(
      {expression: 'globalThis.inst.run()'}));

  InspectorTest.completeTest();
})();
