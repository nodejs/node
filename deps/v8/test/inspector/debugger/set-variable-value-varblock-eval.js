// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that Debugger.setVariableValue can set eval-introduced vars in ' +
    'the varblock scope of a function with non-simple parameters');

contextGroup.addScript(`
function f(a = 1) {
  eval("var leaked = 1;");
  debugger;
  return leaked;
}
`);

(async () => {
  await Protocol.Debugger.enable();

  const evalPromise =
      Protocol.Runtime.evaluate({expression: 'f()', returnByValue: true});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const {callFrameId, scopeChain} = callFrames[0];

  InspectorTest.log('Scope chain: ' + scopeChain.map(s => s.type).join(', '));
  const blockScopeNumber = scopeChain.findIndex(s => s.type === 'block');

  const {result: {result: props}} = await Protocol.Runtime.getProperties(
      {objectId: scopeChain[blockScopeNumber].object.objectId});
  InspectorTest.log(
      'Block scope variables: ' +
      props.map(p => `${p.name}=${p.value.value}`).join(', '));

  InspectorTest.log('Setting "leaked" on the block scope to 42:');
  InspectorTest.logMessage(await Protocol.Debugger.setVariableValue({
    scopeNumber: blockScopeNumber,
    variableName: 'leaked',
    newValue: {value: 42},
    callFrameId,
  }));

  await Protocol.Debugger.resume();
  const {result: {result}} = await evalPromise;
  InspectorTest.log('f() returned: ' + result.value);
  InspectorTest.completeTest();
})();
