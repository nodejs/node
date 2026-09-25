// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-jitless

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests breakable locations in for-of loops.');

let source = `
function getIterable() {
  return {
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
}
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

  var iterable = getIterable();
  for (var k of iterable) { all.push(k); }
  iterable.i = 0;
  for (let k of iterable) { all.push(k); }
}
//# sourceURL=test.js`;

function toSourceLine(line) {
  // The source string starts on line 10, so convert test line numbers to source
  // line numbers by subtracting that.
  return line - 10;
}

contextGroup.addScript(source);
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testBreakLocations() {
    Protocol.Debugger.enable();
    let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
    let {result:{locations}} = await Protocol.Debugger.getPossibleBreakpoints({
      start: {lineNumber: 0, columnNumber : 0, scriptId}});
    await session.logBreakLocations(locations);
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

  async function testStepOver() {
    Protocol.Debugger.pause();
    let fin = Protocol.Runtime.evaluate({
      expression: 'testFunction()//# sourceURL=expr.js'}).then(() => false);
    let result;
    while (result = await Promise.race([fin, Protocol.Debugger.oncePaused()])) {
      let { params: { callFrames } } = result;
      if (callFrames.length === 1) {
        Protocol.Debugger.stepInto();
        continue;
      }
      session.logCallFrames(callFrames);
      session.logSourceLocation(callFrames[0].location);
      Protocol.Debugger.stepOver();
    }
    Protocol.Runtime.evaluate({expression: '42'});
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.resume();
  },

  async function testStepIntoAfterBreakpoint() {
    const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: toSourceLine(38), columnNumber: 11, url: 'test.js'
    });
    Protocol.Runtime.evaluate({
      expression: 'testFunction()//# sourceURL=expr.js'});
    await awaitPausedAndDump();
    Protocol.Debugger.stepInto();
    await awaitPausedAndDump();
    await Protocol.Debugger.resume();
    await Protocol.Debugger.removeBreakpoint({breakpointId});

    async function awaitPausedAndDump() {
      let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
      session.logCallFrames(callFrames);
      session.logSourceLocation(callFrames[0].location);
    }
  },

  async function testSetBreakpoint() {
    const SOURCE_LOCATIONS = [
      {lineNumber: toSourceLine(38), columnNumber: 0},
      {lineNumber: toSourceLine(38), columnNumber: 11},
      {lineNumber: toSourceLine(38), columnNumber: 16},
      {lineNumber: toSourceLine(38), columnNumber: 28},
    ];
    for (const {lineNumber, columnNumber} of SOURCE_LOCATIONS) {
      const url = 'test.js';
      InspectorTest.log(`Setting breakpoint at ${url}:${lineNumber}:${columnNumber}`);
      const {result: {breakpointId, locations}} = await Protocol.Debugger.setBreakpointByUrl({
        lineNumber, columnNumber, url
      });
      locations.forEach(location => session.logSourceLocation(location));
      await Protocol.Debugger.removeBreakpoint({breakpointId});
    }
  },

  async function testSetBreakpointWhileInNext() {
    Protocol.Debugger.disable();
    InspectorTest.log(`Forcing sparkplug compilation`);
    await Protocol.Runtime.evaluate({
      expression: `
        %CompileBaseline(testFunction);
        //# sourceURL=force_baseline.js
      `}).then(() => false);
    Protocol.Debugger.enable();

    InspectorTest.log(`Setting breakpoint in next method body`);
    const {result: {breakpointId: breakpointIdInNext}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: toSourceLine(18), columnNumber:0, url:'test.js'
    });
    let fin = Protocol.Runtime.evaluate({
      expression: `
        testFunction();
        //# sourceURL=expr.js
      `}).then(() => false);
    await awaitPausedAndDump();

    InspectorTest.log(`Setting breakpoint in loop body while paused in next method`);
    await Protocol.Debugger.removeBreakpoint({breakpointId: breakpointIdInNext});
    const {result: {breakpointId: breakpointIdInBody}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: toSourceLine(38), columnNumber:28, url:'test.js'
    });
    await Protocol.Debugger.resume();

    let result;
    while (result = await Promise.race([fin, Protocol.Debugger.oncePaused()])) {
      let { params: { callFrames } } = result;
      session.logCallFrames(callFrames);
      session.logSourceLocation(callFrames[0].location);
      await Protocol.Debugger.resume();
    }

    async function awaitPausedAndDump() {
      let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
      session.logCallFrames(callFrames);
      session.logSourceLocation(callFrames[0].location);
    }
  }
]);
