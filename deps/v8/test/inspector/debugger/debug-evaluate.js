// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests basic frame and global evaluation while paused.');

contextGroup.addScript(
    `
function f() {
  var a = 3;
};

function g() {
  var a = 2;
  f();
  return a;  // Use the value to prevent it being removed by DCE.
};

a = 1;

// String which is longer than 80 chars.
var longString = "1234567890_";
for (var i = 0; i < 4; i++) {
  longString += longString;
}
`,
    0, 0, 'test.js');

async function evalOnFrame(frame, expression) {
  const {result} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: frame.callFrameId,
    expression,
  });
  return result.result.value;
}

async function evalGlobal(expression) {
  const {result} = await Protocol.Runtime.evaluate({expression});
  return result.result.value;
}

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 3,
    columnNumber: 0,
    url: 'test.js',
  });

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    InspectorTest.log(
        'frame(0) "1+2": ' + await evalOnFrame(callFrames[0], '1+2'));
    InspectorTest.log(
        'frame(0) "a+2": ' + await evalOnFrame(callFrames[0], 'a+2'));
    InspectorTest.log(
        'frame(0) "({a:1,b:2}).b+2": ' +
        await evalOnFrame(callFrames[0], '({a:1,b:2}).b+2'));

    InspectorTest.log('frame(0) "a": ' + await evalOnFrame(callFrames[0], 'a'));
    InspectorTest.log('frame(1) "a": ' + await evalOnFrame(callFrames[1], 'a'));
    InspectorTest.log('frame(2) "a": ' + await evalOnFrame(callFrames[2], 'a'));

    InspectorTest.log('global "a": ' + await evalGlobal('a'));
    InspectorTest.log('global "this.a": ' + await evalGlobal('this.a'));
    InspectorTest.log(
        'global "this.longString": ' + await evalGlobal('this.longString'));

    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'g();'});
  InspectorTest.completeTest();
})();
