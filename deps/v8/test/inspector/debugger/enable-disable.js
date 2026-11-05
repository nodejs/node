// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests enable-disable.');

contextGroup.addScript(`
function f() { return 42; }
`);

(async function test() {
  let compileCount = 0;
  Protocol.Debugger.onScriptParsed(() => {
    compileCount++;
  });

  await Protocol.Debugger.enable();
  InspectorTest.log('compileCount != 0: ' + (compileCount !== 0));
  const compileCountAfterEnable = compileCount;

  await Protocol.Debugger.enable();  // Idempotent.
  InspectorTest.log(
      'after second enable: ' + (compileCount === compileCountAfterEnable));

  await Protocol.Debugger.disable();
  InspectorTest.log(
      'after disable: ' + (compileCount === compileCountAfterEnable));

  await Protocol.Debugger.disable();  // Idempotent.
  InspectorTest.log(
      'after second disable: ' + (compileCount === compileCountAfterEnable));

  await Protocol.Debugger.enable();  // Re-enabling re-emits scriptParsed.
  InspectorTest.log(
      'after re-enable: ' + (compileCount === 2 * compileCountAfterEnable));

  InspectorTest.completeTest();
})();
