// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test private class fields in scopes"
);

contextGroup.addScript(`
function run() {
  class A {
    #foo = "hello"
    constructor () {
      debugger;
    }
  };
  new A();
}`);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({ expression: "run()" });

    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside A()
    InspectorTest.logMessage(callFrames);
    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
