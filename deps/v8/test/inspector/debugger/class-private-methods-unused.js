// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/private-class-member-inspector-test.js');

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test accessing unused private methods at runtime"
);

const script = `
  function run() {
    class A {
      #instanceMethod() { return 2; }
      static #staticMethod() { return 1; }
      static testStatic() { debugger; }
      testInstance() { debugger; }
    };
    A.testStatic();
    const a = new A;
    a.testInstance();
  }`;

contextGroup.addScript(script);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();

    // Do not await here, instead oncePaused should be awaited.
    Protocol.Runtime.evaluate({ expression: 'run()' });

    InspectorTest.log('private members of A in testStatic()');
    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside A.testStatic()
    let frame = callFrames[0];
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

    // Variables not referenced in the source code are currently
    // considered "optimized away".
    InspectorTest.log('Access A.#staticMethod() in testStatic()');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'A.#staticMethod();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logMessage(result);

    InspectorTest.log('Access this.#staticMethod() in testStatic()');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#staticMethod();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logMessage(result);

    Protocol.Debugger.resume();
    ({ params: { callFrames } } = await Protocol.Debugger.oncePaused());  // a.testInstatnce();
    frame = callFrames[0];

    InspectorTest.log('private members of a in testInstance()');
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

    InspectorTest.log('Evaluating this.#instanceMethod() in testInstance()');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#instanceMethod();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logMessage(result);

    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
