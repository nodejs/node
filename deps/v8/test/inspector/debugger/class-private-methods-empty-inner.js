// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test empty inner classes with private instance methods in the outer class');

contextGroup.addScript(`
function run() {
  class Outer {
    #method() {}

    factory() {
      return class Inner {
        fn() {
          debugger;
        }
      };
    }
  };

  const a = new Outer();
  const Inner = a.factory();
  (new Inner).fn();
}`);

InspectorTest.runAsyncTestSuite([async function testScopesPaused() {
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'run()'});

  let {params: {callFrames}} =
      await Protocol.Debugger.oncePaused();  // inside fn()
  let frame = callFrames[0];

  let {result} =
      await Protocol.Runtime.getProperties({objectId: frame.this.objectId});

  InspectorTest.logObject(result.privateProperties);

  Protocol.Debugger.resume();
  Protocol.Debugger.disable();
}]);
