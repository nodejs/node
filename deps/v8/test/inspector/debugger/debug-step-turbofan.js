// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-filter=g --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping with TurboFan code on stack.');

const source = `
function f(x) {
  g(x);
  var a = 0;              // Break 6
  return a;               // Break 7
}                         // Break 8

function g(x) {
  if (x) h();
  var a = 0;              // Break 2
  var b = 1;              // Break 3
  return a + b;           // Break 4
}                         // Break 5

function h() {
  debugger;               // Break 0
}                         // Break 1

%PrepareFunctionForOptimization(g);
f(0);
f(0);
%OptimizeFunctionOnNextCall(g);
`;

contextGroup.addScript(source, 0, 0, 'test.js');

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  Protocol.Debugger.onPaused(async message => {
    if (message.params.callFrames.length > 1) {
      breakCount++;
      let loc = message.params.callFrames[0].location;
      let line = source.split('\n')[loc.lineNumber];
      InspectorTest.log(line);
      Protocol.Debugger.stepOver();
    } else {
      Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'f(1);'});
  InspectorTest.log('Total breaks: ' + breakCount);
  InspectorTest.completeTest();
})();
