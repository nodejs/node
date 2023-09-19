// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks [[Scopes]] for functions');

contextGroup.addScript(`
var f;
try {
  throw 1;
} catch (a) {
  with({b:2}) {
    function closure() {
      var c = 3;
      function foo() {
        var d = 4;
        return a + b + c + d;
      }
      return foo;
    }
    f = closure;
  }
}
var e = 5;
//# sourceURL=test.js`);

(async function test() {
  let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
    expression: 'f()'
  });
  let {result:{internalProperties}} = await Protocol.Runtime.getProperties({
    objectId
  });
  let scopes = internalProperties.find(prop => prop.name === '[[Scopes]]');
  let {result:{result}} = await Protocol.Runtime.getProperties({
    objectId: scopes.value.objectId
  });
  await Promise.all(result.map(async scope => {
    scope.variables = (await Protocol.Runtime.getProperties({
      objectId: scope.value.objectId
    })).result.result;
  }));
  let catchScope = result.find(scope => scope.value.description === 'Catch');
  InspectorTest.log('Catch:');
  InspectorTest.logMessage(catchScope.variables.find(variable => variable.name === 'a'));
  InspectorTest.log('With block:');
  let withScope = result.find(scope => scope.value.description === 'With Block');
  InspectorTest.logMessage(withScope.variables.find(variable => variable.name === 'b'));
  InspectorTest.log('Closure (closure):');
  let closureScope = result.find(scope => scope.value.description === 'Closure (closure)');
  InspectorTest.logMessage(closureScope.variables.find(variable => variable.name === 'c'));
  InspectorTest.log('Global:');
  let globalScope = result.find(scope => scope.value.description === 'Global');
  InspectorTest.logMessage(globalScope.variables.find(variable => variable.name === 'e'));
  InspectorTest.completeTest();
})();
