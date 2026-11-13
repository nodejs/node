// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping into callbacks passed to builtin functions.');

const source = `
function cb_false(num) {
  var x = 1;
  return false;
}

function cb_true(num) {
  var x = 1;
  return true;
}

function cb_reduce(a, b) {
  var x = 1;
  return a + b;
}

var a = [1, 2, 3];

function testForEach() {
  debugger;
  a.forEach(cb_true);
  var after = 1;
}

function testSome() {
  debugger;
  a.some(cb_false);
  var after = 1;
}

function testEvery() {
  debugger;
  a.every(cb_true);
  var after = 1;
}

function testMap() {
  debugger;
  a.map(cb_true);
  var after = 1;
}

function testFilter() {
  debugger;
  a.filter(cb_true);
  var after = 1;
}

function testReduce() {
  debugger;
  a.reduce(cb_reduce);
  var after = 1;
}

function testReduceRight() {
  debugger;
  a.reduceRight(cb_reduce);
  var after = 1;
}

function cb_true_2(num) {
  var x = 1;
  return true;
}

function cb_foreach(num) {
  a.forEach(cb_true_2);
  var after = 1;
}

function testNestedForEach() {
  debugger;
  a.forEach(cb_foreach);
  var after = 1;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  const cases = [
    {name: 'testForEach', expectedBreaks: 12, callback: 'cb_true'},
    {name: 'testSome', expectedBreaks: 12, callback: 'cb_false'},
    {name: 'testEvery', expectedBreaks: 12, callback: 'cb_true'},
    {name: 'testMap', expectedBreaks: 12, callback: 'cb_true'},
    {name: 'testFilter', expectedBreaks: 12, callback: 'cb_true'},
    {name: 'testReduce', expectedBreaks: 9, callback: 'cb_reduce'},
    {name: 'testReduceRight', expectedBreaks: 9, callback: 'cb_reduce'},
    {name: 'testNestedForEach', expectedBreaks: 39, callback: 'cb_foreach'},
  ];

  for (const {name, expectedBreaks, callback} of cases) {
    let breaks = 0;
    let callbackHits = 0;

    const onPaused = async ({params: {callFrames}}) => {
      const topFrame = callFrames[0];
      if (topFrame.functionName === callback ||
          topFrame.functionName === 'cb_true_2') {
        callbackHits++;
      }
      breaks++;
      if (breaks < expectedBreaks) {
        await Protocol.Debugger.stepInto();
      } else {
        await Protocol.Debugger.resume();
      }
    };

    Protocol.Debugger.onPaused(onPaused);
    await Protocol.Runtime.evaluate({expression: `${name}();`});
    Protocol.Debugger.onPaused(null);

    InspectorTest.log(
        `${name}: ${breaks} breaks, ${callbackHits} callback pauses`);
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
