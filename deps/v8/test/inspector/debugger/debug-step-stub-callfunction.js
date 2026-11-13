// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through CodeStub CallFunction.');

const source = `
var print = console.log;

function f() {
  debugger;
  eval('');
  print('Hello, world!');
};

function g() {
  function h() {}
  debugger;
  h();
};

function testCallInExpreesion() {
  function h() {}
  debugger;
  var x = 's' + h(10, 20);
};
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  Protocol.Debugger.onPaused(async message => {
    breakCount++;
    if (message.params.callFrames.length > 1) {
      Protocol.Debugger.stepInto();
    } else {
      Protocol.Debugger.resume();
    }
  });

  breakCount = 0;
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log('f() breakCount: ' + breakCount);

  breakCount = 0;
  await Protocol.Runtime.evaluate({expression: 'g()'});
  InspectorTest.log('g() breakCount: ' + breakCount);

  breakCount = 0;
  await Protocol.Runtime.evaluate({expression: 'testCallInExpreesion()'});
  InspectorTest.log('testCallInExpreesion() breakCount: ' + breakCount);

  InspectorTest.completeTest();
})();
