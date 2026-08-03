// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests how breakpoints and side effects live together.');

(async function main() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: `
var a = 1;
function foo() {
  a = 2;
}
//# sourceURL=test.js`});
  Protocol.Debugger.setBreakpointByUrl({url: 'test.js', lineNumber: 3});

  InspectorTest.log('Test breakpoint, should pause inside foo:');
  {
    const evaluatePromise = Protocol.Runtime.evaluate({expression: 'foo()'});
    const {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    session.logCallFrames(callFrames);
    Protocol.Debugger.resume();
    await evaluatePromise;
  }

  InspectorTest.log('\nRun foo with no side effects:');
  {
    const {result:{result}} = await Protocol.Runtime.evaluate({
      expression: 'foo()',
      throwOnSideEffect: true
    });
    InspectorTest.logMessage(result);
  }

  InspectorTest.log('\nTest breakpoint after run with side effect check:');
  {
    const evaluatePromise = Protocol.Runtime.evaluate({expression: 'foo()'});
    const {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    session.logCallFrames(callFrames);
    Protocol.Debugger.resume();
    await evaluatePromise;
  }

  await Protocol.Debugger.disable();

  InspectorTest.log('\nRun foo with no side effects after debugger disabled:');
  {
    const {result:{result}} = await Protocol.Runtime.evaluate({
      expression: 'foo()',
      throwOnSideEffect: true
    });
    InspectorTest.logMessage(result);
  }

  InspectorTest.completeTest();
})();
