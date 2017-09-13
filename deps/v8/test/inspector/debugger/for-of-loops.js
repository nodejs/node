// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests breakable locations in for-of loops.');

let source = `
function testFunction() {
  var obj = {a : 1};
  var arr = [1];
  var all = [];
  for (var k in arr) { all.push(k); }
  for (var k of arr) { all.push(k); }
  for (var k in obj) { all.push(k); }
  for (let k in arr) { all.push(k); }
  for (let k of arr) { all.push(k); }
  for (let k in obj) { all.push(k); }

  var iterable = {
    [Symbol.iterator]() {
      return {
        i: 0,
        next() {
          if (this.i < 1) {
            return { value: this.i++, done: false };
          }
          return { value: undefined, done: true };
        }
      };
    }
  };
  for (var k of iterable) { all.push(k); }
  iterable.i = 0;
  for (let k of iterable) { all.push(k); }
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
      expression: 'testFunction()//# sourceURL=expr.js'}).then(() => false);
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
    Protocol.Debugger.setBreakpointByUrl({lineNumber: 25, url: 'test.js'});
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
