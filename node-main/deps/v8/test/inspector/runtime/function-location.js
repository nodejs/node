// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Check [[FunctionLocation]] internal property');

contextGroup.addScript(
`function a() { b(); };
function    b() {
  c(true);
};
  function c(x) {
    if (x) {
      return 1;
    } else {
      return 1;
    }
  };
function d(x) {
  x = 1 ;
  x = 2 ;
  x = 3 ;
  x = 4 ;
  x = 5 ;
  x = 6 ;
  x = 7 ;
  x = 8 ;
  x = 9 ;
  x = 10;
  x = 11;
  x = 12;
  x = 13;
  x = 14;
  x = 15;
}`);

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  await session.logSourceLocation(await functionLocation('a'), true);
  await session.logSourceLocation(await functionLocation('b'), true);
  await session.logSourceLocation(await functionLocation('c'), true);
  await session.logSourceLocation(await functionLocation('d'), true);
  InspectorTest.completeTest();
})();

async function functionLocation(name) {
  const {result:{result}} = await Protocol.Runtime.evaluate({expression: name});
  const {result:{internalProperties}} = await Protocol.Runtime.getProperties({
    objectId: result.objectId
  });
  const {value:{value}} = internalProperties.find(
      prop => prop.name === '[[FunctionLocation]]');
  return value;
}
