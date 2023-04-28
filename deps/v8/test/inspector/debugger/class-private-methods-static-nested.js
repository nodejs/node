// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/private-class-member-inspector-test.js');

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

    InspectorTest.log('private members on class A');
    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside A.#method()

    let frame = callFrames[0];
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });
    Protocol.Debugger.resume();

    ({ params: { callFrames } } = await Protocol.Debugger.oncePaused());  // B.test();
    frame = callFrames[0];

    InspectorTest.log('private members on class B');
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
