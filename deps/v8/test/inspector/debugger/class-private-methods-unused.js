// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

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

    InspectorTest.log('Get privateProperties of A in testStatic()');
    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside A.testStatic()
    let frame = callFrames[0];
    let { result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    });
    InspectorTest.logObject(result.privateProperties);

    // Variables not referenced in the source code are currently
    // considered "optimized away".
    InspectorTest.log('Access A.#staticMethod() in testStatic()');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'A.#staticMethod();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    InspectorTest.log('Access this.#staticMethod() in testStatic()');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#staticMethod();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    Protocol.Debugger.resume();
    ({ params: { callFrames } } = await Protocol.Debugger.oncePaused());  // a.testInstatnce();
    frame = callFrames[0];

    InspectorTest.log('get privateProperties of a in testInstance()');
    ({ result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    }));
    InspectorTest.logObject(result.privateProperties);

    InspectorTest.log('Evaluating this.#instanceMethod() in testInstance()');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#instanceMethod();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
