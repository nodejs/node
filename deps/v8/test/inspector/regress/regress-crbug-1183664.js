// Copyright 2021 the V8 project authors. All rights reserved.
// Use  of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Regression test for crbug.com/1183664');

const url = 'test://foo.js';
const lineNumber = 0;

const columnNumber1 = 1;
contextGroup.addScript(`console.log("FIRST")`, lineNumber, columnNumber1, url);
const columnNumber2 = 65;
contextGroup.addScript(`console.log("SECOND")`, lineNumber, columnNumber2, url);

InspectorTest.runAsyncTestSuite([
  async function testMultipleScriptsInSameLineWithSameURL() {
    await Protocol.Debugger.enable();
    InspectorTest.logMessage('Setting breakpoint in first script')
    {
      const {result: {breakpointId, locations}} = await Protocol.Debugger.setBreakpointByUrl({
        url,
        lineNumber,
        columnNumber: columnNumber1,
      });
      InspectorTest.logMessage(locations);
      await Protocol.Debugger.removeBreakpoint({breakpointId});
    }
    InspectorTest.logMessage('Setting breakpoint in second script')
    {
      const {result: {breakpointId, locations}} = await Protocol.Debugger.setBreakpointByUrl({
        url,
        lineNumber,
        columnNumber: columnNumber2,
      });
      InspectorTest.logMessage(locations);
      await Protocol.Debugger.removeBreakpoint({breakpointId});
    }
  }
]);
