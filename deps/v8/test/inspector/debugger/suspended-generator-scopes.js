// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-experimental-remove-internal-scopes-property

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that suspended generators produce scopes');

contextGroup.addScript(`
function *gen(a) {
  var b = 42;
  yield a;
  return b;
}

function testSuspendedGenerator() {
  var g = gen(420);
  g.next();
  debugger;
  return g;
}`);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: 'testSuspendedGenerator()'});
    let {params:{callFrames:[callFrame]}} = await Protocol.Debugger.oncePaused();
    // Current local scope.
    let localScope = callFrame.scopeChain.find(scope => scope.type === 'local');
    let variables = (await Protocol.Runtime.getProperties({
      objectId: localScope.object.objectId
    })).result.result;
    let genObjectId =
      variables.find(variable => variable.name === 'g').value.objectId;
    let {result:{internalProperties}} = await Protocol.Runtime.getProperties({
      objectId: genObjectId
    });
    // Generator [[Scopes]].
    let scopes = internalProperties.find(prop => prop.name === '[[Scopes]]');
    let {result:{result}} = await Protocol.Runtime.getProperties({
      objectId: scopes.value.objectId
    });
    // Locals from generator.
    let scope = result.find(scope => scope.value.description === 'Local (gen)');
    ({result:{result}} = await Protocol.Runtime.getProperties({
      objectId: scope.value.objectId
    }));
    InspectorTest.logMessage(result);
    await Protocol.Debugger.disable();
  },

  async function testScopesNonPaused() {
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'gen(430)'
    });
    let {result:{internalProperties}} = await Protocol.Runtime.getProperties({
      objectId
    });
    // Generator [[Scopes]].
    let scopes = internalProperties.find(prop => prop.name === '[[Scopes]]');
    let {result:{result}} = await Protocol.Runtime.getProperties({
      objectId: scopes.value.objectId
    });
    // Locals from generator.
    let scope = result.find(scope => scope.value.description === 'Local (gen)');
    ({result:{result}} = await Protocol.Runtime.getProperties({
      objectId: scope.value.objectId
    }));
    InspectorTest.logMessage(result);
  }
]);
