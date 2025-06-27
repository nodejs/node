// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/private-class-member-inspector-test.js');

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

    InspectorTest.log('private members on the base class');
    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside A.test()
    let frame = callFrames[0];

    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

    InspectorTest.log('Evaluating A.#inc();');
    let { result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'A.#inc();',
      callFrameId: callFrames[0].callFrameId
    });
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

    InspectorTest.log('private members on the subclass');
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

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
