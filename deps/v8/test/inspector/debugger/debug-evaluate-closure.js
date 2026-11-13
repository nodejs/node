// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Debugger.evaluateOnCallFrame with closures and with-scopes.');

contextGroup.addScript(
    `
var outer = 'outer';
var baz = 'baz outer';

function foo() {
  var inner = 'inner';
  var baz = 'baz inner';
  var goo = 'goo';
  var withw = { w: 'w' };
  var withv = { v: 'v' };

  with (withv) {
    var bar = function bar() {
      if (goo !== 'goo foo') throw new Error('Expected goo foo, got ' + goo);
      inner = 'inner bar';
      outer = 'outer bar';
      v = 'v bar';
      return 'bar return';
    };
  }

  with (withw) {
    debugger;
  }

  if (inner !== 'inner foo') {
    throw new Error('Expected inner foo, got ' + inner);
  }
  if (baz !== 'baz inner foo') {
    throw new Error('Expected baz inner foo, got ' + baz);
  }
  if (withw.w !== 'w foo') throw new Error('Expected w foo, got ' + withw.w);
  if (withv.v !== 'v bar') throw new Error('Expected v bar, got ' + withv.v);
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  async function evalOnFrame(frame, expression, scopeNumber) {
    const params = {callFrameId: frame.callFrameId, expression};
    if (scopeNumber !== undefined) params.scopeNumber = scopeNumber;
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(params);
    if (result.exceptionDetails) {
      return `Exception: ${result.result.className}`;
    }
    return result.result.value;
  }

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    InspectorTest.log(
        `frame(0) goo: ${await evalOnFrame(callFrames[0], 'goo')}`);
    InspectorTest.log(
        `frame(0).scope(1) goo: ${await evalOnFrame(callFrames[0], 'goo', 1)}`);
    InspectorTest.log(
        `frame(0).scope(1) w: ${await evalOnFrame(callFrames[0], 'w', 1)}`);
    await evalOnFrame(callFrames[0], 'goo = \'goo foo\'');
    InspectorTest.log(
        `frame(0) bar(): ${await evalOnFrame(callFrames[0], 'bar()')}`);
    InspectorTest.log(
        `frame(0) inner: ${await evalOnFrame(callFrames[0], 'inner')}`);
    InspectorTest.log(
        `frame(0) outer: ${await evalOnFrame(callFrames[0], 'outer')}`);

    InspectorTest.log(
        `frame(0) baz: ${await evalOnFrame(callFrames[0], 'baz')}`);
    InspectorTest.log(
        `frame(1) baz: ${await evalOnFrame(callFrames[1], 'baz')}`);
    await evalOnFrame(callFrames[0], 'w = \'w foo\'');
    await evalOnFrame(callFrames[0], 'inner = \'inner foo\'');
    await evalOnFrame(callFrames[0], 'outer = \'outer foo\'');
    await evalOnFrame(callFrames[0], 'baz = \'baz inner foo\'');
    await evalOnFrame(callFrames[1], 'baz = \'baz outer foo\'');
    await Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate(
      {expression: 'foo(); [outer, baz].join(", ")'});
  InspectorTest.log(`After foo(): ${result.result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
