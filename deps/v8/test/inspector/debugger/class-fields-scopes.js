// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-public-fields --harmony-static-fields

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test class fields scopes"
);

contextGroup.addScript(`
function run() {
  function foo() {
    debugger;
    return "foo";
  }

  function bar() {
    return 3;
  }

  class X {
    x = 1;
    [foo()] = 2;
    p = bar();
  }

  debugger;
  new X;
}`);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({ expression: "run()" });

    let {
      params: { callFrames: callFrames0 }
    } = await Protocol.Debugger.oncePaused(); // inside foo()
    InspectorTest.logMessage(callFrames0);

    Protocol.Debugger.resume();

    await Protocol.Debugger.oncePaused(); // at debugger;
    Protocol.Debugger.stepOver(); // at debugger;

    await Protocol.Debugger.oncePaused(); // at new X;
    Protocol.Debugger.stepInto(); // step into initializer_function;

    // at x = 1;
    let {
      params: { callFrames: callFrames1 }
    } = await Protocol.Debugger.oncePaused();
    InspectorTest.logMessage(callFrames1);
    Protocol.Debugger.stepOver();

    // at [foo()] = 2;
    let {
      params: { callFrames: callFrames2 }
    } = await Protocol.Debugger.oncePaused();
    InspectorTest.logMessage(callFrames2);
    Protocol.Debugger.stepOver();

    // at p = bar();
    let {
      params: { callFrames: callFrames3 }
    } = await Protocol.Debugger.oncePaused();
    InspectorTest.logMessage(callFrames3);
    Protocol.Debugger.stepInto();

    // inside bar();
    let {
      params: { callFrames: callFrames4 }
    } = await Protocol.Debugger.oncePaused();
    InspectorTest.logMessage(callFrames4);

    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
