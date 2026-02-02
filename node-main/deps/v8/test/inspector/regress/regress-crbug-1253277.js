// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol, session} = InspectorTest.start('Regression test for crbug.com/1253277');

const url = 'foo.js';
contextGroup.addScript('function foo(){}foo()', 0, 0, url);
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Promise.all([Protocol.Runtime.enable(), Protocol.Debugger.enable()]);
    const {result: {breakpointId, locations}} = await Protocol.Debugger.setBreakpointByUrl({
      columnNumber: 16,
      lineNumber: 0,
      url,
    });
    await session.logBreakLocations(locations);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Promise.all([Protocol.Runtime.disable(), Protocol.Debugger.disable()]);
  }
]);
