// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/private-class-member-inspector-test.js');

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

    InspectorTest.log('private members after super() is called in IIFE');
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

    Protocol.Debugger.resume();

    ({ params: { callFrames } }
        = await Protocol.Debugger.oncePaused());  // inside C constructor
    frame = callFrames[0];
    InspectorTest.log('private members after super() is called in arrow function');
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

    Protocol.Debugger.resume();
    ({ params: { callFrames } }
        = await Protocol.Debugger.oncePaused());  // inside D constructor
    frame = callFrames[0];

    InspectorTest.log('private members after super() is called in eval()');
    await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });
    Protocol.Debugger.resume();

    Protocol.Debugger.disable();
  }
]);
