// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let { session, contextGroup, Protocol } = InspectorTest.start(
  "Test getting private class methods from an instance that calls nested super()"
);

contextGroup.addScript(`
function run() {
  class A {}
  class B extends A {
    #b() {}
    constructor() {
      (() => super())();
    }
    test() { debugger; }
  };
  (new B()).test();

  class C extends B {
    get #c() {}
    constructor() {
      const callSuper = () => super();
      callSuper();
    }
    test() { debugger; }
  };
  (new C()).test();

  class D extends C {
    set #d(val) {}
    constructor(str) {
      eval(str);
    }
    test() { debugger; }
  };
  (new D('super();')).test();
}`);

InspectorTest.runAsyncTestSuite([
  async function testScopesPaused() {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({ expression: "run()" });

    let {
      params: { callFrames }
    } = await Protocol.Debugger.oncePaused(); // inside B constructor
    let frame = callFrames[0];
    let { result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    });

    InspectorTest.log('properties after super() is called in IIFE');
    InspectorTest.logMessage(result.privateProperties);
    Protocol.Debugger.resume();

    ({ params: { callFrames } }
        = await Protocol.Debugger.oncePaused());  // inside C constructor
    frame = callFrames[0];
    ({ result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    }));
    InspectorTest.log('privateProperties after super() is called in arrow function');
    InspectorTest.logMessage(result.privateProperties);
    Protocol.Debugger.resume();
    ({ params: { callFrames } }
        = await Protocol.Debugger.oncePaused());  // inside D constructor
    frame = callFrames[0];
    ({ result } = await Protocol.Runtime.getProperties({
      objectId: frame.this.objectId
    }));
    InspectorTest.log('privateProperties after super() is called in eval()');
    InspectorTest.logMessage(result.privateProperties);
    Protocol.Debugger.resume();

    Protocol.Debugger.disable();
  }
]);
