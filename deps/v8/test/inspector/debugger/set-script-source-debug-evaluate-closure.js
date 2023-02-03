// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Check that setScriptSource doesn\'t affect debug-evaluate block listing');

const script = `
// INSERT NEWLINES HERE
function f() {
  let a = 3; () => a; // context allocated.
  return function g() {
    let a = 42;       // stack-allocated. Shadowing context-allocated from f.
    return function h() {
      // Give h a context.
      let x = 5; () => x;
      return function i() {
        debugger;
      };
    };
  };
}
(((f())())())();
`;

const updatedScript = script.replace('// INSERT NEWLINES HERE', '\n\n\n');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  const promise = Protocol.Debugger.oncePaused();

  contextGroup.addScript(script);

  const { params: { callFrames: [{ callFrameId, functionLocation: { scriptId } }] } } = await promise;

  // Create a closure that returns `a` and stash it on the global.
  await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: `globalThis['foo'] = () => a;`
  });
  await Protocol.Debugger.resume();

  // Expect a ReferenceError.
  const { result: { result: result1 } } = await Protocol.Runtime.evaluate({
    expression: 'globalThis.foo();'
  });
  InspectorTest.logMessage(result1);

  // Move function 'h' but don't change it.
  const { result: { status } } = await Protocol.Debugger.setScriptSource({
    scriptId,
    scriptSource: updatedScript,
  });
  InspectorTest.log(`Debugger.setScriptSource: ${status}`);

  // Still expect a ReferenceError.
  const { result: { result: result2 } } = await Protocol.Runtime.evaluate({
    expression: 'globalThis.foo();'
  });
  InspectorTest.logMessage(result2);

  InspectorTest.completeTest();
})();
