// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --experimental-reuse-locals-blocklists

const {contextGroup, Protocol} = InspectorTest.start(`Regression test for crbug/1209117`);

contextGroup.addInlineScript(`
var y = 42;

var a = 1;
const testGlobalScope = a => {
  function foo(x) { debugger; }
  return foo(a);
};

let b = 1;
const testScriptScope = b => {
  function foo(x) { debugger; }
  return foo(b);
};

const testFunctionScope = (() => {
  const c = 1;
  const testFunctionScope = c => {
    function foo(x) { debugger; }
    return foo(c);
  };
  return [testFunctionScope, c];
})()[0];
`, `foo.js`);

async function checkVariable(expression, callFrameId) {
  const {result: {exceptionDetails}} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression
  });
  InspectorTest.log(`${expression}: ${exceptionDetails ? 'shadowed' : 'not shadowed'}`);
}

InspectorTest.runAsyncTestSuite([
  async function testGlobalScope() {
    await Promise.all([Protocol.Debugger.enable(), Protocol.Runtime.enable()]);
    const callPromise = Protocol.Runtime.evaluate({expression: 'testGlobalScope(2)'});
    const {params:{callFrames:[{callFrameId}]}} = await Protocol.Debugger.oncePaused();
    checkVariable('a', callFrameId);
    checkVariable('x', callFrameId);
    checkVariable('y', callFrameId);
    await Promise.all([Protocol.Debugger.resume(), callPromise]);
    await Promise.all([Protocol.Debugger.disable(), Protocol.Runtime.disable()]);
  },

  async function testScriptScope() {
    await Promise.all([Protocol.Debugger.enable(), Protocol.Runtime.enable()]);
    const callPromise = Protocol.Runtime.evaluate({expression: 'testScriptScope(2)'});
    const {params:{callFrames:[{callFrameId}]}} = await Protocol.Debugger.oncePaused();
    checkVariable('b', callFrameId);
    checkVariable('x', callFrameId);
    checkVariable('y', callFrameId);
    await Promise.all([Protocol.Debugger.resume(), callPromise]);
    await Promise.all([Protocol.Debugger.disable(), Protocol.Runtime.disable()]);
  },

  async function testFunctionScope() {
    await Promise.all([Protocol.Debugger.enable(), Protocol.Runtime.enable()]);
    const callPromise = Protocol.Runtime.evaluate({expression: 'testFunctionScope(2)'});
    const {params:{callFrames:[{callFrameId}]}} = await Protocol.Debugger.oncePaused();
    checkVariable('c', callFrameId);
    checkVariable('x', callFrameId);
    checkVariable('y', callFrameId);
    await Promise.all([Protocol.Debugger.resume(), callPromise]);
    await Promise.all([Protocol.Debugger.disable(), Protocol.Runtime.disable()]);
  }
]);
