// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test static private class methods"
);

const script = `
function run() {
  class A {
    static test() {
      debugger;
    }
    static #field = 2;
    #instanceMethod() { }  // should not show up
    get #instanceAccessor() { return this.#field; }  // should not show up
    set #instanceAccessor(val) { this.#field = val; }  // should not show up

    static set #writeOnly(val) { this.#field = val; }
    static get #readOnly() { return this.#field; }
    static get #accessor() { return this.#field; }
    static set #accessor(val) { this.#field = val; }
    static #inc() { return ++A.#accessor; }
  };
  A.test();

  class B extends A {
    static get #accessor() { return 'subclassAccessor'; }
    static #subclassMethod() { return B.#accessor; }
    static test() {
      debugger;
    }
  }

  B.test();
}`;

contextGroup.addScript(script);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();

    // Do not await here, instead oncePaused should be awaited.
    Protocol.Runtime.evaluate({ expression: 'run()' });

    InspectorTest.log('privateProperties on the base class');
    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside A.test()
    let frame = callFrames[0];
    let { result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    });
    InspectorTest.logMessage(result.privateProperties);

    // TODO(joyee): make it possible to desugar the brand check, which requires
    // the class variable to be saved, even when the static private
    // methods/accessors are not referenced in the class body.
    InspectorTest.log('Evaluating A.#inc();');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'A.#inc();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    InspectorTest.log('Evaluating this.#inc();');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#inc();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    InspectorTest.log('Evaluating ++this.#accessor;');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: '++this.#accessor;',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    InspectorTest.log('Evaluating this.#readOnly;');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#readOnly;',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    InspectorTest.log('Evaluating this.#writeOnly = 0; this.#field;');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#writeOnly = 0; this.#field;',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logObject(result);

    Protocol.Debugger.resume();
    ({ params: { callFrames } } = await Protocol.Debugger.oncePaused());  // B.test();
    frame = callFrames[0];

    InspectorTest.log('privateProperties on the subclass');
    ({ result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    }));
    InspectorTest.logMessage(result.privateProperties);

    InspectorTest.log('Evaluating this.#inc(); from the base class');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#inc();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logMessage(result);

    InspectorTest.log('Evaluating this.#subclassMethod();');
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#subclassMethod();',
      callFrameId: callFrames[0].callFrameId
    }));
    InspectorTest.logMessage(result);

    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
