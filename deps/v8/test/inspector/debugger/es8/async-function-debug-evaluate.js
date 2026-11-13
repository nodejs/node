// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug evaluation in async functions.');

const source = `
function assertEquals(expected, actual, msg) {
  if (expected !== actual) {
    throw new Error('Expected ' + expected + ', got ' + actual + (msg ? ': ' + msg : ''));
  }
}

var outer = "outer";
const constOuter = "const outer";
let letOuter = "let outer";

async function thrower() {
  return Promise.reject(new Error("oop"));
}

async function testLater() {
  return { x: 57, y: 100 };
}

async function test() {
  var inner = "inner";
  debugger;

  let withVar = await testLater();
  with (withVar) {
    debugger;
  }

  assertEquals("x later(57)", withVar.x);
  assertEquals("y later(100)", withVar.y);
  assertEquals(undefined, withVar.z);
  assertEquals("ZEE", z);

  let letInner = "let inner";
  const constInner = "const inner";
  debugger;

  try {
    await thrower();
  } catch (error) {
    const bun = "floof";
    debugger;
    let cow = "moo";
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

async function evalOnFrame(frame, expression) {
  const {result} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: frame.callFrameId,
    expression,
  });
  if (result.exceptionDetails) {
    return 'throws ' + result.result.className;
  }
  return result.result.value;
}

(async function test() {
  await Protocol.Debugger.enable();

  let breakPointCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    breakPointCount++;
    InspectorTest.log(`-- Breakpoint ${breakPointCount} --`);
    if (breakPointCount === 1) {
      InspectorTest.log(
          'frame(0) inner: ' + await evalOnFrame(callFrames[0], 'inner'));
      InspectorTest.log(
          'frame(0) letInner: ' + await evalOnFrame(callFrames[0], 'letInner'));
      InspectorTest.log(
          'frame(0) constInner: ' +
          await evalOnFrame(callFrames[0], 'constInner'));
      InspectorTest.log(
          'frame(0) outer: ' + await evalOnFrame(callFrames[0], 'outer'));
      InspectorTest.log(
          'frame(0) constOuter: ' +
          await evalOnFrame(callFrames[0], 'constOuter'));
      InspectorTest.log(
          'frame(0) letOuter: ' + await evalOnFrame(callFrames[0], 'letOuter'));

      InspectorTest.log(
          'frame(1) outer: ' + await evalOnFrame(callFrames[1], 'outer'));
      InspectorTest.log(
          'frame(1) constOuter: ' +
          await evalOnFrame(callFrames[1], 'constOuter'));
      InspectorTest.log(
          'frame(1) letOuter: ' + await evalOnFrame(callFrames[1], 'letOuter'));

      InspectorTest.log(
          'frame(0) withVar: ' + await evalOnFrame(callFrames[0], 'withVar'));
    } else if (breakPointCount === 2) {
      InspectorTest.log(
          'frame(0) inner: ' + await evalOnFrame(callFrames[0], 'inner'));
      InspectorTest.log(
          'frame(0) letInner: ' + await evalOnFrame(callFrames[0], 'letInner'));
      InspectorTest.log(
          'frame(0) constInner: ' +
          await evalOnFrame(callFrames[0], 'constInner'));
      InspectorTest.log('frame(0) x: ' + await evalOnFrame(callFrames[0], 'x'));
      InspectorTest.log('frame(0) y: ' + await evalOnFrame(callFrames[0], 'y'));

      InspectorTest.log(
          'frame(0) outer: ' + await evalOnFrame(callFrames[0], 'outer'));
      InspectorTest.log(
          'frame(0) constOuter: ' +
          await evalOnFrame(callFrames[0], 'constOuter'));
      InspectorTest.log(
          'frame(0) letOuter: ' + await evalOnFrame(callFrames[0], 'letOuter'));

      await evalOnFrame(callFrames[0], 'x = `x later(${x})`');
      await evalOnFrame(callFrames[0], 'y = `y later(${y})`');
      await evalOnFrame(callFrames[0], 'z = `ZEE`');
    } else if (breakPointCount === 3) {
      InspectorTest.log(
          'frame(0) inner: ' + await evalOnFrame(callFrames[0], 'inner'));
      InspectorTest.log(
          'frame(0) letInner: ' + await evalOnFrame(callFrames[0], 'letInner'));
      InspectorTest.log(
          'frame(0) constInner: ' +
          await evalOnFrame(callFrames[0], 'constInner'));
    } else if (breakPointCount === 4) {
      InspectorTest.log(
          'frame(0) error.message: ' +
          await evalOnFrame(callFrames[0], 'error.message'));
      InspectorTest.log(
          'frame(0) error.constructor.name: ' +
          await evalOnFrame(callFrames[0], 'error.constructor.name'));
      InspectorTest.log(
          'frame(0) bun: ' + await evalOnFrame(callFrames[0], 'bun'));
      InspectorTest.log(
          'frame(0) cow: ' + await evalOnFrame(callFrames[0], 'cow'));
      InspectorTest.log(
          'frame(0) outer: ' + await evalOnFrame(callFrames[0], 'outer'));
      InspectorTest.log(
          'frame(0) constOuter: ' +
          await evalOnFrame(callFrames[0], 'constOuter'));
      InspectorTest.log(
          'frame(0) letOuter: ' + await evalOnFrame(callFrames[0], 'letOuter'));
    }
    await Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate({
    expression: 'test()',
    awaitPromise: true,
  });
  if (result.exceptionDetails) {
    InspectorTest.log(
        'test() threw: ' + JSON.stringify(result.exceptionDetails));
  } else {
    InspectorTest.log('test() completed successfully');
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
