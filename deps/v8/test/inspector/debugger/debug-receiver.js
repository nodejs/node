// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that debugger correctly reflects receiver in strict and ' +
    'non-strict functions.');

const source = `
function Strict() {
  "use strict";
  debugger;
}

function NonStrict() {
  debugger;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let currentResolve;
  Protocol.Debugger.onPaused(async (msg) => {
    const thisObj = msg.params.callFrames[0].this;
    // Log key properties of callFrames[0].this.
    const properties = {
      type: thisObj.type,
      subtype: thisObj.subtype,
      className: thisObj.className,
      value: thisObj.value,
      description: thisObj.description,
    };
    InspectorTest.log(JSON.stringify(properties));
    await Protocol.Debugger.resume();
    currentResolve();
  });

  async function testCall(expr) {
    InspectorTest.log(`Testing: ${expr}`);
    const paused = new Promise((resolve) => (currentResolve = resolve));
    Protocol.Runtime.evaluate({expression: expr});
    await paused;
  }

  InspectorTest.log('Strict tests:');
  await testCall('Strict.call(undefined)');
  await testCall('Strict.call(null)');
  await testCall('Strict.call(1)');
  await testCall('Strict.call(1.2)');
  await testCall('Strict.call(\'asdf\')');
  await testCall('Strict.call(true)');

  InspectorTest.log('Non-strict tests:');
  await testCall('NonStrict.call(undefined)');
  await testCall('NonStrict.call(null)');
  await testCall('NonStrict.call(1)');
  await testCall('NonStrict.call(1.2)');
  await testCall('NonStrict.call(\'asdf\')');
  await testCall('NonStrict.call(true)');

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
