// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests breakable locations in await expression.');

let source = `
function testFunction() {
  async function f1() {
    for (let x = 0; x < 1; ++x) await x;
    return await Promise.resolve(2);
  }

  async function f2() {
    let r = await f1() + await f1();
    await f1();
    await f1().then(x => x * 2);
    await [1].map(x => Promise.resolve(x))[0];
    await Promise.resolve().then(x => x * 2);
    let p = Promise.resolve(42);
    await p;
    return r;
  }

  return f2();
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
    dumpAllLocations(locations);
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

  async function testStepOver() {
    Protocol.Debugger.pause();
    let fin = Protocol.Runtime.evaluate({
      expression: 'testFunction()//# sourceURL=expr.js', awaitPromise: true}).then(() => false);
    Protocol.Debugger.stepInto();
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepInto();
    await Protocol.Debugger.oncePaused();

    let result;
    while (result = await Promise.race([fin, Protocol.Debugger.oncePaused()])) {
      let {params:{callFrames}} = result;
      session.logCallFrames(callFrames);
      session.logSourceLocation(callFrames[0].location);
      Protocol.Debugger.stepOver();
    }
    Protocol.Runtime.evaluate({expression: '42'});
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.resume();
  },

  async function testStepIntoAfterBreakpoint() {
    Protocol.Debugger.setBreakpointByUrl({lineNumber: 9, url: 'test.js'});
    Protocol.Runtime.evaluate({
      expression: 'testFunction()//# sourceURL=expr.js'});
    await awaitPausedAndDump();
    Protocol.Debugger.stepInto();
    await awaitPausedAndDump();
    Protocol.Debugger.stepInto();
    await awaitPausedAndDump();
    Protocol.Debugger.stepInto();
    await awaitPausedAndDump();
    Protocol.Debugger.stepInto();
    await awaitPausedAndDump();
    Protocol.Debugger.stepInto();
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

function dumpAllLocations(locations) {
  var lines = source.split('\n');
  var locations = locations.sort((loc1, loc2) => {
    if (loc2.lineNumber !== loc1.lineNumber) return loc2.lineNumber - loc1.lineNumber;
    return loc2.columnNumber - loc1.columnNumber;
  });
  for (var location of locations) {
    var line = lines[location.lineNumber];
    line = line.slice(0, location.columnNumber) + locationMark(location.type) + line.slice(location.columnNumber);
    lines[location.lineNumber] = line;
  }
  lines = lines.filter(line => line.indexOf('//# sourceURL=') === -1);
  InspectorTest.log(lines.join('\n') + '\n');
}

function locationMark(type) {
  if (type === 'return') return '|R|';
  if (type === 'call') return '|C|';
  if (type === 'debuggerStatement') return '|D|';
  return '|_|';
}
