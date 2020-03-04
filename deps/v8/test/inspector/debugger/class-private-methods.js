// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods
// TODO(v8:9839): return private methods and accessors

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test private class methods"
);

contextGroup.addScript(`
function run() {
  class A {
    #field = 2;
    #inc() { this.#field++; return this.#field; }
    get #getter() { return this.#field; }
    set #setter(val) { this.#field = val; }
    fn () {
      debugger;
    }
  };
  const a = new A();
  a.fn();
}`);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({ expression: "run()" });

    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside fn()
    const frame = callFrames[0];
    let { result  } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    });
    InspectorTest.logMessage(result.privateProperties);
    let { result: result2 } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#inc();',
      callFrameId: callFrames[0].callFrameId
    });
    InspectorTest.logObject(result2);
    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
