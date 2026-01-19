// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start(`Test for Debugger.evaluateOnCallFrame`);

Protocol.Debugger.enable();
InspectorTest.runAsyncTestSuite([
  async function testFoo() {
    contextGroup.addInlineScript(`
      function foo(x) {
        var a;
        y = 0;
        a = x;
        y = 0;
      }
    `, 'foo.js');

    InspectorTest.log('Set breakpoint before a = x.');
    let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 14,
      url: 'foo.js'
    });
    await evaluateOnDump('foo()', ['x', 'a']);
    await evaluateOnDump('foo("Hello, world!")', ['x', 'a']);

    await Protocol.Debugger.removeBreakpoint({
      breakpointId
    });

    InspectorTest.log('Set breakpoint after a = x.');
    await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 16,
      url: 'foo.js'
    });
    await evaluateOnDump('foo("Hello, world!")', ['x', 'a']);
  },

  async function testZoo() {
    contextGroup.addInlineScript(`
      x = undefined;
      function zoo(t) {
        var a = x;
        Object.prototype.x = 42;
        x = t;
        y = 0;
        delete Object.prototype.x;
        x = a;
      }
    `, 'zoo.js');

    InspectorTest.log('Set breakpoint before y = 0.');
    await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 47,
      url: 'zoo.js'
    });
    await evaluateOnDump('zoo("Hello, world!")', ['x', 'a']);
  },

  async function testBar() {
    contextGroup.addInlineScript(`
      y = 0;
      x = 'Goodbye, world!';
      function bar(x, b) {
        var a;
        function barbar() {
          y = 0;
          x = b;
          a = x;
        }
        barbar();
        y = 0;
      }
    `, 'bar.js');

    InspectorTest.log('Set breakpoint before a = x.');
    let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 68,
      url: 'bar.js'
    });
    await evaluateOnDump('bar(undefined, "Hello, world!")', ['x', 'a']);

    await Protocol.Debugger.removeBreakpoint({
      breakpointId
    });
    InspectorTest.log('Set breakpoint after a = x.');
    await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 73,
      url: 'bar.js'
    });
    await evaluateOnDump('bar(undefined, "Hello, world!")', ['x', 'a']);
  }
]);

async function evaluateOnDump(expression, variables) {
  InspectorTest.log(expression);
  Protocol.Runtime.evaluate({
    expression: `${expression}//# sourceURL=expr.js`
  });
  const {params:{callFrames:[{callFrameId}]}} =
      await Protocol.Debugger.oncePaused();
  for (const variable of variables) {
    InspectorTest.log(`${variable} = `);
    const {result:{result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: variable
    });
    InspectorTest.logMessage(result);
  }
  await Protocol.Debugger.resume();
  InspectorTest.log('');
}
