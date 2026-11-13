// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that Maglev throws TypeError when calling class constructors ' +
    'with break info.');

contextGroup.addScript(
    `class C { #x = 42; }
class Base {}
Base.prototype.c = C;
class Sub extends Base {
  m() { super.c(); }
}

function makeNewClosure(call_it) {
  const D = class {
    constructor() {
      return {};
    }
  };
  if (call_it) D();
  return D;
}
const sampleD = makeNewClosure(false);
new sampleD();`,
    0, 0, 'test.js');

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setBreakpointByUrl(
        {url: 'test.js', lineNumber: 0, columnNumber: 0, condition: 'false'});
    await Protocol.Debugger.setBreakpointByUrl(
        {url: 'test.js', lineNumber: 10, columnNumber: 6, condition: 'false'});
  },

  async function testCallConstantConstructor() {
    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: `(() => {
        %PrepareFunctionForOptimization(Sub.prototype.m);
        try { Sub.prototype.m.call(1); } catch (e) {}
        try { Sub.prototype.m.call(1); } catch (e) {}
        %OptimizeFunctionOnNextCall(Sub.prototype.m);
        try {
          Sub.prototype.m.call(1);
          return 'did not throw';
        } catch (e) {
          return e instanceof TypeError &&
              e.message.includes("cannot be invoked without 'new'");
        }
      })()`,
    });
    InspectorTest.log(`threw TypeError: ${result.value}`);
  },

  async function testCallNewClosureConstructor() {
    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: `(() => {
        %PrepareFunctionForOptimization(makeNewClosure);
        try { makeNewClosure(true); } catch (e) {}
        try { makeNewClosure(true); } catch (e) {}
        %OptimizeFunctionOnNextCall(makeNewClosure);
        try {
          makeNewClosure(true);
          return 'did not throw';
        } catch (e) {
          return e instanceof TypeError &&
              e.message.includes("cannot be invoked without 'new'");
        }
      })()`,
    });
    InspectorTest.log(`threw TypeError: ${result.value}`);
  },
]);
