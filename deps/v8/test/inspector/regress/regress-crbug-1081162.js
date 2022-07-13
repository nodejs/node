// Copyright 2021 the V8 project authors. All rights reserved.
// Use  of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Regression test for crbug/1081162');

const source = `
async function foo(o) {
  await o.bar();
}
`;
const url = 'v8://test/foo.js';

session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testBreakpointResolved() {
    await Protocol.Debugger.enable();
    contextGroup.addScript(source, 0, 0, url);
    await Protocol.Debugger.onceScriptParsed();
    let {result: {locations: [location]}} = await Protocol.Debugger.setBreakpointByUrl({url, lineNumber: 2, columnNumber: 0});
    InspectorTest.log('After Debugger.setBreakpointByUrl');
    await session.logSourceLocation(location);
    contextGroup.addScript(source, 0, 0, url);
    ({params: {location}} = await Protocol.Debugger.onceBreakpointResolved());
    InspectorTest.log('After Debugger.breakpointResolved');
    await session.logSourceLocation(location);
    await Protocol.Debugger.disable();
  }
]);
