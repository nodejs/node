// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that Debugger.setVariableValue handles unallocated variables gracefully');

const source = `
function outer() {
  var unusedOuter;
  function inner() {
    var unusedLocal;
    debugger;
  }
  inner();
}
outer();
`;

(async () => {
  await Protocol.Debugger.enable();

  Protocol.Runtime.evaluate({expression: source});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const {callFrameId, scopeChain} = callFrames[0];

  let localScopeNumber = -1;
  let closureScopeNumber = -1;
  for (let i = 0; i < scopeChain.length; ++i) {
    if (scopeChain[i].type === 'local' && localScopeNumber === -1) {
      localScopeNumber = i;
    } else if (scopeChain[i].type === 'closure' && closureScopeNumber === -1) {
      closureScopeNumber = i;
    }
  }

  InspectorTest.log('Local scope found: ' + (localScopeNumber >= 0));
  InspectorTest.log('Setting unallocated local variable "unusedLocal":');
  InspectorTest.logMessage(await Protocol.Debugger.setVariableValue({
    scopeNumber: localScopeNumber,
    variableName: 'unusedLocal',
    newValue: {value: 42},
    callFrameId,
  }));

  InspectorTest.log('Closure scope found: ' + (closureScopeNumber >= 0));
  InspectorTest.log('Setting unallocated outer variable "unusedOuter":');
  InspectorTest.logMessage(await Protocol.Debugger.setVariableValue({
    scopeNumber: closureScopeNumber,
    variableName: 'unusedOuter',
    newValue: {value: 42},
    callFrameId,
  }));

  await Protocol.Debugger.resume();
  InspectorTest.completeTest();
})();
