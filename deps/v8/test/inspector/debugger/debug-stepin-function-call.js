// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into function call.');

session.setupScriptMap();

contextGroup.addScript(
    `
function g(t) {
  return t + 1; // expected line
}

function call1() {
  debugger;
  g.call(null, 3);
}

function call2() {
  var aLocalVar = 'test';
  var anotherLocalVar = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
  debugger;
  g.call(null, 3);
}

function call3() {
  var alias = g;
  debugger;
  var r = 10 + alias.call(null, 3);
  var aLocalVar = 'test';
  var anotherLocalVar = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

function call4() {
  var alias = g;
  debugger;
  alias.call(null, 3);
  var aLocalVar = 'test';
  var anotherLocalVar = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

function apply1() {
  debugger;
  g.apply(null, [3]);
}

function apply2() {
  var aLocalVar = 'test';
  var anotherLocalVar = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
  debugger;
  g.apply(null, [3, 4]);
}

function apply3() {
  var alias = g;
  debugger;
  var r = 10 + alias.apply(null, [3, 'unused arg']);
  var aLocalVar = 'test';
  var anotherLocalVar = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

function apply4() {
  var alias = g;
  debugger;
  alias.apply(null, [3]);
  var aLocalVar = 'test';
  var anotherLocalVar = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

function bind1() {
  var bound = g.bind(null, 3);
  debugger;
  bound();
}

function applyAndBind1() {
  var bound = g.bind(null, 3);
  debugger;
  bound.apply(null, [3]);
  var aLocalVar = 'test';
  var anotherLocalVar = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

var globalBound = g.bind(null, 3);
function runGlobalBound() {
  debugger;
  globalBound();
}
`,
    0, 0, 'test.js');

(async function() {
  await Protocol.Debugger.enable();

  let state = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (state < 2) {
      state++;
      await Protocol.Debugger.stepInto();
    } else {
      InspectorTest.log(
          `Paused in ${callFrames[0].functionName} (state: ${state}):`);
      await session.logSourceLocation(callFrames[0].location);
      state = 3;
      await Protocol.Debugger.resume();
    }
  });

  const testFunctions = [
    'call1', 'call2', 'call3', 'call4', 'apply1', 'apply2', 'apply3', 'apply4',
    'bind1', 'applyAndBind1', 'runGlobalBound'
  ];

  for (const fn of testFunctions) {
    state = 0;
    InspectorTest.log(`Testing ${fn}...`);
    await Protocol.Runtime.evaluate({expression: `${fn}()`});
    InspectorTest.log(`state: ${state}`);
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
