// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start(
    'Debugger.setInstrumentationBreakpoint');

InspectorTest.runAsyncTestSuite([
  async function testSetTwice() {
    await Protocol.Debugger.enable();
    const { result : firstResult } = await Protocol.Debugger.setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptExecution'
    });
    InspectorTest.log('set breakpoint..');
    InspectorTest.logMessage(firstResult);
    InspectorTest.log('set breakpoint again..');
    InspectorTest.logMessage(await Protocol.Debugger.setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptExecution'
    }));
    InspectorTest.log('remove breakpoint..');
    InspectorTest.logMessage(await Protocol.Debugger.removeBreakpoint({
      breakpointId: firstResult.breakpointId
    }));
    await Protocol.Debugger.disable();
  },

  async function testScriptParsed() {
    await Protocol.Debugger.enable();
    InspectorTest.log('set breakpoint and evaluate script..');
    const { result : firstResult } = await Protocol.Debugger.setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptExecution'
    });
    Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js'});
    {
      const { params: { reason, data } } = await Protocol.Debugger.oncePaused();
      InspectorTest.log(`paused with reason: ${reason}`);
      InspectorTest.logMessage(data);
    }
    await Protocol.Debugger.resume();
    InspectorTest.log('set breakpoint and evaluate script with sourceMappingURL..');
    Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js\n//# sourceMappingURL=map.js'});
    {
      const { params: { reason, data } } = await Protocol.Debugger.oncePaused();
      InspectorTest.log(`paused with reason: ${reason}`);
      InspectorTest.logMessage(data);
    }
    InspectorTest.log('remove breakpoint..');
    InspectorTest.logMessage(await Protocol.Debugger.removeBreakpoint({
      breakpointId: firstResult.breakpointId
    }));
    InspectorTest.log('evaluate script again..');
    await Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js'});
    await Protocol.Debugger.disable();
  },

  async function testScriptWithSourceMapParsed() {
    await Protocol.Debugger.enable();
    InspectorTest.log('set breakpoint for scriptWithSourceMapParsed..');
    const { result : firstResult } = await Protocol.Debugger.setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptWithSourceMapExecution'
    });
    InspectorTest.log('evaluate script without sourceMappingURL..')
    await Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js'});
    InspectorTest.log('evaluate script with sourceMappingURL..')
    Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js\n//# sourceMappingURL=map.js'});
    {
      const { params: { reason, data } } = await Protocol.Debugger.oncePaused();
      InspectorTest.log(`paused with reason: ${reason}`);
      InspectorTest.logMessage(data);
    }
    InspectorTest.log('remove breakpoint..')
    InspectorTest.logMessage(await Protocol.Debugger.removeBreakpoint({
      breakpointId: firstResult.breakpointId
    }));
    InspectorTest.log('evaluate script without sourceMappingURL..')
    await Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js'});
    InspectorTest.log('evaluate script with sourceMappingURL..')
    await Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js\n//# sourceMappingURL=map.js'});
    await Protocol.Debugger.disable();
  },

  async function testBlackboxing() {
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setBlackboxPatterns({patterns: ['foo\.js']});
    InspectorTest.log('set breakpoint and evaluate blackboxed script..');
    const { result : firstResult } = await Protocol.Debugger.setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptExecution'
    });
    await Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js'});
    InspectorTest.log('evaluate not blackboxed script..');
    Protocol.Runtime.evaluate({expression: '//# sourceURL=bar.js'});
    {
      const { params: { reason, data } } = await Protocol.Debugger.oncePaused();
      InspectorTest.log(`paused with reason: ${reason}`);
      InspectorTest.logMessage(data);
    }
    await Protocol.Debugger.resume();
    InspectorTest.log('evaluate blackboxed script that contains not blackboxed one..');
    Protocol.Runtime.evaluate({expression: `eval('//# sourceURL=bar.js')//# sourceURL=foo.js`});
    {
      const { params: { reason, data } } = await Protocol.Debugger.oncePaused();
      InspectorTest.log(`paused with reason: ${reason}`);
      InspectorTest.logMessage(data);
    }
    await Protocol.Debugger.resume();
    await Protocol.Debugger.disable();
  },

  async function testCompileFirstRunLater() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('set breakpoint for scriptWithSourceMapParsed..');
    const { result : firstResult } = await Protocol.Debugger.setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptWithSourceMapExecution'
    });
    InspectorTest.log('compile script with sourceMappingURL..');
    const { result: { scriptId } } = await Protocol.Runtime.compileScript({
      expression: '//# sourceMappingURL=boo.js', sourceURL: 'foo.js', persistScript: true });
    InspectorTest.log('evaluate script without sourceMappingURL..');
    await Protocol.Runtime.evaluate({ expression: '' });
    InspectorTest.log('run previously compiled script with sourceMappingURL..');
    Protocol.Runtime.runScript({ scriptId });
    {
      const { params: { reason, data } } = await Protocol.Debugger.oncePaused();
      InspectorTest.log(`paused with reason: ${reason}`);
      InspectorTest.logMessage(data);
    }
    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  }
]);
