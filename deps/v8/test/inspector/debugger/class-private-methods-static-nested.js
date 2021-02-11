// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test static private class methods"
);

const script = `
function run() {
  class A {
    static #method() {
      debugger;
    }
    static test() {
      class B {
        static test() { debugger; }
      }
      A.#method();  // reference #method so it shows up
      B.test();
    }
  }
  A.test();
}`;

contextGroup.addScript(script);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();

    // Do not await here, instead oncePaused should be awaited.
    Protocol.Runtime.evaluate({ expression: 'run()' });

    InspectorTest.log('privateProperties on class A');
    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside A.#method()

    let frame = callFrames[0];
    let { result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    });
    InspectorTest.logMessage(result.privateProperties);
    Protocol.Debugger.resume();

    ({ params: { callFrames } } = await Protocol.Debugger.oncePaused());  // B.test();
    frame = callFrames[0];

    InspectorTest.log('privateProperties on class B');
    ({ result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    }));
    InspectorTest.logObject(result.privateProperties);

    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
