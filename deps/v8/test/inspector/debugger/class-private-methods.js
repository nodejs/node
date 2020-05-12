// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test private class methods"
);

contextGroup.addScript(`
function run() {
  class A {
    #field = 2;

    static #staticMethod() {}  // should not show up
    static get #staticAccessor() { }  // should not show up
    static set #staticAccessor(val) { }  // should not show up

    #inc() { this.#field++; return this.#field; }

    set #writeOnly(val) { this.#field = val; }

    get #readOnly() { return this.#field; }

    get #accessor() { return this.#field; }
    set #accessor(val) { this.#field = val; }

    fn() {
      debugger;
    }
  };
  const a = new A();
  a.fn();


  class B extends A {
    #subclassMethod() { return 'subclassMethod'; }
    #inc() { return 'subclass #inc'; }
    test() { debugger; }
  }

  const b = new B();
  b.fn();
  b.test();
}`);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({ expression: "run()" });

    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside a.fn()
    let frame = callFrames[0];
    let { result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    });

    InspectorTest.log('privateProperties on the base class instance');
    InspectorTest.logMessage(result.privateProperties);

    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#inc();',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating private methods');
    InspectorTest.logObject(result);

    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#inc();',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating private methods');
    InspectorTest.logObject(result);

    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: '++this.#accessor;',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating private accessors');
    InspectorTest.logObject(result);

    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#readOnly;',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating read-only accessor');
    InspectorTest.logObject(result);

    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#writeOnly = 0; this.#field;',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating write-only accessor');
    InspectorTest.logObject(result);

    Protocol.Debugger.resume();
    ({ params: { callFrames }  } = await Protocol.Debugger.oncePaused());  // b.fn();
    frame = callFrames[0];

    ({ result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    }));
    InspectorTest.log('privateProperties on the subclass instance');
    InspectorTest.logMessage(result.privateProperties);

    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#subclassMethod();',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating private methods in the base class from the subclass');
    InspectorTest.logMessage(result);

    Protocol.Debugger.resume();
    ({ params: { callFrames }  } = await Protocol.Debugger.oncePaused());  // b.test();
    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#subclassMethod();',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating private method in the subclass from the subclass');
    InspectorTest.logObject(result);

    ({ result } = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'this.#inc();',
      callFrameId: callFrames[0].callFrameId
    }));

    InspectorTest.log('Evaluating private method shadowing the base class method');
    InspectorTest.logObject(result);

    Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);
