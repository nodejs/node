// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test for Runtime.globalLexicalScopeVariablesNames');

(async function test() {
  InspectorTest.log('Running \'let a = 1\'');
  Protocol.Runtime.evaluate({expression: 'let a = 1'});
  await dumpGlobalScopeVariables();

  InspectorTest.log('Running \'let b = 2\'');
  Protocol.Runtime.evaluate({expression: 'let b = 2'});
  await dumpGlobalScopeVariables();

  InspectorTest.log('Running \'let b = 3\'');
  Protocol.Runtime.evaluate({expression: 'let b = 3'});
  await dumpGlobalScopeVariables();

  InspectorTest.log('Running \'const c = 4\'');
  Protocol.Runtime.evaluate({expression: 'const c = 4'});
  await dumpGlobalScopeVariables();

  InspectorTest.log('Running \'var d = 5\'');
  InspectorTest.log('(should not be in list of scoped variables)');
  Protocol.Runtime.evaluate({expression: 'var d = 5'});
  await dumpGlobalScopeVariables();

  InspectorTest.log('Running \'class Foo{}\'');
  Protocol.Runtime.evaluate({expression: 'class Foo{}'});
  await dumpGlobalScopeVariables();

  InspectorTest.log('Adding script with scope variables');
  contextGroup.addScript(`
  let e = 1;
  const f = 2;
  const g = 3;
  class Boo {};
  `);
  await dumpGlobalScopeVariables();
  InspectorTest.completeTest();
})();

async function dumpGlobalScopeVariables() {
  let {result:{names}} =
      await Protocol.Runtime.globalLexicalScopeNames();
  InspectorTest.log('Values:');
  for (let name of names) {
    let {result:{result}} = await Protocol.Runtime.evaluate({expression: name});
    if (result.value) {
      InspectorTest.log(`${name} = ${result.value}`);
    } else {
      InspectorTest.log(`${name} =`);
      InspectorTest.logMessage(result);
    }
  }
  InspectorTest.log('');
}
