// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks breakpoints.');

session.setupScriptMap();
InspectorTest.runAsyncTestSuite([
  async function testRemoveBreakpoint() {
    InspectorTest.log('Debugger.removeBreakpoint when agent is disabled:');
    InspectorTest.logMessage(await Protocol.Debugger.removeBreakpoint({
      breakpointId: '1:test.js:0:0'
    }));
    Protocol.Debugger.enable();
    InspectorTest.log('Remove breakpoint with invalid breakpoint id:')
    InspectorTest.logMessage(await Protocol.Debugger.removeBreakpoint({
      breakpointId: ''
    }));
    InspectorTest.logMessage(await Protocol.Debugger.removeBreakpoint({
      breakpointId: ':::'
    }));
    await Protocol.Debugger.disable();
  },

  async function testSetBreakpointByUrl() {
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: `
function foo1(arg) {
  return arg;
}
//# sourceURL=testSetBreakpointByUrl.js`});
    InspectorTest.log('Adding conditional (arg === 1) breakpoint');
    let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 2,
      url: 'testSetBreakpointByUrl.js',
      columnNumber: 2,
      condition: 'arg === 1'
    });
    await evaluate('foo1(0)');
    await evaluate('foo1(1)', breakpointId);

    InspectorTest.log('\nEvaluating another script with the same url')
    Protocol.Runtime.evaluate({expression: `
function foo2(arg) {
  return arg;
}
//# sourceURL=testSetBreakpointByUrl.js`});
    await evaluate('foo2(0)');
    await evaluate('foo2(1)', breakpointId);

    InspectorTest.log('\nRemoving breakpoint');
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await evaluate('foo1(1)');
    await evaluate('foo2(1)');

    InspectorTest.log('\nAdding breakpoint back');
    ({result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 2,
      url: 'testSetBreakpointByUrl.js',
      columnNumber: 2,
      condition: 'arg === 1'
    }));
    await evaluate('foo1(0)');
    await evaluate('foo1(1)', breakpointId);

    InspectorTest.log('\nDisabling debugger agent');
    await Protocol.Debugger.disable();
    await evaluate('foo1(1)');
    await evaluate('foo2(1)');

    InspectorTest.log('\nEnabling debugger agent');
    await Protocol.Debugger.enable();
    await evaluate('foo1(1)');
    await evaluate('foo2(1)');
  },

  async function testSetBreakpointInScriptsWithDifferentOffsets() {
    await Protocol.Debugger.enable();
    InspectorTest.log('Adding breakpoint');
    let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 2,
      url: 'test2.js',
      columnNumber: 2,
    });
    contextGroup.addScript(`
function foo1(arg) {
  return arg;
}
//# sourceURL=test2.js`);
    contextGroup.addScript(`
function foo2(arg) {
  return arg;
}
//# sourceURL=test2.js`, 5);
    await evaluate('foo1(0)', breakpointId);
    await evaluate('foo2(0)');
}
]);

async function evaluate(expression, expectedBreakpoint) {
  InspectorTest.log('evaluating ' + expression + ':');
  let paused = Protocol.Debugger.oncePaused();
  let evaluate = Protocol.Runtime.evaluate({expression});
  let result = await Promise.race([paused, evaluate]);
  if (result.method === 'Debugger.paused') {
    if (result.params.hitBreakpoints) {
      if (result.params.hitBreakpoints.find(b => b === expectedBreakpoint)) {
        InspectorTest.log('  hit expected breakpoint')
      } else {
        InspectorTest.log('  hit unexpected breakpoint');
      }
    }
    await Protocol.Debugger.resume();
  } else {
    InspectorTest.log('  not paused');
  }
}
