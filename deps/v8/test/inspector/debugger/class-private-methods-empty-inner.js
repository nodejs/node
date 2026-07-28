// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/private-class-member-inspector-test.js');

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

  await printPrivateMembers(Protocol, InspectorTest, { objectId: frame.this.objectId });

  Protocol.Debugger.resume();
  Protocol.Debugger.disable();
}]);
