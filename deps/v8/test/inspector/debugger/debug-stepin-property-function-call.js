// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noturbofan

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into property function calls.');

session.setupScriptMap();

contextGroup.addScript(
    `
var count = 0;
function getTestFunctions() {
  var result = [];
  for (var n in this) {
    if (n.substr(0, 4) == 'test') result.push(n);
  }
  return result;
}
var obj = {
  fun: function() {
    ++count;
    return count; // Expected to step
  }
};
obj.fun2 = obj.fun;

function testCall_Dots() {
  debugger;
  obj.fun();
}

function testCall_Quotes() {
  debugger;
  obj["fun"]();
}

function testCall_Call() {
  debugger;
  obj.fun.call(obj);
}

function testCall_Apply() {
  debugger;
  obj.fun.apply(obj);
}

function testCall_Variable() {
  var functionName = "fun";
  debugger;
  obj[functionName]();
}

function testCall_Fun2() {
  debugger;
  obj.fun2();
}

function testCall_InternStrings() {
  var cache = { "fun": "fun" };
  var functionName = "fu" + "n";
  debugger;
  obj[cache[functionName]]();
}

function testCall_ViaFunRef() {
  var functionName = "fu" + "n";
  var funRef = obj[functionName];
  debugger;
  funRef();
}

function testCall_RuntimeVariable1() {
  var functionName = "fu" + "n";
  debugger;
  obj[functionName]();
}

function testCall_RuntimeVariable2() {
  var functionName = "un".replace(/u/, "fu");
  debugger;
  obj[functionName]();
}

function testCall_RuntimeVariable3() {
  var expr = "fu" + "n";
  const functionName = expr;
  debugger;
  obj[functionName]();
}
`,
    0, 0, 'test.js');

(async function() {
  await Protocol.Debugger.enable();

  let state = 1;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (state < 4) {
      state++;
      await Protocol.Debugger.stepInto();
    } else {
      InspectorTest.log(
          `Paused in ${callFrames[0].functionName} (state: ${state}):`);
      await session.logSourceLocation(callFrames[0].location);
      state = 5;
      await Protocol.Debugger.resume();
    }
  });

  const testFunctions = [
    'testCall_Dots',
    'testCall_Quotes',
    'testCall_Call',
    'testCall_Apply',
    'testCall_Variable',
    'testCall_Fun2',
    'testCall_InternStrings',
    'testCall_ViaFunRef',
    'testCall_RuntimeVariable1',
    'testCall_RuntimeVariable2',
    'testCall_RuntimeVariable3',
  ];

  await Protocol.Runtime.evaluate({expression: 'getTestFunctions()'});
  let functionsCalled = 0;
  for (const fn of testFunctions) {
    state = 1;
    InspectorTest.log(`Running ${fn}...`);
    await Protocol.Runtime.evaluate({expression: `${fn}()`});
    functionsCalled++;
    InspectorTest.log(`state: ${state}`);
  }

  InspectorTest.log(`functionsCalled: ${functionsCalled}`);

  let {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'count'});
  InspectorTest.log(`count: ${result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
