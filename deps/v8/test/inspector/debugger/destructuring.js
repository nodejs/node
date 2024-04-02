// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests breakable locations in destructuring.');

let source = `
function testFunction() {
  function func() {
    return [1, 2];
  }

  var [a, b] = func();
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
    await session.logBreakLocations(locations);
  },

  async function testSetBreakpoint() {
    const SOURCE_LOCATIONS = [
      {lineNumber: 6, columnNumber: 0},
      {lineNumber: 6, columnNumber: 7},
      {lineNumber: 6, columnNumber: 10},
      {lineNumber: 6, columnNumber: 15},
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
  }
]);
