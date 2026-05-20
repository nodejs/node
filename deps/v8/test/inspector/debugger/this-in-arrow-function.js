// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Checks this in arrow function scope');

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: `
function foo() {
  return () => {
    let a = this;
    (function() {
      let f = () => { debugger; };
      f();
    }).call('a');
    return a;
  };
}
function boo() {
  foo.call(1)();
}
(() => boo.call({}))();`
  });
  let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  for (let callFrame of callFrames) {
    await session.logSourceLocation(callFrame.location);

    InspectorTest.log('This on callFrame:');
    InspectorTest.logMessage(callFrame.this);
    let {result:{result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrame.callFrameId,
      expression: 'this'
    });
    InspectorTest.log('This in evaluateOnCallFrame:');
    InspectorTest.logMessage(result);

    if (callFrame.this.type === 'undefined' || result.type === 'undefined') {
      InspectorTest.log('Values equal: ' + (callFrame.this.type === result.type) + '\n');
      continue;
    }

    let {result:{result:{value}}} = await Protocol.Runtime.callFunctionOn({
      functionDeclaration: 'function equal(a) { return this === a; }',
      objectId: callFrame.this.objectId,
      arguments: [ result.value ? {value: result.value} : {objectId: result.objectId}],
      returnByValue: true
    });
    InspectorTest.log('Values equal: ' + value + '\n');
  }
  InspectorTest.completeTest();
})();
