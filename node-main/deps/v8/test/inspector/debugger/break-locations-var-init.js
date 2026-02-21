// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests breakable locations in variable initializations.');

let source = `
function testFunction() {
  var obj1 = {a : 1};
  var arr1 = [1];
  var promise = Promise.resolve(1).then(x => x * 2).then(x => x / 2);
  Promise.resolve(1).then(x => x * 2).then(x => x / 2);
  promise = Promise.resolve(1).then(x => x * 2).then(x => x / 2);
  var a = 1;
  const x = (a = 20);
  var y = (a = 100);
  var z = x + (a = 1) + (a = 2) + (a = 3) + f();
  function f() {
    for (let { x, y } = { x: 0, y: 1 }; y > 0; --y) { let z = x + y; }
  }
  var b = obj1.a;
  (async function asyncF() {
    let r = await Promise.resolve(42);
    return r;
  })();
  return promise;
}
//# sourceURL=test.js`;

contextGroup.addScript(source);
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testBreakLocations() {
    Protocol.Debugger.enable();
    let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
    let {result:{locations}} = await Protocol.Debugger.getPossibleBreakpoints({
      start: {lineNumber: 0, columnNumber : 0, scriptId}});
    session.logBreakLocations(locations);
  },

  async function testStepInto() {
    Protocol.Debugger.pause();
    let fin = Protocol.Runtime.evaluate({
      expression: 'testFunction()//# sourceURL=expr.js', awaitPromise: true}).then(() => false);
    let result;
    while (result = await Promise.race([fin, Protocol.Debugger.oncePaused()])) {
      let {params:{callFrames}} = result;
      session.logCallFrames(callFrames);
      session.logSourceLocation(callFrames[0].location);
      Protocol.Debugger.stepInto();
    }
    Protocol.Runtime.evaluate({expression: '42'});
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.resume();
  },

  async function testStepIntoAfterBreakpoint() {
    Protocol.Debugger.setBreakpointByUrl({lineNumber: 10, url: 'test.js'});
    Protocol.Runtime.evaluate({
      expression: 'testFunction()//# sourceURL=expr.js'});
    await awaitPausedAndDump();
    Protocol.Debugger.stepInto();
    await awaitPausedAndDump();
    await Protocol.Debugger.resume();

    async function awaitPausedAndDump() {
      let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
      session.logCallFrames(callFrames);
      session.logSourceLocation(callFrames[0].location);
    }
  }
]);
