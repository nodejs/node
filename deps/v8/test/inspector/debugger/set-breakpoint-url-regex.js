// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { contextGroup, Protocol } = InspectorTest.start(
  `Tests for calling setBreakpoint with urlRegex`);

(async function test(){
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: `
function test() {
  return 42;
}
//# sourceURL=some-kind-of-test.js`});
  await Protocol.Debugger.onceScriptParsed();

  InspectorTest.runAsyncTestSuite([
    async function testSetBreakpointByUrlRegex() {
      const result = await Protocol.Debugger.setBreakpointByUrl({ lineNumber: 2, urlRegex: '.*of-test.js' });
      InspectorTest.logMessage(result.result.locations);
      await expectBreakInEval('test()');
      await Protocol.Debugger.removeBreakpoint({ breakpointId: result.result.breakpointId });
      await expectNoBreakInEval('test()');
    },
    async function testSetBreakpointByUrlWithConditions() {
      // Test Condition false
      let result = await Protocol.Debugger.setBreakpointByUrl({ lineNumber: 2, urlRegex: '.*of-test.js', condition: 'false' });
      InspectorTest.logMessage(result.result.locations);
      await expectNoBreakInEval('test()');
      await Protocol.Debugger.removeBreakpoint({ breakpointId: result.result.breakpointId });

      // Test condition true
      result = await Protocol.Debugger.setBreakpointByUrl({ lineNumber: 2, urlRegex: '.*of-test.js', condition: 'true' });
      InspectorTest.logMessage(result.result.locations);
      await expectBreakInEval('test()');
      await Protocol.Debugger.removeBreakpoint({ breakpointId: result.result.breakpointId });
    },
  ]);

  // Function that will evaluate an expression and return once completed.
  // Used to validate that breakpoint is not hit within the evaluated expression.
  async function expectNoBreakInEval(expression) {
    await Protocol.Runtime.evaluate({expression});
    InspectorTest.log(`Successfully completed eval of: '${expression}'`);
  }

  // Function that will evaluate an expression and return once a paused event is received
  // and the debugger is resumed.
  // Used to validate that breakpoint is hit within the evaluated expression.
  async function expectBreakInEval(expression) {
    Protocol.Runtime.evaluate({expression});
    await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Successfully paused during eval of: '${expression}'`);
    await Protocol.Debugger.resume();
  }

})();
